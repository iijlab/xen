/*
 * cpufeature.h
 *
 * Defines x86 CPU feature bits
 */
#ifndef __ASM_I386_CPUFEATURE_H
#define __ASM_I386_CPUFEATURE_H

#include <xen/const.h>
#include <asm/cpuid.h>

#define cpufeat_word(idx)	((idx) / 32)
#define cpufeat_bit(idx)	((idx) % 32)
#define cpufeat_mask(idx)	(_AC(1, U) << cpufeat_bit(idx))

/* An alias of a feature we know is always going to be present. */
#define X86_FEATURE_ALWAYS      X86_FEATURE_LM

#ifndef __ASSEMBLY__
#include <xen/bitops.h>

#define cpu_has(c, bit)		test_bit(bit, (c)->x86_capability)
#define boot_cpu_has(bit)	test_bit(bit, boot_cpu_data.x86_capability)

#define CPUID_PM_LEAF                    6
#define CPUID6_ECX_APERFMPERF_CAPABILITY 0x1

/* CPUID level 0x00000001.edx */
#define cpu_has_fpu             1
#define cpu_has_de              1
#define cpu_has_pse             1
#define cpu_has_apic            boot_cpu_has(X86_FEATURE_APIC)
#define cpu_has_sep             boot_cpu_has(X86_FEATURE_SEP)
#define cpu_has_mtrr            1
#define cpu_has_pge             1
#define cpu_has_pse36           boot_cpu_has(X86_FEATURE_PSE36)
#define cpu_has_clflush         boot_cpu_has(X86_FEATURE_CLFLUSH)
#define cpu_has_mmx             1
#define cpu_has_htt             boot_cpu_has(X86_FEATURE_HTT)

/* CPUID level 0x00000001.ecx */
#define cpu_has_sse3            boot_cpu_has(X86_FEATURE_SSE3)
#define cpu_has_pclmulqdq       boot_cpu_has(X86_FEATURE_PCLMULQDQ)
#define cpu_has_monitor         boot_cpu_has(X86_FEATURE_MONITOR)
#define cpu_has_vmx             boot_cpu_has(X86_FEATURE_VMX)
#define cpu_has_eist            boot_cpu_has(X86_FEATURE_EIST)
#define cpu_has_ssse3           boot_cpu_has(X86_FEATURE_SSSE3)
#define cpu_has_fma             boot_cpu_has(X86_FEATURE_FMA)
#define cpu_has_cx16            boot_cpu_has(X86_FEATURE_CX16)
#define cpu_has_pdcm            boot_cpu_has(X86_FEATURE_PDCM)
#define cpu_has_pcid            boot_cpu_has(X86_FEATURE_PCID)
#define cpu_has_sse4_1          boot_cpu_has(X86_FEATURE_SSE4_1)
#define cpu_has_sse4_2          boot_cpu_has(X86_FEATURE_SSE4_2)
#define cpu_has_x2apic          boot_cpu_has(X86_FEATURE_X2APIC)
#define cpu_has_popcnt          boot_cpu_has(X86_FEATURE_POPCNT)
#define cpu_has_aesni           boot_cpu_has(X86_FEATURE_AESNI)
#define cpu_has_xsave           boot_cpu_has(X86_FEATURE_XSAVE)
#define cpu_has_avx             boot_cpu_has(X86_FEATURE_AVX)
#define cpu_has_f16c            boot_cpu_has(X86_FEATURE_F16C)
#define cpu_has_rdrand          boot_cpu_has(X86_FEATURE_RDRAND)
#define cpu_has_hypervisor      boot_cpu_has(X86_FEATURE_HYPERVISOR)

/* CPUID level 0x80000001.edx */
#define cpu_has_nx              boot_cpu_has(X86_FEATURE_NX)
#define cpu_has_page1gb         boot_cpu_has(X86_FEATURE_PAGE1GB)
#define cpu_has_rdtscp          boot_cpu_has(X86_FEATURE_RDTSCP)
#define cpu_has_3dnow_ext       boot_cpu_has(X86_FEATURE_3DNOWEXT)
#define cpu_has_3dnow           boot_cpu_has(X86_FEATURE_3DNOW)

/* CPUID level 0x80000001.ecx */
#define cpu_has_cmp_legacy      boot_cpu_has(X86_FEATURE_CMP_LEGACY)
#define cpu_has_svm             boot_cpu_has(X86_FEATURE_SVM)
#define cpu_has_sse4a           boot_cpu_has(X86_FEATURE_SSE4A)
#define cpu_has_xop             boot_cpu_has(X86_FEATURE_XOP)
#define cpu_has_skinit          boot_cpu_has(X86_FEATURE_SKINIT)
#define cpu_has_fma4            boot_cpu_has(X86_FEATURE_FMA4)
#define cpu_has_tbm             boot_cpu_has(X86_FEATURE_TBM)

