// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright 2023 Jordan Niethe, IBM Corp. <jniethe5@gmail.com>
 *
 * Authors:
 *    Jordan Niethe <jniethe5@gmail.com>
 *
 * Description: KVM functions specific to running on Book 3S
 * processors as a PAPR guest.
 *
 */

#include "linux/blk-mq.h"
#include "linux/console.h"
#include "linux/gfp_types.h"
#include "linux/signal.h"
#include <linux/kernel.h>
#include <linux/kvm_host.h>
#include <linux/pgtable.h>

#include <asm/kvm_ppc.h>
#include <asm/kvm_book3s.h>
#include <asm/hvcall.h>
#include <asm/pgalloc.h>
#include <asm/reg.h>
#include <asm/plpar_wrappers.h>
#include <asm/guest-state-buffer.h>
#include "trace_hv.h"

bool __kvmhv_on_papr __read_mostly;
EXPORT_SYMBOL_GPL(__kvmhv_on_papr);


static size_t gs_msg_ops_kvmhv_papr_config_get_size(struct gs_msg *gsm)
{
	u16 ids[] = {
		GSID_RUN_OUTPUT_MIN_SIZE,
		GSID_RUN_INPUT,
		GSID_RUN_OUTPUT,

	};
	size_t size = 0;

	for (int i = 0; i < ARRAY_SIZE(ids); i++)
		size += gse_total_size(gsid_size(ids[i]));
	return size;
}

static int gs_msg_ops_kvmhv_papr_config_fill_info(struct gs_buff *gsb,
						  struct gs_msg *gsm)
{
	struct kvmhv_papr_config *cfg;
	int rc;

	cfg = gsm->data;

	if (gsm_includes(gsm, GSID_RUN_OUTPUT_MIN_SIZE)) {
		rc = gse_put(gsb, GSID_RUN_OUTPUT_MIN_SIZE,
			     cfg->vcpu_run_output_size);
		if (rc < 0)
			return rc;
	}

	if (gsm_includes(gsm, GSID_RUN_INPUT)) {
		rc = gse_put(gsb, GSID_RUN_INPUT, cfg->vcpu_run_input_cfg);
		if (rc < 0)
			return rc;
	}

	if (gsm_includes(gsm, GSID_RUN_OUTPUT)) {
		gse_put(gsb, GSID_RUN_OUTPUT, cfg->vcpu_run_output_cfg);
		if (rc < 0)
			return rc;
	}

	return 0;
}

static int gs_msg_ops_kvmhv_papr_config_refresh_info(struct gs_msg *gsm,
						     struct gs_buff *gsb)
{
	struct kvmhv_papr_config *cfg;
	struct gs_parser gsp = { 0 };
	struct gs_elem *gse;
	int rc;

	cfg = gsm->data;

	rc = gse_parse(&gsp, gsb);
	if (rc < 0)
		return rc;

	gse = gsp_lookup(&gsp, GSID_RUN_OUTPUT_MIN_SIZE);
	if (gse)
		gse_get(gse, &cfg->vcpu_run_output_size);
	return 0;
}

static struct gs_msg_ops config_msg_ops = {
	.get_size = gs_msg_ops_kvmhv_papr_config_get_size,
	.fill_info = gs_msg_ops_kvmhv_papr_config_fill_info,
	.refresh_info = gs_msg_ops_kvmhv_papr_config_refresh_info,
};

static size_t gs_msg_ops_vcpu_get_size(struct gs_msg *gsm)
{
	struct gs_bitmap gsbm = { 0 };
	size_t size = 0;
	u16 iden;

	gsbm_fill(&gsbm);
	gsbm_for_each(&gsbm, iden) {
		switch (iden) {
		case GSID_HOST_STATE_SIZE:
		case GSID_RUN_OUTPUT_MIN_SIZE:
		case GSID_PARTITION_TABLE:
		case GSID_PROCESS_TABLE:
		case GSID_RUN_INPUT:
		case GSID_RUN_OUTPUT:
			break;
		default:
			size += gse_total_size(gsid_size(iden));
		}
	}
	return size;
}

