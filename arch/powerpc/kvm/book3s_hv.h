// SPDX-License-Identifier: GPL-2.0-only

/*
 * Privileged (non-hypervisor) host registers to save.
 */
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

#define HV_WRAPPER_SET(reg, size)					\
static inline void kvmppc_set_##reg ##_hv(struct kvm_vcpu *vcpu, u##size val)	\
{									\
	vcpu->arch.reg = val;						\
}

#define HV_WRAPPER_GET(reg, size)					\
static inline u##size kvmppc_get_##reg ##_hv(struct kvm_vcpu *vcpu)	\
{									\
	return vcpu->arch.reg;						\
}

#define HV_WRAPPER(reg, size)						\
	HV_WRAPPER_SET(reg, size)					\
	HV_WRAPPER_GET(reg, size)					\

#define HV_ARRAY_WRAPPER_SET(reg, size)					\
static inline void kvmppc_set_##reg ##_hv(struct kvm_vcpu *vcpu, int i, u##size val)	\
{									\
	vcpu->arch.reg[i] = val;					\
}

#define HV_ARRAY_WRAPPER_GET(reg, size)					\
static inline u##size kvmppc_get_##reg ##_hv(struct kvm_vcpu *vcpu, int i)	\
{									\
	return vcpu->arch.reg[i];					\
}

#define HV_ARRAY_WRAPPER(reg, size)					\
	HV_ARRAY_WRAPPER_SET(reg, size)					\
	HV_ARRAY_WRAPPER_GET(reg, size)					\

HV_WRAPPER(mmcra, 64)
HV_WRAPPER(hfscr, 64)
HV_WRAPPER(fscr, 64)
HV_WRAPPER(dscr, 64)
HV_WRAPPER(purr, 64)
HV_WRAPPER(spurr, 64)
HV_WRAPPER(amr, 64)
HV_WRAPPER(uamor, 64)
HV_WRAPPER(siar, 64)
HV_WRAPPER(sdar, 64)
HV_WRAPPER(iamr, 64)
HV_WRAPPER(dawr0, 64)
HV_WRAPPER(dawr1, 64)
HV_WRAPPER(dawrx0, 64)
HV_WRAPPER(dawrx1, 64)
HV_WRAPPER(ciabr, 64)
HV_WRAPPER(wort, 64)
HV_WRAPPER(ppr, 64)
HV_WRAPPER(ctrl, 64)

HV_ARRAY_WRAPPER(mmcr, 64)
HV_ARRAY_WRAPPER(sier, 64)
HV_ARRAY_WRAPPER(pmc, 32)

HV_WRAPPER(pvr, 32)
HV_WRAPPER(pspb, 32)
