
/* Portions are: Copyright (c) 1994 Linus Torvalds */

#ifndef __ASM_X86_PROCESSOR_H
#define __ASM_X86_PROCESSOR_H

#ifndef __ASSEMBLY__
#include <xen/cache.h>
#include <xen/types.h>
#include <xen/smp.h>
#include <xen/percpu.h>
#include <asm/types.h>
#include <asm/cpufeature.h>
#include <asm/desc.h>
#endif

#include <asm/x86-defns.h>
#include <asm/x86-vendors.h>

/*
 * Trap/fault mnemonics.
 */
#define TRAP_divide_error      0
#define TRAP_debug             1
#define TRAP_nmi               2
#define TRAP_int3              3
#define TRAP_overflow          4
#define TRAP_bounds            5
#define TRAP_invalid_op        6
#define TRAP_no_device         7
#define TRAP_double_fault      8
#define TRAP_copro_seg         9
#define TRAP_invalid_tss      10
#define TRAP_no_segment       11
#define TRAP_stack_error      12
#define TRAP_gp_fault         13
#define TRAP_page_fault       14
#define TRAP_spurious_int     15
#define TRAP_copro_error      16
#define TRAP_alignment_check  17
#define TRAP_machine_check    18
#define TRAP_simd_error       19
#define TRAP_virtualisation   20
#define TRAP_nr               32

#define TRAP_HAVE_EC X86_EXC_HAVE_EC

/* Set for entry via SYSCALL. Informs return code to use SYSRETQ not IRETQ. */
/* NB. Same as VGCF_in_syscall. No bits in common with any other TRAP_ defn. */
#define TRAP_syscall         256

/* Boolean return code: the reason for a fault has been fixed. */
#define EXCRET_fault_fixed 1

/* 'trap_bounce' flags values */
#define TBF_EXCEPTION          1
#define TBF_EXCEPTION_ERRCODE  2
#define TBF_INTERRUPT          8

/* 'arch_vcpu' flags values */
#define _TF_kernel_mode        0
#define TF_kernel_mode         (1<<_TF_kernel_mode)

/* #PF error code values. */
#define PFEC_page_present   (_AC(1,U) << 0)
#define PFEC_write_access   (_AC(1,U) << 1)
#define PFEC_user_mode      (_AC(1,U) << 2)
#define PFEC_reserved_bit   (_AC(1,U) << 3)
#define PFEC_insn_fetch     (_AC(1,U) << 4)
#define PFEC_prot_key       (_AC(1,U) << 5)
#define PFEC_shstk          (_AC(1,U) << 6)
#define PFEC_arch_mask      (_AC(0xffff,U)) /* Architectural PFEC values. */
/* Internally used only flags. */
#define PFEC_page_paged     (1U<<16)
#define PFEC_page_shared    (1U<<17)
#define PFEC_implicit       (1U<<18) /* Pagewalk input for ldt/gdt/idt/tr accesses. */
#define PFEC_synth_mask     (~PFEC_arch_mask) /* Synthetic PFEC values. */

/* Other exception error code values. */
#define X86_XEC_EXT         (_AC(1,U) << 0)
#define X86_XEC_IDT         (_AC(1,U) << 1)
#define X86_XEC_TI          (_AC(1,U) << 2)

#define XEN_MINIMAL_CR4 (X86_CR4_PGE | X86_CR4_PAE)

#define XEN_CR4_PV32_BITS (X86_CR4_SMEP|X86_CR4_SMAP)

/* Common SYSCALL parameters. */
#define XEN_MSR_STAR (((uint64_t)FLAT_RING3_CS32 << 48) |   \
                      ((uint64_t)__HYPERVISOR_CS << 32))
#define XEN_SYSCALL_MASK (X86_EFLAGS_AC|X86_EFLAGS_VM|X86_EFLAGS_RF|    \
                          X86_EFLAGS_NT|X86_EFLAGS_DF|X86_EFLAGS_IF|    \
                          X86_EFLAGS_TF)

/*
 * Host IA32_CR_PAT value to cover all memory types.  This is not the default
 * MSR_PAT value, and is an ABI with PV guests.
 */
#define XEN_MSR_PAT ((_AC(X86_MT_WB,  ULL) << 0x00) | \
                     (_AC(X86_MT_WT,  ULL) << 0x08) | \
                     (_AC(X86_MT_UCM, ULL) << 0x10) | \
                     (_AC(X86_MT_UC,  ULL) << 0x18) | \
                     (_AC(X86_MT_WC,  ULL) << 0x20) | \
                     (_AC(X86_MT_WP,  ULL) << 0x28) | \
                     (_AC(X86_MT_UC,  ULL) << 0x30) | \
                     (_AC(X86_MT_UC,  ULL) << 0x38))

