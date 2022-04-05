/*
 * Copyright (C) 2007 Advanced Micro Devices, Inc.
 * Author: Leo Duran <leo.duran@amd.com>
 * Author: Wei Wang <wei.wang2@amd.com> - adapted to xen
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; If not, see <http://www.gnu.org/licenses/>.
 */

#include <xen/sched.h>
#include <xen/iocap.h>
#include <xen/pci.h>
#include <xen/pci_regs.h>
#include <xen/paging.h>
#include <xen/softirq.h>
#include <asm/amd-iommu.h>
#include <asm/hvm/svm/amd-iommu-proto.h>
#include "../ats.h"

static bool_t __read_mostly init_done;

static const struct iommu_ops amd_iommu_ops;

struct amd_iommu *find_iommu_for_device(int seg, int bdf)
{
    struct ivrs_mappings *ivrs_mappings = get_ivrs_mappings(seg);

    if ( !ivrs_mappings || bdf >= ivrs_bdf_entries )
        return NULL;

    if ( unlikely(!ivrs_mappings[bdf].iommu) && likely(init_done) )
    {
        unsigned int bd0 = bdf & ~PCI_FUNC(~0);

        if ( ivrs_mappings[bd0].iommu )
        {
            struct ivrs_mappings tmp = ivrs_mappings[bd0];

            tmp.iommu = NULL;
            if ( tmp.dte_requestor_id == bd0 )
                tmp.dte_requestor_id = bdf;
            ivrs_mappings[bdf] = tmp;

            printk(XENLOG_WARNING "%04x:%02x:%02x.%u not found in ACPI tables;"
                   " using same IOMMU as function 0\n",
                   seg, PCI_BUS(bdf), PCI_SLOT(bdf), PCI_FUNC(bdf));

            /* write iommu field last */
            ivrs_mappings[bdf].iommu = ivrs_mappings[bd0].iommu;
        }
    }

    return ivrs_mappings[bdf].iommu;
}

/*
 * Some devices will use alias id and original device id to index interrupt
 * table and I/O page table respectively. Such devices will have
 * both alias entry and select entry in IVRS structure.
 *
 * Return original device id, if device has valid interrupt remapping
 * table setup for both select entry and alias entry.
 */
int get_dma_requestor_id(u16 seg, u16 bdf)
{
    struct ivrs_mappings *ivrs_mappings = get_ivrs_mappings(seg);
    int req_id;

    BUG_ON ( bdf >= ivrs_bdf_entries );
    req_id = ivrs_mappings[bdf].dte_requestor_id;
    if ( (ivrs_mappings[bdf].intremap_table != NULL) &&
         (ivrs_mappings[req_id].intremap_table != NULL) )
        req_id = bdf;

    return req_id;
}

static int is_translation_valid(u32 *entry)
{
    return (get_field_from_reg_u32(entry[0],
                                   IOMMU_DEV_TABLE_VALID_MASK,
                                   IOMMU_DEV_TABLE_VALID_SHIFT) &&
            get_field_from_reg_u32(entry[0],
                                   IOMMU_DEV_TABLE_TRANSLATION_VALID_MASK,
                                   IOMMU_DEV_TABLE_TRANSLATION_VALID_SHIFT));
}

static void disable_translation(u32 *dte)
{
    u32 entry;

    entry = dte[0];
    set_field_in_reg_u32(IOMMU_CONTROL_DISABLED, entry,
                         IOMMU_DEV_TABLE_TRANSLATION_VALID_MASK,
                         IOMMU_DEV_TABLE_TRANSLATION_VALID_SHIFT, &entry);
    set_field_in_reg_u32(IOMMU_CONTROL_DISABLED, entry,
                         IOMMU_DEV_TABLE_VALID_MASK,
                         IOMMU_DEV_TABLE_VALID_SHIFT, &entry);
    dte[0] = entry;
}

static int __must_check allocate_domain_resources(struct domain_iommu *hd)
{
    int rc;

    spin_lock(&hd->arch.mapping_lock);
    rc = amd_iommu_alloc_root(hd);
    spin_unlock(&hd->arch.mapping_lock);

    return rc;
}

static bool any_pdev_behind_iommu(const struct domain *d,
                                  const struct pci_dev *exclude,
                                  const struct amd_iommu *iommu)
{
    const struct pci_dev *pdev;

    for_each_pdev ( d, pdev )
    {
        if ( pdev == exclude )
            continue;

        if ( find_iommu_for_device(pdev->seg,
                                   PCI_BDF2(pdev->bus, pdev->devfn)) == iommu )
            return true;
    }

    return false;
}

