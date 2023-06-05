// SPDX-License-Identifier: GPL-2.0

#include "asm/hvcall.h"
#include <linux/log2.h>
#include <asm/pgalloc.h>
#include <asm/guest-state-buffer.h>

static const u16 gse_iden_len[__GSE_TYPE_MAX] = {
	[GSE_BE32] = sizeof(__be32),
	[GSE_BE64] = sizeof(__be64),
	[GSE_VEC128] = sizeof(vector128),
	[GSE_PARTITION_TABLE] = sizeof(struct gs_part_table),
	[GSE_PROCESS_TABLE] = sizeof(struct gs_proc_table),
	[GSE_BUFFER] = sizeof(struct gs_buff_info),
};

/**
 * gsb_new() - create a new guest state buffer
 * @size: total size of the guest state buffer (includes header)
 * @guest_id: guest_id
 * @vcpu_id: vcpu_id
 * @flags: GFP flags
 *
 * Returns a guest state buffer.
 */
struct gs_buff *gsb_new(size_t size, unsigned long guest_id,
			unsigned long vcpu_id, gfp_t flags)
{
	struct gs_buff *gsb;

	gsb = kzalloc(sizeof(*gsb), flags);
	if (!gsb)
		return NULL;

	size = roundup_pow_of_two(size);
	gsb->hdr = kzalloc(size, GFP_KERNEL);
	if (!gsb->hdr)
		goto free;

	gsb->capacity = size;
	gsb->len = sizeof(struct gs_header);
	gsb->vcpu_id = vcpu_id;
	gsb->guest_id = guest_id;

	gsb->hdr->nelems = cpu_to_be32(0);

	return gsb;

free:
	kfree(gsb);
	return NULL;
}
EXPORT_SYMBOL(gsb_new);

/**
 * gsb_free() - free a guest state buffer
 * @gsb: guest state buffer
 */
void gsb_free(struct gs_buff *gsb)
{
	kfree(gsb->hdr);
	kfree(gsb);
}
EXPORT_SYMBOL(gsb_free);

/**
 * gsb_put() - allocate space in a guest state buffer
 * @gsb: buffer to allocate in
 * @size: amount of space to allocate
 *
 * Returns a pointer to the amount of space requested within the buffer and
 * increments the count of elements in the buffer.
 *
 * Does not check if there is enough space in the buffer.
 */
void *gsb_put(struct gs_buff *gsb, size_t size)
{
	u32 nelems = gsb_nelems(gsb);
	void *p;

	p = (void *)gsb_header(gsb) + gsb_len(gsb);
	gsb->len += size;

	gsb_header(gsb)->nelems = cpu_to_be32(nelems + 1);
	return p;
}
EXPORT_SYMBOL(gsb_put);

static int gsid_class(u16 iden)
{
	if ((iden >= GSE_GUESTWIDE_START) && (iden <= GSE_GUESTWIDE_END))
		return GS_CLASS_GUESTWIDE;

	if ((iden >= GSE_META_START) && (iden <= GSE_META_END))
		return GS_CLASS_META;

	if ((iden >= GSE_DW_REGS_START) && (iden <= GSE_DW_REGS_END))
		return GS_CLASS_DWORD_REG;

	if ((iden >= GSE_W_REGS_START) && (iden <= GSE_W_REGS_END))
		return GS_CLASS_WORD_REG;

	if ((iden >= GSE_VSRS_START) && (iden <= GSE_VSRS_END))
		return GS_CLASS_VECTOR;

	if ((iden >= GSE_INTR_REGS_START) && (iden <= GSE_INTR_REGS_END))
		return GS_CLASS_INTR;

	return -1;
}