static int gs_msg_ops_vcpu_fill_info(struct gs_buff *gsb, struct gs_msg *gsm)
{
	struct kvm_vcpu *vcpu;
	vector128 v;
	int rc, i;
	u16 iden;

	vcpu = gsm->data;

	gsm_for_each(gsm, iden)
	{
		rc = 0;

		if ((gsm->flags & GS_FLAGS_WIDE) !=
		    (gsid_flags(iden) & GS_FLAGS_WIDE))
			continue;

		switch (iden) {
		case GSID_DSCR:
			rc = gse_put(gsb, iden, vcpu->arch.dscr);
			break;
		case GSID_MMCRA:
			rc = gse_put(gsb, iden, vcpu->arch.mmcra);
			break;
		case GSID_HFSCR:
			rc = gse_put(gsb, iden, vcpu->arch.hfscr);
			break;
		case GSID_PURR:
			rc = gse_put(gsb, iden, vcpu->arch.purr);
			break;
		case GSID_SPURR:
			rc = gse_put(gsb, iden, vcpu->arch.spurr);
			break;
		case GSID_AMR:
			rc = gse_put(gsb, iden, vcpu->arch.amr);
			break;
		case GSID_UAMOR:
			rc = gse_put(gsb, iden, vcpu->arch.uamor);
			break;
		case GSID_SIAR:
			rc = gse_put(gsb, iden, vcpu->arch.siar);
			break;
		case GSID_SDAR:
			rc = gse_put(gsb, iden, vcpu->arch.sdar);
			break;
		case GSID_IAMR:
			rc = gse_put(gsb, iden, vcpu->arch.iamr);
			break;
		case GSID_DAWR0:
			rc = gse_put(gsb, iden, vcpu->arch.dawr0);
			break;
		case GSID_DAWR1:
			rc = gse_put(gsb, iden, vcpu->arch.dawr1);
			break;
		case GSID_DAWRX0:
			rc = gse_put(gsb, iden, vcpu->arch.dawrx0);
			break;
		case GSID_DAWRX1:
			rc = gse_put(gsb, iden, vcpu->arch.dawrx1);
			break;
		case GSID_CIABR:
			rc = gse_put(gsb, iden, vcpu->arch.ciabr);
			break;
		case GSID_WORT:
			rc = gse_put(gsb, iden, vcpu->arch.wort);
			break;
		case GSID_PPR:
			rc = gse_put(gsb, iden, vcpu->arch.ppr);
			break;
		case GSID_PSPB:
			rc = gse_put(gsb, iden, vcpu->arch.pspb);
			break;
		case GSID_TAR:
			rc = gse_put(gsb, iden, vcpu->arch.tar);
			break;
		case GSID_FSCR:
			rc = gse_put(gsb, iden, vcpu->arch.fscr);
			break;
		case GSID_EBBHR:
			rc = gse_put(gsb, iden, vcpu->arch.ebbhr);
			break;
		case GSID_EBBRR:
			rc = gse_put(gsb, iden, vcpu->arch.ebbrr);
			break;
		case GSID_BESCR:
			rc = gse_put(gsb, iden, vcpu->arch.bescr);
			break;
		case GSID_IC:
			rc = gse_put(gsb, iden, vcpu->arch.ic);
			break;
		case GSID_CTRL:
			rc = gse_put(gsb, iden, vcpu->arch.ctrl);
			break;
		case GSID_PIDR:
			rc = gse_put(gsb, iden, vcpu->arch.pid);
			break;
		case GSID_AMOR:
			rc = gse_put(gsb, iden, vcpu->arch.amor);
			break;
		case GSID_VRSAVE:
			rc = gse_put(gsb, iden, vcpu->arch.vrsave);
			break;
		case GSID_MMCR(0) ... GSID_MMCR(3):
			i = iden - GSID_MMCR(0);
			rc = gse_put(gsb, iden, vcpu->arch.mmcr[i]);
			break;
		case GSID_SIER(0) ... GSID_SIER(2):
			i = iden - GSID_SIER(0);
			rc = gse_put(gsb, iden, vcpu->arch.sier[i]);
			break;
		case GSID_PMC(0) ... GSID_PMC(5):
			i = iden - GSID_PMC(0);
			rc = gse_put(gsb, iden, vcpu->arch.pmc[i]);
			break;
		case GSID_GPR(0) ... GSID_GPR(31):
			i = iden - GSID_GPR(0);
			rc = gse_put(gsb, iden, vcpu->arch.regs.gpr[i]);
			break;
		case GSID_CR:
			rc = gse_put(gsb, iden, vcpu->arch.regs.ccr);
			break;
		case GSID_XER:
			rc = gse_put(gsb, iden, vcpu->arch.regs.xer);
			break;
		case GSID_CTR:
			rc = gse_put(gsb, iden, vcpu->arch.regs.ctr);
			break;
		case GSID_LR:
			rc = gse_put(gsb, iden, vcpu->arch.regs.link);
			break;
		case GSID_NIA:
			rc = gse_put(gsb, iden, vcpu->arch.regs.nip);
			break;
		case GSID_SRR0:
			rc = gse_put(gsb, iden, vcpu->arch.shregs.srr0);
			break;
		case GSID_SRR1:
			rc = gse_put(gsb, iden, vcpu->arch.shregs.srr1);
			break;
		case GSID_SPRG0:
			rc = gse_put(gsb, iden, vcpu->arch.shregs.sprg0);
			break;
		case GSID_SPRG1:
			rc = gse_put(gsb, iden, vcpu->arch.shregs.sprg1);
			break;
		case GSID_SPRG2:
			rc = gse_put(gsb, iden, vcpu->arch.shregs.sprg2);
			break;
		case GSID_SPRG3:
			rc = gse_put(gsb, iden, vcpu->arch.shregs.sprg3);
			break;
		case GSID_DAR:
			rc = gse_put(gsb, iden, vcpu->arch.shregs.dar);
			break;
		case GSID_DSISR:
			rc = gse_put(gsb, iden, vcpu->arch.shregs.dsisr);
			break;
		case GSID_MSR:
			rc = gse_put(gsb, iden, vcpu->arch.shregs.msr);
			break;
		case GSID_VTB:
			rc = gse_put(gsb, iden, vcpu->arch.vcore->vtb);
			break;
		case GSID_LPCR:
			rc = gse_put(gsb, iden, vcpu->arch.vcore->lpcr);
			break;
		case GSID_TB_OFFSET:
			rc = gse_put(gsb, iden, vcpu->arch.vcore->tb_offset);
			break;
		case GSID_FPSCR:
			rc = gse_put(gsb, iden, vcpu->arch.fp.fpscr);
			break;
		case GSID_VSRS(0) ... GSID_VSRS(31):
			i = iden - GSID_VSRS(0);
			memcpy(&v, &vcpu->arch.fp.fpr[i],
			       sizeof(vcpu->arch.fp.fpr[i]));
			rc = gse_put(gsb, iden, v);
			break;
#ifdef CONFIG_VSX
		case GSID_VSCR:
			rc = gse_put(gsb, iden, vcpu->arch.vr.vscr.u[3]);
			break;
		case GSID_VSRS(32) ... GSID_VSRS(63):
			i = iden - GSID_VSRS(32);
			rc = gse_put(gsb, iden, vcpu->arch.vr.vr[i]);
			break;
#endif
		case GSID_DEC_EXPIRY_TB: {
			u64 dw;

			dw = vcpu->arch.dec_expires -
			     vcpu->arch.vcore->tb_offset;
			rc = gse_put(gsb, iden, dw);
		}
			break;
		}

		if (rc < 0)
			return rc;
	}

	return 0;
}