/* CPUID level 0x0000000D:1.eax */
#define cpu_has_xsaveopt        boot_cpu_has(X86_FEATURE_XSAVEOPT)
#define cpu_has_xsavec          boot_cpu_has(X86_FEATURE_XSAVEC)
#define cpu_has_xgetbv1         boot_cpu_has(X86_FEATURE_XGETBV1)
#define cpu_has_xsaves          boot_cpu_has(X86_FEATURE_XSAVES)

/* CPUID level 0x00000007:0.ebx */
#define cpu_has_bmi1            boot_cpu_has(X86_FEATURE_BMI1)
#define cpu_has_hle             boot_cpu_has(X86_FEATURE_HLE)
#define cpu_has_avx2            boot_cpu_has(X86_FEATURE_AVX2)
#define cpu_has_smep            boot_cpu_has(X86_FEATURE_SMEP)
#define cpu_has_bmi2            boot_cpu_has(X86_FEATURE_BMI2)
#define cpu_has_invpcid         boot_cpu_has(X86_FEATURE_INVPCID)
#define cpu_has_rtm             boot_cpu_has(X86_FEATURE_RTM)
#define cpu_has_pqe             boot_cpu_has(X86_FEATURE_PQE)
#define cpu_has_fpu_sel         (!boot_cpu_has(X86_FEATURE_NO_FPU_SEL))
#define cpu_has_mpx             boot_cpu_has(X86_FEATURE_MPX)
#define cpu_has_avx512f         boot_cpu_has(X86_FEATURE_AVX512F)
#define cpu_has_avx512dq        boot_cpu_has(X86_FEATURE_AVX512DQ)
#define cpu_has_rdseed          boot_cpu_has(X86_FEATURE_RDSEED)
#define cpu_has_smap            boot_cpu_has(X86_FEATURE_SMAP)
#define cpu_has_avx512_ifma     boot_cpu_has(X86_FEATURE_AVX512_IFMA)
#define cpu_has_clflushopt      boot_cpu_has(X86_FEATURE_CLFLUSHOPT)
#define cpu_has_clwb            boot_cpu_has(X86_FEATURE_CLWB)
#define cpu_has_avx512er        boot_cpu_has(X86_FEATURE_AVX512ER)
#define cpu_has_avx512cd        boot_cpu_has(X86_FEATURE_AVX512CD)
#define cpu_has_proc_trace      boot_cpu_has(X86_FEATURE_PROC_TRACE)
#define cpu_has_sha             boot_cpu_has(X86_FEATURE_SHA)
#define cpu_has_avx512bw        boot_cpu_has(X86_FEATURE_AVX512BW)
#define cpu_has_avx512vl        boot_cpu_has(X86_FEATURE_AVX512VL)

/* CPUID level 0x00000007:0.ecx */
#define cpu_has_avx512_vbmi     boot_cpu_has(X86_FEATURE_AVX512_VBMI)
#define cpu_has_pku             boot_cpu_has(X86_FEATURE_PKU)
#define cpu_has_avx512_vbmi2    boot_cpu_has(X86_FEATURE_AVX512_VBMI2)
#define cpu_has_gfni            boot_cpu_has(X86_FEATURE_GFNI)
#define cpu_has_vaes            boot_cpu_has(X86_FEATURE_VAES)
#define cpu_has_vpclmulqdq      boot_cpu_has(X86_FEATURE_VPCLMULQDQ)
#define cpu_has_avx512_vnni     boot_cpu_has(X86_FEATURE_AVX512_VNNI)
#define cpu_has_avx512_bitalg   boot_cpu_has(X86_FEATURE_AVX512_BITALG)
#define cpu_has_avx512_vpopcntdq boot_cpu_has(X86_FEATURE_AVX512_VPOPCNTDQ)
#define cpu_has_rdpid           boot_cpu_has(X86_FEATURE_RDPID)
#define cpu_has_movdiri         boot_cpu_has(X86_FEATURE_MOVDIRI)
#define cpu_has_movdir64b       boot_cpu_has(X86_FEATURE_MOVDIR64B)
#define cpu_has_enqcmd          boot_cpu_has(X86_FEATURE_ENQCMD)
#define cpu_has_pks             boot_cpu_has(X86_FEATURE_PKS)

