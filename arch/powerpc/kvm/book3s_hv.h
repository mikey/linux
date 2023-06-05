// SPDX-License-Identifier: GPL-2.0-only

/*
 * Privileged (non-hypervisor) host registers to save.
 */
#include "asm/guest-state-buffer.h"

struct p9_host_os_sprs {
	unsigned long iamr;
	unsigned long amr;

	unsigned int pmc1;
	unsigned int pmc2;
	unsigned int pmc3;
	unsigned int pmc4;
	unsigned int pmc5;
	unsigned int pmc6;
	unsigned long mmcr0;
	unsigned long mmcr1;
	unsigned long mmcr2;
	unsigned long mmcr3;
	unsigned long mmcra;
	unsigned long siar;
	unsigned long sier1;
	unsigned long sier2;
	unsigned long sier3;
	unsigned long sdar;
};

static inline bool nesting_enabled(struct kvm *kvm)
{
	return kvm->arch.nested_enable && kvm_is_radix(kvm);
}

bool load_vcpu_state(struct kvm_vcpu *vcpu,
			   struct p9_host_os_sprs *host_os_sprs);
void store_vcpu_state(struct kvm_vcpu *vcpu);
void save_p9_host_os_sprs(struct p9_host_os_sprs *host_os_sprs);
void restore_p9_host_os_sprs(struct kvm_vcpu *vcpu,
				    struct p9_host_os_sprs *host_os_sprs);
void switch_pmu_to_guest(struct kvm_vcpu *vcpu,
			    struct p9_host_os_sprs *host_os_sprs);
void switch_pmu_to_host(struct kvm_vcpu *vcpu,
			    struct p9_host_os_sprs *host_os_sprs);

#ifdef CONFIG_KVM_BOOK3S_HV_P9_TIMING
void accumulate_time(struct kvm_vcpu *vcpu, struct kvmhv_tb_accumulator *next);
#define start_timing(vcpu, next) accumulate_time(vcpu, next)
#define end_timing(vcpu) accumulate_time(vcpu, NULL)
#else
#define accumulate_time(vcpu, next) do {} while (0)
#define start_timing(vcpu, next) do {} while (0)
#define end_timing(vcpu) do {} while (0)
#endif

#define HV_WRAPPER_SET(reg, size, iden)					\
static inline void kvmppc_set_##reg ##_hv(struct kvm_vcpu *vcpu, u##size val)	\
{									\
	vcpu->arch.reg = val;						\
	kvmhv_papr_mark_dirty(vcpu, iden);				\
}

#define HV_WRAPPER_GET(reg, size, iden)					\
static inline u##size kvmppc_get_##reg ##_hv(struct kvm_vcpu *vcpu)	\
{									\
	kvmhv_papr_cached_reload(vcpu, iden);				\
	return vcpu->arch.reg;						\
}

#define HV_WRAPPER(reg, size, iden)					\
	HV_WRAPPER_SET(reg, size, iden)					\
	HV_WRAPPER_GET(reg, size, iden)					\

#define HV_ARRAY_WRAPPER_SET(reg, size, iden)				\
static inline void kvmppc_set_##reg ##_hv(struct kvm_vcpu *vcpu, int i, u##size val)	\
{									\
	vcpu->arch.reg[i] = val;					\
	kvmhv_papr_mark_dirty(vcpu, iden(i));				\
}

#define HV_ARRAY_WRAPPER_GET(reg, size, iden)				\
static inline u##size kvmppc_get_##reg ##_hv(struct kvm_vcpu *vcpu, int i)	\
{									\
	kvmhv_papr_cached_reload(vcpu, iden(i));			\
	return vcpu->arch.reg[i];					\
}

#define HV_ARRAY_WRAPPER(reg, size, iden)				\
	HV_ARRAY_WRAPPER_SET(reg, size, iden)				\
	HV_ARRAY_WRAPPER_GET(reg, size, iden)				\

HV_WRAPPER(mmcra, 64, GSID_MMCRA)
HV_WRAPPER(hfscr, 64, GSID_HFSCR)
HV_WRAPPER(fscr, 64, GSID_FSCR)
HV_WRAPPER(dscr, 64, GSID_DSCR)
HV_WRAPPER(purr, 64, GSID_PURR)
HV_WRAPPER(spurr, 64, GSID_SPURR)
HV_WRAPPER(amr, 64, GSID_AMR)
HV_WRAPPER(uamor, 64, GSID_UAMOR)
HV_WRAPPER(siar, 64, GSID_SIAR)
HV_WRAPPER(sdar, 64, GSID_SDAR)
HV_WRAPPER(iamr, 64, GSID_IAMR)
HV_WRAPPER(dawr0, 64, GSID_DAWR0)
HV_WRAPPER(dawr1, 64, GSID_DAWR1)
HV_WRAPPER(dawrx0, 64, GSID_DAWRX0)
HV_WRAPPER(dawrx1, 64, GSID_DAWRX1)
HV_WRAPPER(ciabr, 64, GSID_CIABR)
HV_WRAPPER(wort, 64, GSID_WORT)
HV_WRAPPER(ppr, 64, GSID_PPR)
HV_WRAPPER(ctrl, 64, GSID_CTRL);
HV_WRAPPER(amor, 64, GSID_AMOR)

HV_ARRAY_WRAPPER(mmcr, 64, GSID_MMCR)
HV_ARRAY_WRAPPER(sier, 64, GSID_SIER)
HV_ARRAY_WRAPPER(pmc, 32, GSID_PMC)

HV_WRAPPER(pspb, 32, GSID_PSPB)