#ifndef __ASSEMBLY__

struct domain;
struct vcpu;

struct x86_cpu_id {
    uint16_t vendor;
    uint16_t family;
    uint16_t model;
    uint16_t feature;   /* bit index */
    const void *driver_data;
};

struct cpuinfo_x86 {
    __u8 x86;            /* CPU family */
    __u8 x86_vendor;     /* CPU vendor */
    __u8 x86_model;
    __u8 x86_mask;
    int  cpuid_level;    /* Maximum supported CPUID level, -1=no CPUID */
    __u32 extended_cpuid_level; /* Maximum supported CPUID extended level */
    unsigned int x86_capability[NCAPINTS];
    char x86_vendor_id[16];
    char x86_model_id[64];
    int  x86_cache_size; /* in KB - valid for CPUS which support this call  */
    int  x86_cache_alignment;    /* In bytes */
    __u32 x86_max_cores; /* cpuid returned max cores value */
    __u32 booted_cores;  /* number of cores as seen by OS */
    __u32 x86_num_siblings; /* cpuid logical cpus per chip value */
    __u32 apicid;
    __u32 phys_proc_id;    /* package ID of each logical CPU */
    __u32 cpu_core_id;     /* core ID of each logical CPU*/
    __u32 compute_unit_id; /* AMD compute unit ID of each logical CPU */
    unsigned short x86_clflush_size;
} __cacheline_aligned;

/*
 * capabilities of CPUs
 */

extern struct cpuinfo_x86 boot_cpu_data;

extern struct cpuinfo_x86 cpu_data[];
#define current_cpu_data cpu_data[smp_processor_id()]

extern bool probe_cpuid_faulting(void);
extern void ctxt_switch_levelling(const struct vcpu *next);
extern void (*ctxt_switch_masking)(const struct vcpu *next);

extern bool_t opt_cpu_info;
extern u32 trampoline_efer;
extern u64 trampoline_misc_enable_off;

/* Maximum width of physical addresses supported by the hardware. */
extern unsigned int paddr_bits;
/* Max physical address width supported within HAP guests. */
extern unsigned int hap_paddr_bits;
/* Maximum width of virtual addresses supported by the hardware. */
extern unsigned int vaddr_bits;

extern const struct x86_cpu_id *x86_match_cpu(const struct x86_cpu_id table[]);

extern void identify_cpu(struct cpuinfo_x86 *);
extern void setup_clear_cpu_cap(unsigned int);
extern void setup_force_cpu_cap(unsigned int);
extern bool is_forced_cpu_cap(unsigned int);
extern void print_cpu_info(unsigned int cpu);
extern void init_intel_cacheinfo(struct cpuinfo_x86 *c);

#define cpu_to_core(_cpu)   (cpu_data[_cpu].cpu_core_id)
#define cpu_to_socket(_cpu) (cpu_data[_cpu].phys_proc_id)

unsigned int apicid_to_socket(unsigned int);

static inline int cpu_nr_siblings(unsigned int cpu)
{
    return cpu_data[cpu].x86_num_siblings;
}

/*
 * Generic CPUID function
 * clear %ecx since some cpus (Cyrix MII) do not set or clear %ecx
 * resulting in stale register contents being returned.
 */
#define cpuid(_op,_eax,_ebx,_ecx,_edx)          \
    asm volatile ( "cpuid"                      \
          : "=a" (*(int *)(_eax)),              \
            "=b" (*(int *)(_ebx)),              \
            "=c" (*(int *)(_ecx)),              \
            "=d" (*(int *)(_edx))               \
          : "0" (_op), "2" (0) )

/* Some CPUID calls want 'count' to be placed in ecx */
static inline void cpuid_count(
    unsigned int op,
    unsigned int count,
    unsigned int *eax,
    unsigned int *ebx,
    unsigned int *ecx,
    unsigned int *edx)
{
    asm volatile ( "cpuid"
          : "=a" (*eax), "=b" (*ebx), "=c" (*ecx), "=d" (*edx)
          : "0" (op), "c" (count) );
}

/*
 * CPUID functions returning a single datum
 */