static int __must_check amd_iommu_setup_domain_device(
    struct domain *domain, struct amd_iommu *iommu,
    u8 devfn, struct pci_dev *pdev)
{
    uint32_t *dte;
    unsigned long flags;
    unsigned int req_id, sr_flags;
    int dte_i = 0, rc;
    u8 bus = pdev->bus;
    struct domain_iommu *hd = dom_iommu(domain);
    const struct ivrs_mappings *ivrs_dev;

    BUG_ON(!hd->arch.paging_mode || !iommu->dev_table.buffer);

    rc = allocate_domain_resources(hd);
    if ( rc )
        return rc;

    req_id = get_dma_requestor_id(iommu->seg,
                                  PCI_BDF2(pdev->bus, pdev->devfn));
    ivrs_dev = &get_ivrs_mappings(iommu->seg)[req_id];
    sr_flags = (iommu_hwdom_passthrough && is_hardware_domain(domain)
                ? 0 : SET_ROOT_VALID)
               | (ivrs_dev->unity_map ? SET_ROOT_WITH_UNITY_MAP : 0);

    if ( ats_enabled )
        dte_i = 1;

    /* get device-table entry */
    req_id = get_dma_requestor_id(iommu->seg, PCI_BDF2(bus, devfn));
    dte = iommu->dev_table.buffer + (req_id * IOMMU_DEV_TABLE_ENTRY_SIZE);
    ivrs_dev = &get_ivrs_mappings(iommu->seg)[req_id];

    spin_lock_irqsave(&iommu->lock, flags);

    if ( !is_translation_valid((u32 *)dte) )
    {
        /* bind DTE to domain page-tables */
        rc = amd_iommu_set_root_page_table(
                 dte, page_to_maddr(hd->arch.root_table),
                 domain->domain_id, hd->arch.paging_mode, sr_flags);
        if ( rc )
        {
            ASSERT(rc < 0);
            spin_unlock_irqrestore(&iommu->lock, flags);
            return rc;
        }

        if ( pci_ats_device(iommu->seg, bus, pdev->devfn) &&
             iommu_has_cap(iommu, PCI_CAP_IOTLB_SHIFT) )
            iommu_dte_set_iotlb((u32 *)dte, dte_i);

        amd_iommu_flush_device(iommu, req_id);
    }
    else if ( amd_iommu_get_root_page_table(dte) !=
              page_to_maddr(hd->arch.root_table) )
    {
        /*
         * Strictly speaking if the device is the only one with this requestor
         * ID, it could be allowed to be re-assigned regardless of unity map
         * presence.  But let's deal with that case only if it is actually
         * found in the wild.
         */
        if ( req_id != PCI_BDF2(bus, devfn) &&
             (sr_flags & SET_ROOT_WITH_UNITY_MAP) )
            rc = -EOPNOTSUPP;
        else
            rc = amd_iommu_set_root_page_table(
                     dte, page_to_maddr(hd->arch.root_table),
                     domain->domain_id, hd->arch.paging_mode, sr_flags);
        if ( rc < 0 )
        {
            spin_unlock_irqrestore(&iommu->lock, flags);
            return rc;
        }
        if ( rc &&
             domain != pdev->domain &&
             /*
              * By non-atomically updating the DTE's domain ID field last,
              * during a short window in time TLB entries with the old domain
              * ID but the new page tables may have been inserted.  This could
              * affect I/O of other devices using this same (old) domain ID.
              * Such updating therefore is not a problem if this was the only
              * device associated with the old domain ID.  Diverting I/O of any
              * of a dying domain's devices to the quarantine page tables is
              * intended anyway.
              */
             !pdev->domain->is_dying &&
             (any_pdev_behind_iommu(pdev->domain, pdev, iommu) ||
              pdev->phantom_stride) )
            printk(" %04x:%02x:%02x.%u: reassignment may cause %pd data corruption\n",
                   pdev->seg, bus, PCI_SLOT(devfn), PCI_FUNC(devfn),
                   pdev->domain);

        if ( pci_ats_device(iommu->seg, bus, pdev->devfn) &&
             iommu_has_cap(iommu, PCI_CAP_IOTLB_SHIFT) )
            ASSERT(get_field_from_reg_u32(
                       dte[3], IOMMU_DEV_TABLE_IOTLB_SUPPORT_MASK,
                       IOMMU_DEV_TABLE_IOTLB_SUPPORT_SHIFT) == dte_i);

        amd_iommu_flush_device(iommu, req_id);
    }

    spin_unlock_irqrestore(&iommu->lock, flags);

    AMD_IOMMU_DEBUG("Setup I/O page table: device id = %#x, type = %#x, "
                    "root table = %#"PRIx64", "
                    "domain = %d, paging mode = %d\n",
                    req_id, pdev->type,
                    page_to_maddr(hd->arch.root_table),
                    domain->domain_id, hd->arch.paging_mode);

    ASSERT(pcidevs_locked());

    if ( pci_ats_device(iommu->seg, bus, pdev->devfn) &&
         !pci_ats_enabled(iommu->seg, bus, pdev->devfn) )
    {
        if ( devfn == pdev->devfn )
            enable_ats_device(pdev, &iommu->ats_devices);

        amd_iommu_flush_iotlb(devfn, pdev, INV_IOMMU_ALL_PAGES_ADDRESS, 0);
    }

    return 0;
}