static int gs_msg_ops_vcpu_refresh_info(struct gs_msg *gsm, struct gs_buff *gsb)
{
	struct gs_parser gsp = { 0 };
	struct kvmhv_papr_host *ph;
	struct gs_bitmap *valids;
	struct kvm_vcpu *vcpu;
	struct gs_elem *gse;
	vector128 v;
	int rc, i;
	u16 iden;

	vcpu = gsm->data;

	rc = gse_parse(&gsp, gsb);
	if (rc < 0)
		return rc;

	ph = &vcpu->arch.papr_host;
	valids = &ph->valids;

	gsp_for_each(&gsp, iden, gse)
	{
		switch (iden) {
		case GSID_DSCR:
			gse_get(gse, &vcpu->arch.dscr);
			break;
		case GSID_MMCRA:
			gse_get(gse, &vcpu->arch.mmcra);
			break;
		case GSID_HFSCR:
			gse_get(gse, &vcpu->arch.hfscr);
			break;
		case GSID_PURR:
			gse_get(gse, &vcpu->arch.purr);
			break;
		case GSID_SPURR:
			gse_get(gse, &vcpu->arch.spurr);
			break;
		case GSID_AMR:
			gse_get(gse, &vcpu->arch.amr);
			break;
		case GSID_UAMOR:
			gse_get(gse, &vcpu->arch.uamor);
			break;
		case GSID_SIAR:
			gse_get(gse, &vcpu->arch.siar);
			break;
		case GSID_SDAR:
			gse_get(gse, &vcpu->arch.sdar);
			break;
		case GSID_IAMR:
			gse_get(gse, &vcpu->arch.iamr);
			break;
		case GSID_DAWR0:
			gse_get(gse, &vcpu->arch.dawr0);
			break;
		case GSID_DAWR1:
			gse_get(gse, &vcpu->arch.dawr1);
			break;
		case GSID_DAWRX0:
			gse_get(gse, &vcpu->arch.dawrx0);
			break;
		case GSID_DAWRX1:
			gse_get(gse, &vcpu->arch.dawrx1);
			break;
		case GSID_CIABR:
			gse_get(gse, &vcpu->arch.ciabr);
			break;
		case GSID_WORT:
			gse_get(gse, &vcpu->arch.wort);
			break;
		case GSID_PPR:
			gse_get(gse, &vcpu->arch.ppr);
			break;
		case GSID_PSPB:
			gse_get(gse, &vcpu->arch.pspb);
			break;
		case GSID_TAR:
			gse_get(gse, &vcpu->arch.tar);
			break;
		case GSID_FSCR:
			gse_get(gse, &vcpu->arch.fscr);
			break;
		case GSID_EBBHR:
			gse_get(gse, &vcpu->arch.ebbhr);
			break;
		case GSID_EBBRR:
			gse_get(gse, &vcpu->arch.ebbrr);
			break;
		case GSID_BESCR:
			gse_get(gse, &vcpu->arch.bescr);
			break;
		case GSID_IC:
			gse_get(gse, &vcpu->arch.ic);
			break;
		case GSID_CTRL:
			gse_get(gse, &vcpu->arch.ctrl);
			break;
		case GSID_PIDR:
			gse_get(gse, &vcpu->arch.pid);
			break;
		case GSID_AMOR:
			gse_get(gse, &vcpu->arch.amor);
			break;
		case GSID_VRSAVE:
			gse_get(gse, &vcpu->arch.vrsave);
			break;
		case GSID_MMCR(0) ... GSID_MMCR(3):
			i = iden - GSID_MMCR(0);
			gse_get(gse, &vcpu->arch.mmcr[i]);
			break;
		case GSID_SIER(0) ... GSID_SIER(2):
			i = iden - GSID_SIER(0);
			gse_get(gse, &vcpu->arch.sier[i]);
			break;
		case GSID_PMC(0) ... GSID_PMC(5):
			i = iden - GSID_PMC(0);
			gse_get(gse, &vcpu->arch.pmc[i]);
			break;
		case GSID_GPR(0) ... GSID_GPR(31):
			i = iden - GSID_GPR(0);
			gse_get(gse, &vcpu->arch.regs.gpr[i]);
			break;
		case GSID_CR:
			gse_get(gse, &vcpu->arch.regs.ccr);
			break;
		case GSID_XER:
			gse_get(gse, &vcpu->arch.regs.xer);
			break;
		case GSID_CTR:
			gse_get(gse, &vcpu->arch.regs.ctr);
			break;
		case GSID_LR:
			gse_get(gse, &vcpu->arch.regs.link);
			break;
		case GSID_NIA:
			gse_get(gse, &vcpu->arch.regs.nip);
			break;
		case GSID_SRR0:
			gse_get(gse, &vcpu->arch.shregs.srr0);
			break;
		case GSID_SRR1:
			gse_get(gse, &vcpu->arch.shregs.srr1);
			break;
		case GSID_SPRG0:
			gse_get(gse, &vcpu->arch.shregs.sprg0);
			break;
		case GSID_SPRG1:
			gse_get(gse, &vcpu->arch.shregs.sprg1);
			break;
		case GSID_SPRG2:
			gse_get(gse, &vcpu->arch.shregs.sprg2);
			break;
		case GSID_SPRG3:
			gse_get(gse, &vcpu->arch.shregs.sprg3);
			break;
		case GSID_DAR:
			gse_get(gse, &vcpu->arch.shregs.dar);
			break;
		case GSID_DSISR:
			gse_get(gse, &vcpu->arch.shregs.dsisr);
			break;
		case GSID_MSR:
			gse_get(gse, &vcpu->arch.shregs.msr);
			break;
		case GSID_VTB:
			gse_get(gse, &vcpu->arch.vcore->vtb);
			break;
		case GSID_LPCR:
			gse_get(gse, &vcpu->arch.vcore->lpcr);
			break;
		case GSID_TB_OFFSET:
			gse_get(gse, &vcpu->arch.vcore->tb_offset);
			break;
		case GSID_FPSCR:
			gse_get(gse, &vcpu->arch.fp.fpscr);
			break;
		case GSID_VSRS(0) ... GSID_VSRS(31):
			gse_get(gse, &v);
			i = iden - GSID_VSRS(0);
			memcpy(&vcpu->arch.fp.fpr[i], &v,
			       sizeof(vcpu->arch.fp.fpr[i]));
			break;
#ifdef CONFIG_VSX
		case GSID_VSCR:
			gse_get(gse, &vcpu->arch.vr.vscr.u[3]);
			break;
		case GSID_VSRS(32) ... GSID_VSRS(63):
			i = iden - GSID_VSRS(32);
			gse_get(gse, &vcpu->arch.vr.vr[i]);
			break;
#endif
		case GSID_HDAR:
			gse_get(gse, &vcpu->arch.fault_dar);
			break;
		case GSID_HDSISR:
			gse_get(gse, &vcpu->arch.fault_dsisr);
			break;
		case GSID_ASDR:
			gse_get(gse, &vcpu->arch.fault_gpa);
			break;
		case GSID_HEIR:
			gse_get(gse, &vcpu->arch.emul_inst);
			break;
		case GSID_DEC_EXPIRY_TB: {
			u64 dw;

			gse_get(gse, &dw);
			vcpu->arch.dec_expires =
				dw + vcpu->arch.vcore->tb_offset;
			break;
		}
		default:
			continue;
		}
		gsbm_set(valids, iden);
	}

	return 0;
}