static always_inline unsigned int cpuid_eax(unsigned int op)
{
    unsigned int eax;

    asm volatile ( "cpuid"
          : "=a" (eax)
          : "0" (op)
          : "bx", "cx", "dx" );
    return eax;
}

static always_inline unsigned int cpuid_ebx(unsigned int op)
{
    unsigned int eax, ebx;

    asm volatile ( "cpuid"
          : "=a" (eax), "=b" (ebx)
          : "0" (op)
          : "cx", "dx" );
    return ebx;
}

static always_inline unsigned int cpuid_ecx(unsigned int op)
{
    unsigned int eax, ecx;

    asm volatile ( "cpuid"
          : "=a" (eax), "=c" (ecx)
          : "0" (op)
          : "bx", "dx" );
    return ecx;
}

static always_inline unsigned int cpuid_edx(unsigned int op)
{
    unsigned int eax, edx;

    asm volatile ( "cpuid"
          : "=a" (eax), "=d" (edx)
          : "0" (op)
          : "bx", "cx" );
    return edx;
}

static always_inline unsigned int cpuid_count_ebx(
    unsigned int leaf, unsigned int subleaf)
{
    unsigned int ebx, tmp;

    cpuid_count(leaf, subleaf, &tmp, &ebx, &tmp, &tmp);

    return ebx;
}

static always_inline unsigned int cpuid_count_edx(
    unsigned int leaf, unsigned int subleaf)
{
    unsigned int edx, tmp;

    cpuid_count(leaf, subleaf, &tmp, &tmp, &tmp, &edx);

    return edx;
}

static inline unsigned long read_cr0(void)
{
    unsigned long cr0;
    asm volatile ( "mov %%cr0,%0\n\t" : "=r" (cr0) );
    return cr0;
} 

static inline void write_cr0(unsigned long val)
{
    asm volatile ( "mov %0,%%cr0" : : "r" ((unsigned long)val) );
}

static inline unsigned long read_cr2(void)
{
    unsigned long cr2;
    asm volatile ( "mov %%cr2,%0\n\t" : "=r" (cr2) );
    return cr2;
}

static inline void write_cr3(unsigned long val)
{
    asm volatile ( "mov %0, %%cr3" : : "r" (val) : "memory" );
}

static inline unsigned long cr3_pa(unsigned long cr3)
{
    return cr3 & X86_CR3_ADDR_MASK;
}

static inline unsigned int cr3_pcid(unsigned long cr3)
{
    return IS_ENABLED(CONFIG_PV) ? cr3 & X86_CR3_PCID_MASK : 0;
}

static inline unsigned long read_cr4(void)
{
    return get_cpu_info()->cr4;
}

static inline void write_cr4(unsigned long val)
{
    struct cpu_info *info = get_cpu_info();

#ifdef CONFIG_PV
    /* No global pages in case of PCIDs enabled! */
    ASSERT(!(val & X86_CR4_PGE) || !(val & X86_CR4_PCIDE));
#else
    ASSERT(!(val & X86_CR4_PCIDE));
#endif

    /*
     * On hardware supporting FSGSBASE, the value in %cr4 is the kernel's
     * choice for 64bit PV guests, which impacts whether Xen can use the
     * instructions.
     *
     * The {rd,wr}{fs,gs}base() helpers use info->cr4 to work out whether it
     * is safe to execute the {RD,WR}{FS,GS}BASE instruction, falling back to
     * the MSR path if not.  Some users require interrupt safety.
     *
     * If FSGSBASE is currently or about to become clear, reflect this in
     * info->cr4 before updating %cr4, so an interrupt which hits in the
     * middle won't observe FSGSBASE set in info->cr4 but clear in %cr4.
     */
    info->cr4 = val & (info->cr4 | ~X86_CR4_FSGSBASE);

    asm volatile ( "mov %[val], %%cr4"
                   : "+m" (info->cr4) /* Force ordering without a barrier. */
                   : [val] "r" (val) );

    info->cr4 = val;
}

/* Clear and set 'TS' bit respectively */
static inline void clts(void) 
{
    asm volatile ( "clts" );
}

static inline void stts(void) 
{
    write_cr0(X86_CR0_TS|read_cr0());
}

/*
 * Save the cr4 feature set we're using (ie
 * Pentium 4MB enable and PPro Global page
 * enable), so that any CPU's that boot up
 * after us can get the correct flags.
 */
extern unsigned long mmu_cr4_features;

static always_inline void set_in_cr4 (unsigned long mask)
{
    mmu_cr4_features |= mask;
    write_cr4(read_cr4() | mask);
}