int __init amd_iov_detect(void)
{
    INIT_LIST_HEAD(&amd_iommu_head);

    if ( !iommu_enable && !iommu_intremap )
        return 0;

    if ( (amd_iommu_detect_acpi() !=0) || (iommu_found() == 0) )
    {
        printk("AMD-Vi: IOMMU not found!\n");
        iommu_intremap = 0;
        return -ENODEV;
    }

    iommu_ops = amd_iommu_ops;

    if ( amd_iommu_init() != 0 )
    {
        printk("AMD-Vi: Error initialization\n");
        return -ENODEV;
    }

    init_done = 1;

    if ( !amd_iommu_perdev_intremap )
        printk(XENLOG_WARNING "AMD-Vi: Using global interrupt remap table is not recommended (see XSA-36)!\n");
    return scan_pci_devices();
}

int amd_iommu_alloc_root(struct domain_iommu *hd)
{
    if ( unlikely(!hd->arch.root_table) )
    {
        hd->arch.root_table = alloc_amd_iommu_pgtable();
        if ( !hd->arch.root_table )
            return -ENOMEM;
    }

    return 0;
}

int __read_mostly amd_iommu_min_paging_mode = 1;

static int amd_iommu_domain_init(struct domain *d)
{
    struct domain_iommu *hd = dom_iommu(d);

    /*
     * Choose the number of levels for the IOMMU page tables.
     * - PV needs 3 or 4, depending on whether there is RAM (including hotplug
     *   RAM) above the 512G boundary.
     * - HVM could in principle use 3 or 4 depending on how much guest
     *   physical address space we give it, but this isn't known yet so use 4
     *   unilaterally.
     * - Unity maps may require an even higher number.
     */
    hd->arch.paging_mode = max(amd_iommu_get_paging_mode(
            is_hvm_domain(d)
            ? 1ul << (DEFAULT_DOMAIN_ADDRESS_WIDTH - PAGE_SHIFT)
            : get_upper_mfn_bound() + 1),
        amd_iommu_min_paging_mode);

    return 0;
}

static int amd_iommu_add_device(u8 devfn, struct pci_dev *pdev);

static void __hwdom_init amd_iommu_hwdom_init(struct domain *d)
{
    const struct amd_iommu *iommu;

    if ( allocate_domain_resources(dom_iommu(d)) )
        BUG();

    for_each_amd_iommu ( iommu )
        if ( iomem_deny_access(d, PFN_DOWN(iommu->mmio_base_phys),
                               PFN_DOWN(iommu->mmio_base_phys +
                                        IOMMU_MMIO_REGION_LENGTH - 1)) )
            BUG();

    /* Make sure workarounds are applied (if needed) before adding devices. */
    arch_iommu_hwdom_init(d);
    setup_hwdom_pci_devices(d, amd_iommu_add_device);
}