static struct gs_msg_ops vcpu_message_ops = {
	.get_size = gs_msg_ops_vcpu_get_size,
	.fill_info = gs_msg_ops_vcpu_fill_info,
	.refresh_info = gs_msg_ops_vcpu_refresh_info,
};

static int kvmhv_papr_host_create(struct kvm_vcpu *vcpu,
				  struct kvmhv_papr_host *ph)
{
	struct kvmhv_papr_config *cfg;
	struct gs_buff *gsb, *vcpu_run_output, *vcpu_run_input;
	unsigned long guest_id, vcpu_id;
	struct gs_msg *gsm, *vcpu_message, *vcore_message;
	int rc;

	cfg = &ph->cfg;
	guest_id = vcpu->kvm->arch.lpid;
	vcpu_id = vcpu->vcpu_id;

	gsm = gsm_new(&config_msg_ops, cfg, GS_FLAGS_WIDE, GFP_KERNEL);
	if (!gsm) {
		rc = -ENOMEM;
		goto err;
	}

	gsb = gsb_new(gsm_size(gsm), guest_id, vcpu_id, GFP_KERNEL);
	if (!gsb) {
		rc = -ENOMEM;
		goto free_gsm;
	}

	rc = gsb_receive_datum(gsb, gsm, GSID_RUN_OUTPUT_MIN_SIZE);
	if (rc < 0) {
		pr_err("KVM-PAPR: couldn't get vcpu run output buffer minimum size\n");
		goto free_gsb;
	}