static int gsid_type(u16 iden)
{
	int type = -1;

	switch (gsid_class(iden)) {
	case GS_CLASS_GUESTWIDE:
		switch (iden) {
		case GSID_HOST_STATE_SIZE:
		case GSID_RUN_OUTPUT_MIN_SIZE:
		case GSID_TB_OFFSET:
			type = GSE_BE64;
			break;
		case GSID_PARTITION_TABLE:
			type = GSE_PARTITION_TABLE;
			break;
		case GSID_PROCESS_TABLE:
			type = GSE_PROCESS_TABLE;
			break;
		case GSID_LOGICAL_PVR:
			type = GSE_BE32;
			break;
		}
		break;
	case GS_CLASS_META:
		switch (iden) {
		case GSID_RUN_INPUT:
		case GSID_RUN_OUTPUT:
			type = GSE_BUFFER;
			break;
		case GSID_VPA:
			type = GSE_BE64;
			break;
		}
		break;
	case GS_CLASS_DWORD_REG:
		type = GSE_BE64;
		break;
	case GS_CLASS_WORD_REG:
		type = GSE_BE32;
		break;
	case GS_CLASS_VECTOR:
		type = GSE_VEC128;
		break;
	case GS_CLASS_INTR:
		switch (iden) {
		case GSID_HDAR:
		case GSID_ASDR:
		case GSID_HEIR:
			type = GSE_BE64;
			break;
		case GSID_HDSISR:
			type = GSE_BE32;
			break;
		}
		break;
	}

	return type;
}

/**
 * gsid_flags() - the flags for a guest state ID
 * @iden: guest state ID
 *
 * Returns any flags for the guest state ID.
 */
unsigned long gsid_flags(u16 iden)
{
	unsigned long flags = 0;

	switch (gsid_class(iden)) {
	case GS_CLASS_GUESTWIDE:
		flags = GS_FLAGS_WIDE;
		break;
	case GS_CLASS_META:
	case GS_CLASS_DWORD_REG:
	case GS_CLASS_WORD_REG:
	case GS_CLASS_VECTOR:
	case GS_CLASS_INTR:
		break;
	}

	return flags;
}
EXPORT_SYMBOL(gsid_flags);

/**
 * gsid_size() - the size of a guest state ID
 * @iden: guest state ID
 *
 * Returns the size of guest state ID.
 */
u16 gsid_size(u16 iden)
{
	int type;

	type = gsid_type(iden);
	if (type == -1)
		return 0;

	if (type >= __GSE_TYPE_MAX)
		return 0;

	return gse_iden_len[type];
}
EXPORT_SYMBOL(gsid_size);

/**
 * gsid_mask() - the settable bits of a guest state ID
 * @iden: guest state ID
 *
 * Returns a mask of settable bits for a guest state ID.
 */
u64 gsid_mask(u16 iden)
{
	u64 mask = ~0ull;

	switch (iden) {
	case GSID_LPCR:
		mask = LPCR_DPFD | LPCR_ILE | LPCR_AIL | LPCR_LD | LPCR_MER | LPCR_GTSE;
		break;
	case GSID_MSR:
		mask = ~(MSR_HV | MSR_S | MSR_ME);
		break;
	}

	return mask;
}
EXPORT_SYMBOL(gsid_mask);

/**
 * __gse_put() - add a guest state element to a buffer
 * @gsb: buffer to the element to
 * @iden: guest state ID
 * @size: length of data
 * @data: pointer to data
 */
int __gse_put(struct gs_buff *gsb, u16 iden, u16 size, const void *data)
{
	struct gs_elem *gse;
	u16 total_size;

	total_size = sizeof(*gse) + size;
	if (total_size + gsb_len(gsb) > gsb_capacity(gsb))
		return -ENOMEM;

	if (gsid_size(iden) != size)
		return -EINVAL;

	gse = gsb_put(gsb, total_size);
	gse->iden = cpu_to_be16(iden);
	gse->len = cpu_to_be16(size);
	memcpy(gse->data, data, size);

	return 0;
}
EXPORT_SYMBOL(__gse_put);

/**
 * gse_parse() - create a parse map from a guest state buffer
 * @gsp: guest state parser
 * @gsb: guest state buffer
 */