void amd_iommu_disable_domain_device(struct domain *domain,
                                     struct amd_iommu *iommu,
                                     u8 devfn, struct pci_dev *pdev)
{
    void *dte;
    unsigned long flags;
    int req_id;
    u8 bus = pdev->bus;

    BUG_ON ( iommu->dev_table.buffer == NULL );
    req_id = get_dma_requestor_id(iommu->seg, PCI_BDF2(bus, devfn));
    dte = iommu->dev_table.buffer + (req_id * IOMMU_DEV_TABLE_ENTRY_SIZE);

    spin_lock_irqsave(&iommu->lock, flags);
    if ( is_translation_valid((u32 *)dte) )
    {
        disable_translation((u32 *)dte);

        if ( pci_ats_device(iommu->seg, bus, pdev->devfn) &&
             iommu_has_cap(iommu, PCI_CAP_IOTLB_SHIFT) )
            iommu_dte_set_iotlb((u32 *)dte, 0);

        amd_iommu_flush_device(iommu, req_id);

        AMD_IOMMU_DEBUG("Disable: device id = %#x, "
                        "domain = %d, paging mode = %d\n",
                        req_id,  domain->domain_id,
                        dom_iommu(domain)->arch.paging_mode);
    }
    spin_unlock_irqrestore(&iommu->lock, flags);

    ASSERT(pcidevs_locked());

    if ( devfn == pdev->devfn &&
         pci_ats_device(iommu->seg, bus, devfn) &&
         pci_ats_enabled(iommu->seg, bus, devfn) )
        disable_ats_device(pdev);
}

static int reassign_device(struct domain *source, struct domain *target,
                           u8 devfn, struct pci_dev *pdev)
{
    struct amd_iommu *iommu;
    int bdf, rc;
    const struct ivrs_mappings *ivrs_mappings = get_ivrs_mappings(pdev->seg);

    bdf = PCI_BDF2(pdev->bus, pdev->devfn);
    iommu = find_iommu_for_device(pdev->seg, bdf);
    if ( !iommu )
    {
        AMD_IOMMU_DEBUG("Fail to find iommu."
                        " %04x:%02x:%x02.%x cannot be assigned to dom%d\n",
                        pdev->seg, pdev->bus, PCI_SLOT(devfn), PCI_FUNC(devfn),
                        target->domain_id);
        return -ENODEV;
    }

    rc = amd_iommu_setup_domain_device(target, iommu, devfn, pdev);
    if ( rc )
        return rc;

    if ( devfn == pdev->devfn && pdev->domain != target )
    {
        list_move(&pdev->domain_list, &target->arch.pdev_list);
        pdev->domain = target;
    }

    /*
     * If the device belongs to the hardware domain, and it has a unity mapping,
     * don't remove it from the hardware domain, because BIOS may reference that
     * mapping.
     */
    if ( !is_hardware_domain(source) )
    {
        rc = amd_iommu_reserve_domain_unity_unmap(
                 source,
                 ivrs_mappings[get_dma_requestor_id(pdev->seg, bdf)].unity_map);
        if ( rc )
            return rc;
    }

    AMD_IOMMU_DEBUG("Re-assign %04x:%02x:%02x.%u from dom%d to dom%d\n",
                    pdev->seg, pdev->bus, PCI_SLOT(devfn), PCI_FUNC(devfn),
                    source->domain_id, target->domain_id);

    return 0;
}

static int amd_iommu_assign_device(struct domain *d, u8 devfn,
                                   struct pci_dev *pdev,
                                   u32 flag)
{
    struct ivrs_mappings *ivrs_mappings = get_ivrs_mappings(pdev->seg);
    int bdf = PCI_BDF2(pdev->bus, devfn);
    int req_id = get_dma_requestor_id(pdev->seg, bdf);
    int rc = amd_iommu_reserve_domain_unity_map(
                 d, ivrs_mappings[req_id].unity_map, flag);

    if ( !rc )
        rc = reassign_device(pdev->domain, d, devfn, pdev);

    if ( rc && !is_hardware_domain(d) )
    {
        int ret = amd_iommu_reserve_domain_unity_unmap(
                      d, ivrs_mappings[req_id].unity_map);

        if ( ret )
        {
            printk(XENLOG_ERR "AMD-Vi: "
                   "unity-unmap for %pd/%04x:%02x:%02x.%u failed (%d)\n",
                   d, pdev->seg, pdev->bus,
                   PCI_SLOT(devfn), PCI_FUNC(devfn), ret);
            domain_crash(d);
        }
    }

    return rc;
}

static void deallocate_next_page_table(struct page_info *pg, int level)
{
    PFN_ORDER(pg) = level;
    spin_lock(&iommu_pt_cleanup_lock);
    page_list_add_tail(pg, &iommu_pt_cleanup_list);
    spin_unlock(&iommu_pt_cleanup_lock);
}