static always_inline void __monitor(const void *eax, unsigned long ecx,
                                    unsigned long edx)
{
    /* "monitor %eax,%ecx,%edx;" */
    asm volatile (
        ".byte 0x0f,0x01,0xc8;"
        : : "a" (eax), "c" (ecx), "d"(edx) );
}

static always_inline void __mwait(unsigned long eax, unsigned long ecx)
{
    /* "mwait %eax,%ecx;" */
    asm volatile (
        ".byte 0x0f,0x01,0xc9;"
        : : "a" (eax), "c" (ecx) );
}

#define IOBMP_BYTES             8192
#define IOBMP_INVALID_OFFSET    0x8000

struct __packed tss64 {
    uint32_t :32;
    uint64_t rsp0, rsp1, rsp2;
    uint64_t :64;
    /*
     * Interrupt Stack Table is 1-based so tss->ist[0] corresponds to an IST
     * value of 1 in an Interrupt Descriptor.
     */
    uint64_t ist[7];
    uint64_t :64;
    uint16_t :16, bitmap;
};
struct tss_page {
    uint64_t __aligned(PAGE_SIZE) ist_ssp[8];
    struct tss64 tss;
};
DECLARE_PER_CPU(struct tss_page, tss_page);

#define IST_NONE 0UL
#define IST_MCE  1UL
#define IST_NMI  2UL
#define IST_DB   3UL
#define IST_DF   4UL
#define IST_MAX  4UL

/* Set the Interrupt Stack Table used by a particular IDT entry. */
static inline void set_ist(idt_entry_t *idt, unsigned int ist)
{
    /* IST is a 3 bit field, 32 bits into the IDT entry. */
    ASSERT(ist <= IST_MAX);

    /* Typically used on a live idt.  Disuade any clever optimisations. */
    ACCESS_ONCE(idt->ist) = ist;
}

static inline void enable_each_ist(idt_entry_t *idt)
{
    set_ist(&idt[TRAP_double_fault],  IST_DF);
    set_ist(&idt[TRAP_nmi],           IST_NMI);
    set_ist(&idt[TRAP_machine_check], IST_MCE);
    set_ist(&idt[TRAP_debug],         IST_DB);
}

static inline void disable_each_ist(idt_entry_t *idt)
{
    set_ist(&idt[TRAP_double_fault],  IST_NONE);
    set_ist(&idt[TRAP_nmi],           IST_NONE);
    set_ist(&idt[TRAP_machine_check], IST_NONE);
    set_ist(&idt[TRAP_debug],         IST_NONE);
}

#define IDT_ENTRIES 256
extern idt_entry_t idt_table[];
extern idt_entry_t *idt_tables[];

DECLARE_PER_CPU(root_pgentry_t *, root_pgt);

extern void write_ptbase(struct vcpu *v);

/* REP NOP (PAUSE) is a good thing to insert into busy-wait loops. */
static always_inline void rep_nop(void)
{
    asm volatile ( "rep;nop" : : : "memory" );
}

#define cpu_relax() rep_nop()

void show_code(const struct cpu_user_regs *regs);
void show_stack_overflow(unsigned int cpu, const struct cpu_user_regs *regs);
void show_registers(const struct cpu_user_regs *regs);
void show_execution_state(const struct cpu_user_regs *regs);
void cf_check show_execution_state_nonconst(struct cpu_user_regs *regs);
#define dump_execution_state() \
    run_in_exception_handler(show_execution_state_nonconst)
void show_page_walk(unsigned long addr);
void noreturn fatal_trap(const struct cpu_user_regs *regs, bool_t show_remote);

extern void mtrr_ap_init(void);
extern void mtrr_bp_init(void);

void mcheck_init(struct cpuinfo_x86 *c, bool_t bsp);

#define DECLARE_TRAP_HANDLER(_name)                    \
    void _name(void);                                  \
    void do_ ## _name(struct cpu_user_regs *regs)
#define DECLARE_TRAP_HANDLER_CONST(_name)              \
    void _name(void);                                  \
    void do_ ## _name(const struct cpu_user_regs *regs)