int gse_parse(struct gs_parser *gsp, struct gs_buff *gsb)
{
	struct gs_elem *curr;
	int rem, i;

	gsb_for_each_elem(i, curr, gsb, rem) {
		if (gse_len(curr) != gsid_size(gse_iden(curr)))
			return -EINVAL;
		gsp_insert(gsp, gse_iden(curr), curr);
	}

	if (gsb_nelems(gsb) != i)
		return -EINVAL;
	return 0;
}
EXPORT_SYMBOL(gse_parse);

static inline int gse_flatten_iden(u16 iden)
{
	int bit = 0;
	int class;

	class = gsid_class(iden);

	if (class == GS_CLASS_GUESTWIDE) {
		bit += iden - GSE_GUESTWIDE_START;
		return bit;
	}

	bit += GSE_GUESTWIDE_COUNT;

	if (class == GS_CLASS_META) {
		bit += iden - GSE_META_START;
		return bit;
	}

	bit += GSE_META_COUNT;

	if (class == GS_CLASS_DWORD_REG) {
		bit += iden - GSE_DW_REGS_START;
		return bit;
	}

	bit += GSE_DW_REGS_COUNT;

	if (class == GS_CLASS_WORD_REG) {
		bit += iden - GSE_W_REGS_START;
		return bit;
	}

	bit += GSE_W_REGS_COUNT;

	if (class == GS_CLASS_VECTOR) {
		bit += iden - GSE_VSRS_START;
		return bit;
	}

	bit += GSE_VSRS_COUNT;

	if (class == GS_CLASS_INTR) {
		bit += iden - GSE_INTR_REGS_START;
		return bit;
	}

	return 0;
}

static inline u16 gse_unflatten_iden(int bit)
{
	u16 iden;

	if (bit < GSE_GUESTWIDE_COUNT) {
		iden = GSE_GUESTWIDE_START + bit;
		return iden;
	}
	bit -= GSE_GUESTWIDE_COUNT;

	if (bit < GSE_META_COUNT) {
		iden = GSE_META_START + bit;
		return iden;
	}
	bit -= GSE_META_COUNT;

	if (bit < GSE_DW_REGS_COUNT) {
		iden = GSE_DW_REGS_START + bit;
		return iden;
	}
	bit -= GSE_DW_REGS_COUNT;

	if (bit < GSE_W_REGS_COUNT) {
		iden = GSE_W_REGS_START + bit;
		return iden;
	}
	bit -= GSE_W_REGS_COUNT;

	if (bit < GSE_VSRS_COUNT) {
		iden = GSE_VSRS_START + bit;
		return iden;
	}
	bit -= GSE_VSRS_COUNT;

	if (bit < GSE_IDEN_COUNT) {
		iden = GSE_INTR_REGS_START + bit;
		return iden;
	}

	return 0;
}

/**
 * gsp_insert() - add a mapping from an guest state ID to an element
 * @gsp: guest state parser
 * @iden: guest state id (key)
 * @gse: guest state element (value)
 */
void gsp_insert(struct gs_parser *gsp, u16 iden, struct gs_elem *gse)
{
	int i;

	i = gse_flatten_iden(iden);
	gsbm_set(&gsp->iterator, iden);
	gsp->gses[i] = gse;
}
EXPORT_SYMBOL(gsp_insert);

/**
 * gsp_lookup() - lookup an element from a guest state ID
 * @gsp: guest state parser
 * @iden: guest state ID (key)
 *
 * Returns the guest state element if present.
 */
struct gs_elem *gsp_lookup(struct gs_parser *gsp, u16 iden)
{
	int i;

	i = gse_flatten_iden(iden);
	return gsp->gses[i];
}
EXPORT_SYMBOL(gsp_lookup);

/**
 * gsbm_set() - set the guest state ID
 * @gsbm: guest state bitmap
 * @iden: guest state ID
 */
void gsbm_set(struct gs_bitmap *gsbm, u16 iden)
{
	set_bit(gse_flatten_iden(iden), gsbm->bitmap);
}
EXPORT_SYMBOL(gsbm_set);