/* CPUID level 0x80000007.edx */
#define cpu_has_hw_pstate       boot_cpu_has(X86_FEATURE_HW_PSTATE)
#define cpu_has_itsc            boot_cpu_has(X86_FEATURE_ITSC)

/* CPUID level 0x80000008.ebx */
#define cpu_has_amd_ssbd        boot_cpu_has(X86_FEATURE_AMD_SSBD)
#define cpu_has_virt_ssbd       boot_cpu_has(X86_FEATURE_VIRT_SSBD)
#define cpu_has_ssb_no          boot_cpu_has(X86_FEATURE_SSB_NO)

/* CPUID level 0x00000007:0.edx */
#define cpu_has_avx512_4vnniw   boot_cpu_has(X86_FEATURE_AVX512_4VNNIW)
#define cpu_has_avx512_4fmaps   boot_cpu_has(X86_FEATURE_AVX512_4FMAPS)
#define cpu_has_avx512_vp2intersect boot_cpu_has(X86_FEATURE_AVX512_VP2INTERSECT)
#define cpu_has_srbds_ctrl      boot_cpu_has(X86_FEATURE_SRBDS_CTRL)
#define cpu_has_rtm_always_abort boot_cpu_has(X86_FEATURE_RTM_ALWAYS_ABORT)
#define cpu_has_tsx_force_abort boot_cpu_has(X86_FEATURE_TSX_FORCE_ABORT)
#define cpu_has_serialize       boot_cpu_has(X86_FEATURE_SERIALIZE)
#define cpu_has_avx512_fp16     boot_cpu_has(X86_FEATURE_AVX512_FP16)
#define cpu_has_arch_caps       boot_cpu_has(X86_FEATURE_ARCH_CAPS)

/* CPUID level 0x00000007:1.eax */
#define cpu_has_avx_vnni        boot_cpu_has(X86_FEATURE_AVX_VNNI)
#define cpu_has_avx512_bf16     boot_cpu_has(X86_FEATURE_AVX512_BF16)

/* Synthesized. */
#define cpu_has_arch_perfmon    boot_cpu_has(X86_FEATURE_ARCH_PERFMON)
#define cpu_has_cpuid_faulting  boot_cpu_has(X86_FEATURE_CPUID_FAULTING)
#define cpu_has_aperfmperf      boot_cpu_has(X86_FEATURE_APERFMPERF)
#define cpu_has_lfence_dispatch boot_cpu_has(X86_FEATURE_LFENCE_DISPATCH)
#define cpu_has_nscb            boot_cpu_has(X86_FEATURE_NSCB)
#define cpu_has_xen_lbr         boot_cpu_has(X86_FEATURE_XEN_LBR)
#define cpu_has_xen_shstk       boot_cpu_has(X86_FEATURE_XEN_SHSTK)
#define cpu_has_xen_ibt         boot_cpu_has(X86_FEATURE_XEN_IBT)

#define cpu_has_msr_tsc_aux     (cpu_has_rdtscp || cpu_has_rdpid)

/* Bugs. */
#define cpu_bug_fpu_ptrs        boot_cpu_has(X86_BUG_FPU_PTRS)
#define cpu_bug_null_seg        boot_cpu_has(X86_BUG_NULL_SEG)

enum _cache_type {
    CACHE_TYPE_NULL = 0,
    CACHE_TYPE_DATA = 1,
    CACHE_TYPE_INST = 2,
    CACHE_TYPE_UNIFIED = 3
};

union _cpuid4_leaf_eax {
    struct {
        enum _cache_type type:5;
        unsigned int level:3;
        unsigned int is_self_initializing:1;
        unsigned int is_fully_associative:1;
        unsigned int reserved:4;
        unsigned int num_threads_sharing:12;
        unsigned int num_cores_on_die:6;
    } split;
    u32 full;
};

union _cpuid4_leaf_ebx {
    struct {
        unsigned int coherency_line_size:12;
        unsigned int physical_line_partition:10;
        unsigned int ways_of_associativity:10;
    } split;
    u32 full;
};

union _cpuid4_leaf_ecx {
    struct {
        unsigned int number_of_sets:32;
    } split;
    u32 full;
};

struct cpuid4_info {
    union _cpuid4_leaf_eax eax;
    union _cpuid4_leaf_ebx ebx;
    union _cpuid4_leaf_ecx ecx;
    unsigned long size;
};

int cpuid4_cache_lookup(int index, struct cpuid4_info *this_leaf);
#endif /* !__ASSEMBLY__ */

#endif /* __ASM_I386_CPUFEATURE_H */

/* 
 * Local Variables:
 * mode:c
 * comment-column:42
 * End:
 */