DECLARE_TRAP_HANDLER(divide_error);
DECLARE_TRAP_HANDLER(debug);
DECLARE_TRAP_HANDLER_CONST(nmi);
DECLARE_TRAP_HANDLER(int3);
DECLARE_TRAP_HANDLER(overflow);
DECLARE_TRAP_HANDLER(bounds);
DECLARE_TRAP_HANDLER(invalid_op);
DECLARE_TRAP_HANDLER(device_not_available);
DECLARE_TRAP_HANDLER(double_fault);
DECLARE_TRAP_HANDLER(invalid_TSS);
DECLARE_TRAP_HANDLER(segment_not_present);
DECLARE_TRAP_HANDLER(stack_segment);
DECLARE_TRAP_HANDLER(general_protection);
DECLARE_TRAP_HANDLER(page_fault);
DECLARE_TRAP_HANDLER(early_page_fault);
DECLARE_TRAP_HANDLER(coprocessor_error);
DECLARE_TRAP_HANDLER(simd_coprocessor_error);
DECLARE_TRAP_HANDLER_CONST(machine_check);
DECLARE_TRAP_HANDLER(alignment_check);
DECLARE_TRAP_HANDLER(entry_CP);

DECLARE_TRAP_HANDLER(entry_int82);

#undef DECLARE_TRAP_HANDLER_CONST
#undef DECLARE_TRAP_HANDLER

void trap_nop(void);

static inline void enable_nmis(void)
{
    unsigned long tmp;

    asm volatile ( "mov     %%rsp, %[rsp]        \n\t"
                   "lea    .Ldone(%%rip), %[rip] \n\t"
#ifdef CONFIG_XEN_SHSTK
                   /* Check for CET-SS being active. */
                   "mov    $1, %k[ssp]           \n\t"
                   "rdsspq %[ssp]                \n\t"
                   "cmp    $1, %k[ssp]           \n\t"
                   "je     .Lshstk_done          \n\t"

                   /* Push 3 words on the shadow stack */
                   ".rept 3                      \n\t"
                   "call 1f; nop; 1:             \n\t"
                   ".endr                        \n\t"

                   /* Fixup to be an IRET shadow stack frame */
                   "wrssq  %q[cs], -1*8(%[ssp])  \n\t"
                   "wrssq  %[rip], -2*8(%[ssp])  \n\t"
                   "wrssq  %[ssp], -3*8(%[ssp])  \n\t"

                   ".Lshstk_done:"
#endif
                   /* Write an IRET regular frame */
                   "push   %[ss]                 \n\t"
                   "push   %[rsp]                \n\t"
                   "pushf                        \n\t"
                   "push   %q[cs]                \n\t"
                   "push   %[rip]                \n\t"
                   "iretq                        \n\t"
                   ".Ldone:                      \n\t"
                   : [rip] "=&r" (tmp),
                     [rsp] "=&r" (tmp),
                     [ssp] "=&r" (tmp)
                   : [ss] "i" (__HYPERVISOR_DS),
                     [cs] "r" (__HYPERVISOR_CS) );
}

void sysenter_entry(void);
void sysenter_eflags_saved(void);
void int80_direct_trap(void);

struct stubs {
    union {
        void(*func)(void);
        unsigned long addr;
    };
    unsigned long mfn;
};

DECLARE_PER_CPU(struct stubs, stubs);
unsigned long alloc_stub_page(unsigned int cpu, unsigned long *mfn);

void cpuid_hypervisor_leaves(const struct vcpu *v, uint32_t leaf,
                             uint32_t subleaf, struct cpuid_leaf *res);
int guest_rdmsr_xen(const struct vcpu *v, uint32_t idx, uint64_t *val);
int guest_wrmsr_xen(struct vcpu *v, uint32_t idx, uint64_t val);

static inline uint8_t get_cpu_family(uint32_t raw, uint8_t *model,
                                     uint8_t *stepping)
{
    uint8_t fam = (raw >> 8) & 0xf;

    if ( fam == 0xf )
        fam += (raw >> 20) & 0xff;

    if ( model )
    {
        uint8_t mod = (raw >> 4) & 0xf;

        if ( fam >= 0x6 )
            mod |= (raw >> 12) & 0xf0;

        *model = mod;
    }
    if ( stepping )
        *stepping = raw & 0xf;
    return fam;
}

extern int8_t opt_tsx, cpu_has_tsx_ctrl;
extern bool rtm_disabled;
void tsx_init(void);

void update_mcu_opt_ctrl(void);
void set_in_mcu_opt_ctrl(uint32_t mask, uint32_t val);

enum ap_boot_method {
    AP_BOOT_NORMAL,
    AP_BOOT_SKINIT,
};
extern enum ap_boot_method ap_boot_method;

#endif /* !__ASSEMBLY__ */

#endif /* __ASM_X86_PROCESSOR_H */

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