/**
 * gsbm_clear() - clear the guest state ID
 * @gsbm: guest state bitmap
 * @iden: guest state ID
 */
void gsbm_clear(struct gs_bitmap *gsbm, u16 iden)
{
	clear_bit(gse_flatten_iden(iden), gsbm->bitmap);
}
EXPORT_SYMBOL(gsbm_clear);

/**
 * gsbm_test() - test the guest state ID
 * @gsbm: guest state bitmap
 * @iden: guest state ID
 */
bool gsbm_test(struct gs_bitmap *gsbm, u16 iden)
{
	return test_bit(gse_flatten_iden(iden), gsbm->bitmap);
}
EXPORT_SYMBOL(gsbm_test);

/**
 * gsbm_next() - return the next set guest state ID
 * @gsbm: guest state bitmap
 * @prev: last guest state ID
 */
u16 gsbm_next(struct gs_bitmap *gsbm, u16 prev)
{
	int bit, pbit;

	pbit = prev ? gse_flatten_iden(prev) + 1 : 0;
	bit = find_next_bit(gsbm->bitmap, GSE_IDEN_COUNT, pbit);

	if (bit < GSE_IDEN_COUNT)
		return gse_unflatten_iden(bit);
	return 0;
}
EXPORT_SYMBOL(gsbm_next);

/**
 * gsm_init() - initialize a guest state message
 * @gsm: guest state message
 * @ops: callbacks
 * @data: private data
 * @flags: guest wide or thread wide
 */
int gsm_init(struct gs_msg *gsm, struct gs_msg_ops *ops, void *data,
	     unsigned long flags)
{
	memset(gsm, 0, sizeof(*gsm));
	gsm->ops = ops;
	gsm->data = data;
	gsm->flags = flags;

	return 0;
}
EXPORT_SYMBOL(gsm_init);

/**
 * gsm_init() - creates a new guest state message
 * @ops: callbacks
 * @data: private data
 * @flags: guest wide or thread wide
 * @gfp_flags: GFP allocation flags
 *
 * Returns an initialized guest state message.
 */
struct gs_msg *gsm_new(struct gs_msg_ops *ops, void *data, unsigned long flags,
		       gfp_t gfp_flags)
{
	struct gs_msg *gsm;

	gsm = kzalloc(sizeof(*gsm), gfp_flags);
	if (!gsm)
		return NULL;

	gsm_init(gsm, ops, data, flags);

	return gsm;
}
EXPORT_SYMBOL(gsm_new);

/**
 * gsm_size() - creates a new guest state message
 * @gsm: self
 *
 * Returns the size required for the message.
 */
size_t gsm_size(struct gs_msg *gsm)
{
	if (gsm->ops->get_size)
		return gsm->ops->get_size(gsm);
	return 0;
}
EXPORT_SYMBOL(gsm_size);

/**
 * gsm_free() - free guest state message
 * @gsm: guest state message
 *
 * Returns the size required for the message.
 */
void gsm_free(struct gs_msg *gsm)
{
	kfree(gsm);
}
EXPORT_SYMBOL(gsm_free);

/**
 * gsm_fill_info() - serialises message to guest state buffer format
 * @gsm: self
 * @gsb: buffer to serialise into
 */
int gsm_fill_info(struct gs_msg *gsm, struct gs_buff *gsb)
{
	if (!gsm->ops->fill_info)
		return -EINVAL;

	gsb_reset(gsb);
	return gsm->ops->fill_info(gsb, gsm);
}
EXPORT_SYMBOL(gsm_fill_info);

/**
 * gsm_fill_info() - deserialises from guest state buffer
 * @gsm: self
 * @gsb: buffer to serialise from
 */
int gsm_refresh_info(struct gs_msg *gsm, struct gs_buff *gsb)
{
	if (!gsm->ops->fill_info)
		return -EINVAL;

	return gsm->ops->refresh_info(gsm, gsb);
}
EXPORT_SYMBOL(gsm_refresh_info);