	vcpu_run_output = gsb_new(cfg->vcpu_run_output_size, guest_id, vcpu_id, GFP_KERNEL);
	if (!vcpu_run_output) {
		rc = -ENOMEM;
		goto free_gsb;
	}

	cfg->vcpu_run_output_cfg.address = gsb_paddress(vcpu_run_output);
	cfg->vcpu_run_output_cfg.size = gsb_capacity(vcpu_run_output);
	ph->vcpu_run_output = vcpu_run_output;

	gsm->flags = 0;
	rc = gsb_send_datum(gsb, gsm, GSID_RUN_OUTPUT);
	if (rc < 0) {
		pr_err("KVM-PAPR: couldn't set vcpu run output buffer\n");
		goto free_gs_out;
	}

	vcpu_message = gsm_new(&vcpu_message_ops, vcpu, 0, GFP_KERNEL);
	if (!vcpu_message) {
		rc = -ENOMEM;
		goto free_gs_out;
	}
	gsm_include_all(vcpu_message);

	ph->vcpu_message = vcpu_message;

	vcpu_run_input = gsb_new(gsm_size(vcpu_message), guest_id, vcpu_id, GFP_KERNEL);
	if (!vcpu_run_input) {
		rc = -ENOMEM;
		goto free_vcpu_message;
	}