static void deallocate_page_table(struct page_info *pg)
{
    void *table_vaddr, *pde;
    u64 next_table_maddr;
    unsigned int index, level = PFN_ORDER(pg), next_level;

    PFN_ORDER(pg) = 0;

    if ( level <= 1 )
    {
        free_amd_iommu_pgtable(pg);
        return;
    }

    table_vaddr = __map_domain_page(pg);

    for ( index = 0; index < PTE_PER_TABLE_SIZE; index++ )
    {
        pde = table_vaddr + (index * IOMMU_PAGE_TABLE_ENTRY_SIZE);
        next_table_maddr = amd_iommu_get_address_from_pte(pde);
        next_level = iommu_next_level(pde);

        if ( (next_table_maddr != 0) && (next_level != 0) &&
             iommu_is_pte_present(pde) )
        {
            /* We do not support skip levels yet */
            ASSERT(next_level == level - 1);
            deallocate_next_page_table(maddr_to_page(next_table_maddr), 
                                       next_level);
        }
    }

    unmap_domain_page(table_vaddr);
    free_amd_iommu_pgtable(pg);
}

static void deallocate_iommu_page_tables(struct domain *d)
{
    struct domain_iommu *hd = dom_iommu(d);

    if ( iommu_use_hap_pt(d) )
        return;

    spin_lock(&hd->arch.mapping_lock);
    if ( hd->arch.root_table )
    {
        deallocate_next_page_table(hd->arch.root_table, hd->arch.paging_mode);
        hd->arch.root_table = NULL;
    }
    spin_unlock(&hd->arch.mapping_lock);
}


static void amd_iommu_domain_destroy(struct domain *d)
{
    iommu_identity_map_teardown(d);
    deallocate_iommu_page_tables(d);
    amd_iommu_flush_all_pages(d);
}

static int amd_iommu_add_device(u8 devfn, struct pci_dev *pdev)
{
    struct amd_iommu *iommu;
    u16 bdf;
    bool fresh_domid = false;
    int ret;

    if ( !pdev->domain )
        return -EINVAL;

    bdf = PCI_BDF2(pdev->bus, pdev->devfn);
    iommu = find_iommu_for_device(pdev->seg, bdf);
    if ( unlikely(!iommu) )
    {
        /* Filter bridge devices. */
        if ( pdev->type == DEV_TYPE_PCI_HOST_BRIDGE &&
             is_hardware_domain(pdev->domain) )
        {
            AMD_IOMMU_DEBUG("Skipping host bridge %04x:%02x:%02x.%u\n",
                            pdev->seg, pdev->bus, PCI_SLOT(devfn),
                            PCI_FUNC(devfn));
            return 0;
        }

        AMD_IOMMU_DEBUG("No iommu for %04x:%02x:%02x.%u; cannot be handed to d%d\n",
                        pdev->seg, pdev->bus, PCI_SLOT(devfn), PCI_FUNC(devfn),
                        pdev->domain->domain_id);
        return -ENODEV;
    }

    if ( iommu_quarantine && pdev->arch.pseudo_domid == DOMID_INVALID )
    {
        pdev->arch.pseudo_domid = iommu_alloc_domid(iommu->domid_map);
        if ( pdev->arch.pseudo_domid == DOMID_INVALID )
            return -ENOSPC;
        fresh_domid = true;
    }

    ret = amd_iommu_setup_domain_device(pdev->domain, iommu, devfn, pdev);
    if ( ret && fresh_domid )
    {
        iommu_free_domid(pdev->arch.pseudo_domid, iommu->domid_map);
        pdev->arch.pseudo_domid = DOMID_INVALID;
    }

    return ret;
}

static int amd_iommu_remove_device(u8 devfn, struct pci_dev *pdev)
{
    struct amd_iommu *iommu;
    u16 bdf;
    if ( !pdev->domain )
        return -EINVAL;

    bdf = PCI_BDF2(pdev->bus, pdev->devfn);
    iommu = find_iommu_for_device(pdev->seg, bdf);
    if ( !iommu )
    {
        AMD_IOMMU_DEBUG("Fail to find iommu."
                        " %04x:%02x:%02x.%u cannot be removed from dom%d\n",
                        pdev->seg, pdev->bus, PCI_SLOT(devfn), PCI_FUNC(devfn),
                        pdev->domain->domain_id);
        return -ENODEV;
    }

    amd_iommu_disable_domain_device(pdev->domain, iommu, devfn, pdev);

    iommu_free_domid(pdev->arch.pseudo_domid, iommu->domid_map);
    pdev->arch.pseudo_domid = DOMID_INVALID;

    return 0;
}