	ph->vcpu_run_input = vcpu_run_input;
	cfg->vcpu_run_input_cfg.address = gsb_paddress(vcpu_run_input);
	cfg->vcpu_run_input_cfg.size = gsb_capacity(vcpu_run_input);
	rc = gsb_send_datum(gsb, gsm, GSID_RUN_INPUT);
	if (rc < 0) {
		pr_err("KVM-PAPR: couldn't set vcpu run input buffer\n");
		goto free_vcpu_run_input;
	}

	vcore_message =
		gsm_new(&vcpu_message_ops, vcpu, GS_FLAGS_WIDE, GFP_KERNEL);
	if (!vcore_message) {
		rc = -ENOMEM;
		goto free_vcpu_run_input;
	}

	gsm_include_all(vcore_message);
	ph->vcore_message = vcore_message;

	gsbm_fill(&ph->valids);
	gsm_free(gsm);
	gsb_free(gsb);
	return 0;

free_vcpu_run_input:
	gsb_free(vcpu_run_input);
free_vcpu_message:
	gsm_free(vcpu_message);
free_gs_out:
	gsb_free(vcpu_run_output);
free_gsb:
	gsb_free(gsb);
free_gsm:
	gsm_free(gsm);
err:
	return rc;
}

/**
 * __kvmhv_papr_mark_dirty() - mark a Guest State ID to be sent to the host
 * @vcpu: vcpu
 * @iden: guest state ID
 *
 * Mark a guest state ID as having been changed by the L1 host and thus
 * the new value must be sent to the L0 hypervisor. See kvmhv_papr_flush_vcpu()
 */
int __kvmhv_papr_mark_dirty(struct kvm_vcpu *vcpu, u16 iden)
{
	struct kvmhv_papr_host *ph;
	struct gs_bitmap *valids;
	struct gs_msg *gsm;

	if (!iden)
		return 0;

	ph = &vcpu->arch.papr_host;
	valids = &ph->valids;
	gsm = ph->vcpu_message;
	gsm_include(gsm, iden);
	gsm = ph->vcore_message;
	gsm_include(gsm, iden);
	gsbm_set(valids, iden);
	return 0;
}
EXPORT_SYMBOL_GPL(__kvmhv_papr_mark_dirty);

/**
 * __kvmhv_papr_cached_reload() - reload a Guest State ID from the host
 * @vcpu: vcpu
 * @iden: guest state ID
 *
 * Reload the value for the guest state ID from the L0 host into the L1 host.
 * This is cached so that going out to the L0 host only happens if necessary.
 */