static int amd_iommu_group_id(u16 seg, u8 bus, u8 devfn)
{
    int bdf = PCI_BDF2(bus, devfn);

    return (bdf < ivrs_bdf_entries) ? get_dma_requestor_id(seg, bdf) : bdf;
}

#include <asm/io_apic.h>

static void amd_dump_p2m_table_level(struct page_info* pg, int level, 
                                     paddr_t gpa, int indent)
{
    paddr_t address;
    void *table_vaddr, *pde;
    paddr_t next_table_maddr;
    int index, next_level, present;
    u32 *entry;

    if ( level < 1 )
        return;

    table_vaddr = __map_domain_page(pg);
    if ( table_vaddr == NULL )
    {
        printk("Failed to map IOMMU domain page %"PRIpaddr"\n", 
                page_to_maddr(pg));
        return;
    }

    for ( index = 0; index < PTE_PER_TABLE_SIZE; index++ )
    {
        if ( !(index % 2) )
            process_pending_softirqs();

        pde = table_vaddr + (index * IOMMU_PAGE_TABLE_ENTRY_SIZE);
        next_table_maddr = amd_iommu_get_address_from_pte(pde);
        entry = pde;

        present = get_field_from_reg_u32(entry[0],
                                         IOMMU_PDE_PRESENT_MASK,
                                         IOMMU_PDE_PRESENT_SHIFT);

        if ( !present )
            continue;

        next_level = get_field_from_reg_u32(entry[0],
                                            IOMMU_PDE_NEXT_LEVEL_MASK,
                                            IOMMU_PDE_NEXT_LEVEL_SHIFT);

        if ( next_level && (next_level != (level - 1)) )
        {
            printk("IOMMU p2m table error. next_level = %d, expected %d\n",
                   next_level, level - 1);

            continue;
        }

        address = gpa + amd_offset_level_address(index, level);
        if ( next_level >= 1 )
            amd_dump_p2m_table_level(
                maddr_to_page(next_table_maddr), next_level,
                address, indent + 1);
        else
            printk("%*sdfn: %08lx  mfn: %08lx\n",
                   indent, "",
                   (unsigned long)PFN_DOWN(address),
                   (unsigned long)PFN_DOWN(next_table_maddr));
    }

    unmap_domain_page(table_vaddr);
}

static void amd_dump_p2m_table(struct domain *d)
{
    const struct domain_iommu *hd = dom_iommu(d);

    if ( !hd->arch.root_table )
        return;

    printk("p2m table has %d levels\n", hd->arch.paging_mode);
    amd_dump_p2m_table_level(hd->arch.root_table, hd->arch.paging_mode, 0, 0);
}

static const struct iommu_ops __initconstrel amd_iommu_ops = {
    .init = amd_iommu_domain_init,
    .hwdom_init = amd_iommu_hwdom_init,
    .quarantine_init = amd_iommu_quarantine_init,
    .add_device = amd_iommu_add_device,
    .remove_device = amd_iommu_remove_device,
    .assign_device  = amd_iommu_assign_device,
    .teardown = amd_iommu_domain_destroy,
    .map_page = amd_iommu_map_page,
    .unmap_page = amd_iommu_unmap_page,
    .iotlb_flush = amd_iommu_flush_iotlb_pages,
    .iotlb_flush_all = amd_iommu_flush_iotlb_all,
    .free_page_table = deallocate_page_table,
    .reassign_device = reassign_device,
    .get_device_group_id = amd_iommu_group_id,
    .update_ire_from_apic = amd_iommu_ioapic_update_ire,
    .update_ire_from_msi = amd_iommu_msi_msg_update_ire,
    .read_apic_from_ire = amd_iommu_read_ioapic_from_ire,
    .read_msi_from_ire = amd_iommu_read_msi_from_ire,
    .setup_hpet_msi = amd_setup_hpet_msi,
    .suspend = amd_iommu_suspend,
    .resume = amd_iommu_resume,
    .share_p2m = amd_iommu_share_p2m,
    .crash_shutdown = amd_iommu_crash_shutdown,
    .dump_p2m_table = amd_dump_p2m_table,
};