int __kvmhv_papr_cached_reload(struct kvm_vcpu *vcpu, u16 iden)
{
	struct kvmhv_papr_host *ph;
	struct gs_bitmap *valids;
	struct gs_buff *gsb;
	struct gs_msg gsm;
	int rc;

	if (!iden)
		return 0;

	ph = &vcpu->arch.papr_host;
	valids = &ph->valids;
	if (gsbm_test(valids, iden))
		return 0;

	gsb = ph->vcpu_run_input;
	gsm_init(&gsm, &vcpu_message_ops, vcpu, gsid_flags(iden));
	rc = gsb_receive_datum(gsb, &gsm, iden);
	if (rc < 0) {
		pr_err("KVM-PAPR: couldn't get GSID: 0x%x\n", iden);
		return rc;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(__kvmhv_papr_cached_reload);

/**
 * kvmhv_papr_flush_vcpu() - send modified Guest State IDs to the host
 * @vcpu: vcpu
 * @time_limit: hdec expiry tb
 *
 * Send the values marked by __kvmhv_papr_mark_dirty() to the L0 host. Thread
 * wide values are copied to the H_GUEST_RUN_VCPU input buffer. Guest wide
 * values need to be sent with H_GUEST_SET first.
 *
 * The hdec tb offset is always sent to L0 host.
 */
int kvmhv_papr_flush_vcpu(struct kvm_vcpu *vcpu, u64 time_limit)
{
	struct kvmhv_papr_host *ph;
	struct gs_buff *gsb;
	struct gs_msg *gsm;
	int rc;

	ph = &vcpu->arch.papr_host;
	gsb = ph->vcpu_run_input;
	gsm = ph->vcore_message;
	rc = gsb_send_data(gsb, gsm);
	if (rc < 0) {
		pr_err("KVM-PAPR: couldn't set guest wide elements\n");
		return rc;
	}

	gsm = ph->vcpu_message;
	rc = gsm_fill_info(gsm, gsb);
	if (rc < 0) {
		pr_err("KVM-PAPR: couldn't fill vcpu run input buffer\n");
		return rc;
	}

	rc = gse_put(gsb, GSID_HDEC_EXPIRY_TB, time_limit);
	if (rc < 0)
		return rc;
	return 0;
}
EXPORT_SYMBOL_GPL(kvmhv_papr_flush_vcpu);


/**
 * kvmhv_papr_set_ptbl_entry() - send partition and process table state to L0 host
 * @lpid: guest id
 * @dw0: partition table double word
 * @dw1: process table double word
 */
int kvmhv_papr_set_ptbl_entry(u64 lpid, u64 dw0, u64 dw1)
{
	struct gs_part_table patbl;
	struct gs_proc_table prtbl;
	struct gs_buff *gsb;
	size_t size;
	int rc;

	size = gse_total_size(gsid_size(GSID_PARTITION_TABLE)) +
	       gse_total_size(gsid_size(GSID_PROCESS_TABLE)) +
	       sizeof(struct gs_header);
	gsb = gsb_new(size, lpid, 0, GFP_KERNEL);
	if (!gsb)
		return -ENOMEM;

	patbl.address = dw0 & RPDB_MASK;
	patbl.ea_bits = ((((dw0 & RTS1_MASK) >> (RTS1_SHIFT - 3)) |
			  ((dw0 & RTS2_MASK) >> RTS2_SHIFT)) +
			 31);
	patbl.gpd_size = 1ul << ((dw0 & RPDS_MASK) + 3);
	rc = gse_put(gsb, GSID_PARTITION_TABLE, patbl);
	if (rc < 0)
		goto free_gsb;

	prtbl.address = dw1 & PRTB_MASK;
	prtbl.gpd_size = 1ul << ((dw1 & PRTS_MASK) + 12);
	rc = gse_put(gsb, GSID_PROCESS_TABLE, prtbl);
	if (rc < 0)
		goto free_gsb;

	rc = gsb_send(gsb, GS_FLAGS_WIDE);
	if (rc < 0) {
		pr_err("KVM-PAPR: couldn't set the PATE\n");
		goto free_gsb;
	}

	gsb_free(gsb);
	return 0;

free_gsb:
	gsb_free(gsb);
	return rc;
}
EXPORT_SYMBOL_GPL(kvmhv_papr_set_ptbl_entry);

/**
 * kvmhv_papr_parse_output() - receive values from H_GUEST_RUN_VCPU output
 * @vcpu: vcpu
 *
 * Parse the output buffer from H_GUEST_RUN_VCPU to update vcpu.
 */
int kvmhv_papr_parse_output(struct kvm_vcpu *vcpu)
{
	struct kvmhv_papr_host *ph;
	struct gs_buff *gsb;
	struct gs_msg gsm;

	ph = &vcpu->arch.papr_host;
	gsb = ph->vcpu_run_output;

	vcpu->arch.fault_dar = 0;
	vcpu->arch.fault_dsisr = 0;
	vcpu->arch.fault_gpa = 0;
	vcpu->arch.emul_inst = KVM_INST_FETCH_FAILED;

	gsm_init(&gsm, &vcpu_message_ops, vcpu, 0);
	gsm_refresh_info(&gsm, gsb);

	return 0;
}
EXPORT_SYMBOL_GPL(kvmhv_papr_parse_output);

static void kvmhv_papr_host_free(struct kvm_vcpu *vcpu,
				 struct kvmhv_papr_host *ph)
{
	gsm_free(ph->vcpu_message);
	gsm_free(ph->vcore_message);
	gsb_free(ph->vcpu_run_input);
	gsb_free(ph->vcpu_run_output);
}

int __kvmhv_papr_reload_ptregs(struct kvm_vcpu *vcpu, struct pt_regs *regs)
{
	int rc;

	for (int i = 0; i < 32; i++) {
		rc = kvmhv_papr_cached_reload(vcpu, GSID_GPR(i));
		if (rc < 0)
			return rc;
	}

	rc = kvmhv_papr_cached_reload(vcpu, GSID_CR);
	if (rc < 0)
		return rc;
	rc = kvmhv_papr_cached_reload(vcpu, GSID_XER);
	if (rc < 0)
		return rc;
	rc = kvmhv_papr_cached_reload(vcpu, GSID_CTR);
	if (rc < 0)
		return rc;
	rc = kvmhv_papr_cached_reload(vcpu, GSID_LR);
	if (rc < 0)
		return rc;
	rc = kvmhv_papr_cached_reload(vcpu, GSID_NIA);
	if (rc < 0)
		return rc;

	return 0;
}
EXPORT_SYMBOL_GPL(__kvmhv_papr_reload_ptregs);

int __kvmhv_papr_mark_dirty_ptregs(struct kvm_vcpu *vcpu, struct pt_regs *regs)
{
	for (int i = 0; i < 32; i++)
		kvmhv_papr_mark_dirty(vcpu, GSID_GPR(i));

	kvmhv_papr_mark_dirty(vcpu, GSID_CR);
	kvmhv_papr_mark_dirty(vcpu, GSID_XER);
	kvmhv_papr_mark_dirty(vcpu, GSID_CTR);
	kvmhv_papr_mark_dirty(vcpu, GSID_LR);
	kvmhv_papr_mark_dirty(vcpu, GSID_NIA);

	return 0;
}
EXPORT_SYMBOL_GPL(__kvmhv_papr_mark_dirty_ptregs);

/**
 * kvmhv_papr_vcpu_create() - create nested vcpu for the PAPR API
 * @vcpu: vcpu
 * @ph: PAPR nested host state
 *
 * Parse the output buffer from H_GUEST_RUN_VCPU to update vcpu.
 */
int kvmhv_papr_vcpu_create(struct kvm_vcpu *vcpu,
			   struct kvmhv_papr_host *ph)
{
	long rc;

	rc = plpar_guest_create_vcpu(0, vcpu->kvm->arch.lpid, vcpu->vcpu_id);

	if (rc != H_SUCCESS) {
		pr_err("KVM: Create Guest vcpu hcall failed, rc=%ld\n", rc);
		switch (rc) {
		case H_NOT_ENOUGH_RESOURCES:
		case H_ABORTED:
			return -ENOMEM;
		case H_AUTHORITY:
			return -EPERM;
		default:
			return -EINVAL;
		}
	}

	rc = kvmhv_papr_host_create(vcpu, ph);

	return rc;
}
EXPORT_SYMBOL_GPL(kvmhv_papr_vcpu_create);

/**
 * kvmhv_papr_vcpu_free() - free the PAPR host state
 * @vcpu: vcpu
 * @ph: PAPR nested host state
 */
void kvmhv_papr_vcpu_free(struct kvm_vcpu *vcpu,
			  struct kvmhv_papr_host *ph)
{
	kvmhv_papr_host_free(vcpu, ph);
}
EXPORT_SYMBOL_GPL(kvmhv_papr_vcpu_free);
