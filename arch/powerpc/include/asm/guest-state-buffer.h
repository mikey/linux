/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Interface based on include/net/netlink.h
 */
#ifndef _ASM_POWERPC_GUEST_STATE_BUFFER_H
#define _ASM_POWERPC_GUEST_STATE_BUFFER_H

#include "asm/hvcall.h"
#include <linux/gfp.h>
#include <linux/bitmap.h>
#include <asm/plpar_wrappers.h>

/**************************************************************************
 * Guest State Buffer Constants
 **************************************************************************/
#define GSID_BLANK			0x0000

#define GSID_HOST_STATE_SIZE		0x0001
#define GSID_RUN_OUTPUT_MIN_SIZE	0x0002
#define GSID_LOGICAL_PVR		0x0003
#define GSID_TB_OFFSET			0x0004
#define GSID_PARTITION_TABLE		0x0005
#define GSID_PROCESS_TABLE		0x0006

#define GSID_RUN_INPUT			0x0C00
#define GSID_RUN_OUTPUT			0x0C01
#define GSID_VPA			0x0C02

#define GSID_GPR(x)			(0x1000 + (x))
#define GSID_HDEC_EXPIRY_TB		0x1020
#define GSID_NIA			0x1021
#define GSID_MSR			0x1022
#define GSID_LR				0x1023
#define GSID_XER			0x1024
#define GSID_CTR			0x1025
#define GSID_CFAR			0x1026
#define GSID_SRR0			0x1027
#define GSID_SRR1			0x1028
#define GSID_DAR			0x1029
#define GSID_DEC_EXPIRY_TB		0x102A
#define GSID_VTB			0x102B
#define GSID_LPCR			0x102C
#define GSID_HFSCR			0x102D
#define GSID_FSCR			0x102E
#define GSID_FPSCR			0x102F
#define GSID_DAWR0			0x1030
#define GSID_DAWR1			0x1031
#define GSID_CIABR			0x1032
#define GSID_PURR			0x1033
#define GSID_SPURR			0x1034
#define GSID_IC				0x1035
#define GSID_SPRG0			0x1036
#define GSID_SPRG1			0x1037
#define GSID_SPRG2			0x1038
#define GSID_SPRG3			0x1039
#define GSID_PPR			0x103A
#define GSID_MMCR(x)			(0x103B + (x))
#define GSID_MMCRA			0x103F
#define GSID_SIER(x)			(0x1040 + (x))
#define GSID_BESCR			0x1043
#define GSID_EBBHR			0x1044
#define GSID_EBBRR			0x1045
#define GSID_AMR			0x1046
#define GSID_IAMR			0x1047
#define GSID_AMOR			0x1048
#define GSID_UAMOR			0x1049
#define GSID_SDAR			0x104A
#define GSID_SIAR			0x104B
#define GSID_DSCR			0x104C
#define GSID_TAR			0x104D
#define GSID_DEXCR			0x104E
#define GSID_HDEXCR			0x104F
#define GSID_HASHKEYR			0x1050
#define GSID_HASHPKEYR			0x1051
#define GSID_CTRL			0x1052

#define GSID_CR				0x2000
#define GSID_PIDR			0x2001
#define GSID_DSISR			0x2002
#define GSID_VSCR			0x2003
#define GSID_VRSAVE			0x2004
#define GSID_DAWRX0			0x2005
#define GSID_DAWRX1			0x2006
#define GSID_PMC(x)			(0x2007 + (x))
#define GSID_WORT			0x200D
#define GSID_PSPB			0x200E

#define GSID_VSRS(x)			(0x3000 + (x))

#define GSID_HDAR			0xF000
#define GSID_HDSISR			0xF001
#define GSID_HEIR			0xF002
#define GSID_ASDR			0xF003


#define GSE_GUESTWIDE_START GSID_BLANK
#define GSE_GUESTWIDE_END GSID_PROCESS_TABLE
#define GSE_GUESTWIDE_COUNT (GSE_GUESTWIDE_END - GSE_GUESTWIDE_START + 1)

#define GSE_META_START GSID_RUN_INPUT
#define GSE_META_END GSID_VPA
#define GSE_META_COUNT (GSE_META_END - GSE_META_START + 1)

#define GSE_DW_REGS_START GSID_GPR(0)
#define GSE_DW_REGS_END GSID_CTRL
#define GSE_DW_REGS_COUNT (GSE_DW_REGS_END - GSE_DW_REGS_START + 1)

#define GSE_W_REGS_START GSID_CR
#define GSE_W_REGS_END GSID_PSPB
#define GSE_W_REGS_COUNT (GSE_W_REGS_END - GSE_W_REGS_START + 1)

#define GSE_VSRS_START GSID_VSRS(0)
#define GSE_VSRS_END GSID_VSRS(63)
#define GSE_VSRS_COUNT (GSE_VSRS_END - GSE_VSRS_START + 1)

#define GSE_INTR_REGS_START GSID_HDAR
#define GSE_INTR_REGS_END GSID_ASDR
#define GSE_INTR_REGS_COUNT (GSE_INTR_REGS_END - GSE_INTR_REGS_START + 1)

#define GSE_IDEN_COUNT                                              \
	(GSE_GUESTWIDE_COUNT + GSE_META_COUNT + GSE_DW_REGS_COUNT + \
	 GSE_W_REGS_COUNT + GSE_VSRS_COUNT + GSE_INTR_REGS_COUNT)


/**
 * Ranges of guest state buffer elements
 */
enum {
	GS_CLASS_GUESTWIDE = 0x01,
	GS_CLASS_META = 0x02,
	GS_CLASS_DWORD_REG = 0x04,
	GS_CLASS_WORD_REG = 0x08,
	GS_CLASS_VECTOR = 0x10,
	GS_CLASS_INTR = 0x20,
};

/**
 * Types of guest state buffer elements
 */
enum {
	GSE_BE32,
	GSE_BE64,
	GSE_VEC128,
	GSE_PARTITION_TABLE,
	GSE_PROCESS_TABLE,
	GSE_BUFFER,
	__GSE_TYPE_MAX,
};

/**
 * Flags for guest state elements
 */
enum {
	GS_FLAGS_WIDE = 0x01,
};

/**
 * struct gs_part_table - deserialized partition table information element
 * @address: start of the partition table
 * @ea_bits: number of bits in the effective address
 * @gpd_size: root page directory size
 */
struct gs_part_table {
	u64 address;
	u64 ea_bits;
	u64 gpd_size;
};

/**
 * struct gs_proc_table - deserialized process table information element
 * @address: start of the process table
 * @gpd_size: process table size
 */
struct gs_proc_table {
	u64 address;
	u64 gpd_size;
};

/**
 * struct gs_buff_info - deserialized meta guest state buffer information
 * @address: start of the guest state buffer
 * @size: size of the guest state buffer
 */
struct gs_buff_info {
	u64 address;
	u64 size;
};

/**
 * struct gs_header - serialized guest state buffer header
 * @nelem: count of guest state elements in the buffer
 * @data: start of the stream of elements in the buffer
 */
struct gs_header {
	__be32 nelems;
	char data[];
} __packed;

/**
 * struct gs_elem - serialized guest state buffer element
 * @iden: Guest State ID
 * @len: length of data
 * @data: the guest state buffer element's value
 */
struct gs_elem {
	__be16 iden;
	__be16 len;
	char data[];
} __packed;

/**
 * struct gs_buff - a guest state buffer with metadata.
 * @capacity: total length of the buffer
 * @len: current length of the elements and header
 * @guest_id: guest id associated with the buffer
 * @vcpu_id: vcpu_id associated with the buffer
 * @hdr: the serialised guest state buffer
 */
struct gs_buff {
	size_t capacity;
	size_t len;
	unsigned long guest_id;
	unsigned long vcpu_id;
	struct gs_header *hdr;
};

/**
 * struct gs_bitmap - a bitmap for element ids
 * @bitmap: a bitmap large enough for all Guest State IDs
 */
struct gs_bitmap {
/* private: */
	DECLARE_BITMAP(bitmap, GSE_IDEN_COUNT);
};

/**
 * struct gs_parser - a map of element ids to locations in a buffer
 * @iterator: bitmap used for iterating
 * @gses: contains the pointers to elements
 *
 * A guest state parser is used for deserialising a guest state buffer.
 * Given a buffer, it then allows looking up guest state elements using
 * a guest state id.
 */
struct gs_parser {
/* private: */
	struct gs_bitmap iterator;
	struct gs_elem *gses[GSE_IDEN_COUNT];
};

enum {
	GSM_GUEST_WIDE = 0x1,
	GSM_SEND = 0x2,
	GSM_RECEIVE = 0x4,
	GSM_GSB_OWNER = 0x8,
};

struct gs_msg;

/**
 * struct gs_msg_ops - guest state message behavior
 * @get_size: maximum size required for the message data
 * @fill_info: serializes to the guest state buffer format
 * @refresh_info: dserializes from the guest state buffer format
 */
struct gs_msg_ops {
	size_t (*get_size)(struct gs_msg *gsm);
	int (*fill_info)(struct gs_buff *gsb, struct gs_msg *gsm);
	int (*refresh_info)(struct gs_msg *gsm, struct gs_buff *gsb);
};

/**
 * struct gs_msg - a guest state message
 * @bitmap: the guest state ids that should be included
 * @ops: modify message behavior for reading and writing to buffers
 * @flags: guest wide or thread wide
 * @data: location where buffer data will be written to or from.
 *
 * A guest state message is allows flexibility in sending in receiving data
 * in a guest state buffer format.
 */
struct gs_msg {
	struct gs_bitmap bitmap;
	struct gs_msg_ops *ops;
	unsigned long flags;
	void *data;
};

/**************************************************************************
 * Guest State IDs
 **************************************************************************/

u16 gsid_size(u16 iden);
unsigned long gsid_flags(u16 iden);
u64 gsid_mask(u16 iden);

/**************************************************************************
 * Guest State Buffers
 **************************************************************************/
struct gs_buff *gsb_new(size_t size, unsigned long guest_id,
			unsigned long vcpu_id, gfp_t flags);
void gsb_free(struct gs_buff *gsb);
void *gsb_put(struct gs_buff *gsb, size_t size);
int gsb_send(struct gs_buff *gsb, unsigned long flags);
int gsb_recv(struct gs_buff *gsb, unsigned long flags);

/**
 * gsb_header() - the header of a guest state buffer
 * @gsb: guest state buffer
 *
 * Returns a pointer to the buffer header.
 */
static inline struct gs_header *gsb_header(struct gs_buff *gsb)
{
	return gsb->hdr;
}

/**
 * gsb_data() - the elements of a guest state buffer
 * @gsb: guest state buffer
 *
 * Returns a pointer to the first element of the buffer data.
 */
static inline struct gs_elem *gsb_data(struct gs_buff *gsb)
{
	return (struct gs_elem *)gsb_header(gsb)->data;
}

/**
 * gsb_len() - the current length of a guest state buffer
 * @gsb: guest state buffer
 *
 * Returns the length including the header of a buffer.
 */
static inline size_t gsb_len(struct gs_buff *gsb)
{
	return gsb->len;
}

/**
 * gsb_capacity() - the capacity of a guest state buffer
 * @gsb: guest state buffer
 *
 * Returns the capacity of a buffer.
 */
static inline size_t gsb_capacity(struct gs_buff *gsb)
{
	return gsb->capacity;
}

/**
 * gsb_paddress() - the physical address of buffer
 * @gsb: guest state buffer
 *
 * Returns the physical address of the buffer.
 */
static inline u64 gsb_paddress(struct gs_buff *gsb)
{
	return __pa(gsb_header(gsb));
}

/**
 * gsb_nelems() - the number of elements in a buffer
 * @gsb: guest state buffer
 *
 * Returns the number of elements in a buffer
 */
static inline u32 gsb_nelems(struct gs_buff *gsb)
{
	return be32_to_cpu(gsb_header(gsb)->nelems);
}

/**
 * gsb_reset() - empty a guest state buffer
 * @gsb: guest state buffer
 *
 * Reset the number of elements and length of buffer to empty.
 */
static inline void gsb_reset(struct gs_buff *gsb)
{
	gsb_header(gsb)->nelems = cpu_to_be32(0);
	gsb->len = sizeof(struct gs_header);
}

/**
 * gsb_data_len() - the length of a buffer excluding the header
 * @gsb: guest state buffer
 *
 * Returns the length of a buffer excluding the header
 */
static inline size_t gsb_data_len(struct gs_buff *gsb)
{
	return gsb->len - sizeof(struct gs_header);
}

/**
 * gsb_data_cap() - the capacity of a buffer excluding the header
 * @gsb: guest state buffer
 *
 * Returns the capacity of a buffer excluding the header
 */
static inline size_t gsb_data_cap(struct gs_buff *gsb)
{
	return gsb->capacity - sizeof(struct gs_header);
}

/**
 * gsb_for_each_elem - iterate over the elements in a buffer
 * @i: loop counter
 * @pos: set to current element
 * @gsb: guest state buffer
 * @rem: initialized to buffer capacity, holds bytes currently remaining in stream
 */
#define gsb_for_each_elem(i, pos, gsb, rem)                       \
	gse_for_each_elem(i, gsb_nelems(gsb), pos, gsb_data(gsb), \
			  gsb_data_cap(gsb), rem)

/**************************************************************************
 * Guest State Elements
 **************************************************************************/

/**
 * gse_iden() - guest state ID of element
 * @gse: guest state element
 *
 * Return the guest state ID in host endianness.
 */
static inline u16 gse_iden(const struct gs_elem *gse)
{
	return be16_to_cpu(gse->iden);
}

/**
 * gse_len() - length of guest state element data
 * @gse: guest state element
 *
 * Returns the length of guest state element data
 */
static inline u16 gse_len(const struct gs_elem *gse)
{
	return be16_to_cpu(gse->len);
}

/**
 * gse_total_len() - total length of guest state element
 * @gse: guest state element
 *
 * Returns the length of the data plus the ID and size header.
 */
static inline u16 gse_total_len(const struct gs_elem *gse)
{
	return be16_to_cpu(gse->len) + sizeof(*gse);
}

/**
 * gse_total_size() - space needed for a given data length
 * @size: data length
 *
 * Returns size plus the space needed for the ID and size header.
 */
static inline u16 gse_total_size(u16 size)
{
	return sizeof(struct gs_elem) + size;
}

/**
 * gse_data() - pointer to data of a guest state element
 * @gse: guest state element
 *
 * Returns a pointer to the beginning of guest state element data.
 */
static inline void *gse_data(const struct gs_elem *gse)
{
	return (void *)gse->data;
}

/**
 * gse_ok() - checks space exists for guest state element
 * @gse: guest state element
 * @remaining: bytes of space remaining
 *
 * Returns true if the guest state element can fit in remaining space.
 */
static inline bool gse_ok(const struct gs_elem *gse, int remaining)
{
	return remaining >= gse_total_len(gse);
}

/**
 * gse_next() - iterate to the next guest state element in a stream
 * @gse: stream of guest state elements
 * @remaining: length of the guest element stream
 *
 * Returns the next guest state element in a stream of elements. The length of
 * the stream is updated in remaining.
 */
static inline struct gs_elem *gse_next(const struct gs_elem *gse,
				       int *remaining)
{
	int len = sizeof(*gse) + gse_len(gse);

	*remaining -= len;
	return (struct gs_elem *)(gse->data + gse_len(gse));
}

/**
 * gse_for_each_elem - iterate over a stream of guest state elements
 * @i: loop counter
 * @max: number of elements
 * @pos: set to current element
 * @head: head of elements
 * @len: length of the stream
 * @rem: initialized to len, holds bytes currently remaining elements
 */
#define gse_for_each_elem(i, max, pos, head, len, rem)                  \
	for (i = 0, pos = head, rem = len; gse_ok(pos, rem) && i < max; \
	     pos = gse_next(pos, &(rem)), i++)

int __gse_put(struct gs_buff *gsb, u16 iden, u16 size, const void *data);
int gse_parse(struct gs_parser *gsp, struct gs_buff *gsb);

/**
 * gse_put_be32() - add a be32 guest state element to a buffer
 * @gsb: guest state buffer to add element to
 * @iden: guest state ID
 * @val: big endian value
 */
static inline int gse_put_be32(struct gs_buff *gsb, u16 iden, __be32 val)
{
	__be32 tmp;

	tmp = val;
	return __gse_put(gsb, iden, sizeof(__be32), &tmp);
}

/**
 * gse_put_u32() - add a host endian 32bit int guest state element to a buffer
 * @gsb: guest state buffer to add element to
 * @iden: guest state ID
 * @val: host endian value
 */
static inline int gse_put_u32(struct gs_buff *gsb, u16 iden, u32 val)
{
	__be32 tmp;

	tmp = cpu_to_be32(val);
	return gse_put_be32(gsb, iden, tmp);
}

/**
 * gse_put_be64() - add a be64 guest state element to a buffer
 * @gsb: guest state buffer to add element to
 * @iden: guest state ID
 * @val: big endian value
 */
static inline int gse_put_be64(struct gs_buff *gsb, u16 iden, __be64 val)
{
	__be64 tmp;

	tmp = val;
	return __gse_put(gsb, iden, sizeof(__be64), &tmp);
}

/**
 * gse_put_u64() - add a host endian 64bit guest state element to a buffer
 * @gsb: guest state buffer to add element to
 * @iden: guest state ID
 * @val: host endian value
 */
static inline int gse_put_u64(struct gs_buff *gsb, u16 iden, u64 val)
{
	__be64 tmp;

	tmp = cpu_to_be64(val);
	return gse_put_be64(gsb, iden, tmp);
}

/**
 * __gse_put_reg() - add a register type guest state element to a buffer
 * @gsb: guest state buffer to add element to
 * @iden: guest state ID
 * @val: host endian value
 *
 * Adds a register type guest state element. Uses the guest state ID for
 * determining the length of the guest element. If the guest state ID has
 * bits that can not be set they will be cleared.
 */
static inline int __gse_put_reg(struct gs_buff *gsb, u16 iden, u64 val)
{
	val &= gsid_mask(iden);
	if (gsid_size(iden) == sizeof(u64))
		return gse_put_u64(gsb, iden, val);

	if (gsid_size(iden) == sizeof(u32)) {
		u32 tmp;

		tmp = (u32)val;
		if (tmp != val)
			return -EINVAL;

		return gse_put_u32(gsb, iden, tmp);
	}
	return -EINVAL;
}

/**
 * gse_put_vector128() - add a vector guest state element to a buffer
 * @gsb: guest state buffer to add element to
 * @iden: guest state ID
 * @val: 16 byte vector value
 */
static inline int gse_put_vector128(struct gs_buff *gsb, u16 iden,
				    vector128 val)
{
	__be64 tmp[2] = { 0 };
	union {
		__vector128 v;
		u64 dw[2];
	} u;

	u.v = val;
	tmp[0] = cpu_to_be64(u.dw[TS_FPROFFSET]);
#ifdef CONFIG_VSX
	tmp[1] = cpu_to_be64(u.dw[TS_VSRLOWOFFSET]);
#endif
	return __gse_put(gsb, iden, sizeof(tmp), &tmp);
}

/**
 * gse_put_part_table() - add a partition table guest state element to a buffer
 * @gsb: guest state buffer to add element to
 * @iden: guest state ID
 * @val: partition table value
 */
static inline int gse_put_part_table(struct gs_buff *gsb, u16 iden,
				     struct gs_part_table val)
{
	__be64 tmp[3];

	tmp[0] = cpu_to_be64(val.address);
	tmp[1] = cpu_to_be64(val.ea_bits);
	tmp[2] = cpu_to_be64(val.gpd_size);
	return __gse_put(gsb, GSID_PARTITION_TABLE, sizeof(tmp), &tmp);
}

/**
 * gse_put_proc_table() - add a process table guest state element to a buffer
 * @gsb: guest state buffer to add element to
 * @iden: guest state ID
 * @val: process table value
 */
static inline int gse_put_proc_table(struct gs_buff *gsb, u16 iden,
				     struct gs_proc_table val)
{
	__be64 tmp[2];

	tmp[0] = cpu_to_be64(val.address);
	tmp[1] = cpu_to_be64(val.gpd_size);
	return __gse_put(gsb, GSID_PROCESS_TABLE, sizeof(tmp), &tmp);
}

/**
 * gse_put_buff_info() - adds a GSB description guest state element to a buffer
 * @gsb: guest state buffer to add element to
 * @iden: guest state ID
 * @val: guest state buffer description value
 */
static inline int gse_put_buff_info(struct gs_buff *gsb, u16 iden,
				    struct gs_buff_info val)
{
	__be64 tmp[2];

	tmp[0] = cpu_to_be64(val.address);
	tmp[1] = cpu_to_be64(val.size);
	return __gse_put(gsb, iden, sizeof(tmp), &tmp);
}

int __gse_put(struct gs_buff *gsb, u16 iden, u16 size, const void *data);

/**
 * gse_get_be32() - return the data of a be32 element
 * @gse: guest state element
 */
static inline __be32 gse_get_be32(const struct gs_elem *gse)
{
	return *(__be32 *)gse_data(gse);
}

/**
 * gse_get_u32() - return the data of a be32 element in host endianness
 * @gse: guest state element
 */
static inline u32 gse_get_u32(const struct gs_elem *gse)
{
	return be32_to_cpu(gse_get_be32(gse));
}

/**
 * gse_get_be64() - return the data of a be64 element
 * @gse: guest state element
 */
static inline __be64 gse_get_be64(const struct gs_elem *gse)
{
	return *(__be64 *)gse_data(gse);
}

/**
 * gse_get_u64() - return the data of a be64 element in host endianness
 * @gse: guest state element
 */
static inline u64 gse_get_u64(const struct gs_elem *gse)
{
	return be64_to_cpu(gse_get_be64(gse));
}

/**
 * __gse_get_reg() - return the date of a register type guest state element
 * @gse: guest state element
 *
 * Determine the element data size from its guest state ID and return the
 * correctly sized value.
 */
static inline u64 __gse_get_reg(const struct gs_elem *gse)
{
	if (gse_len(gse) == sizeof(u64))
		return gse_get_u64(gse);

	if (gse_len(gse) == sizeof(u32)) {
		u32 tmp;

		tmp = gse_get_u32(gse);
		return (u64)tmp;
	}
	return 0;
}

/**
 * gse_get_vector128() - return the data of a vector element
 * @gse: guest state element
 */
static inline vector128 gse_get_vector128(const struct gs_elem *gse)
{
	union {
		__vector128 v;
		u64 dw[2];
	} u = { 0 };
	__be64 *src;

	src = (__be64 *)gse_data(gse);
	u.dw[TS_FPROFFSET] = be64_to_cpu(src[0]);
#ifdef CONFIG_VSX
	u.dw[TS_VSRLOWOFFSET] = be64_to_cpu(src[1]);
#endif
	return u.v;
}

/**
 * gse_put - add a guest state element to a buffer
 * @gsb: guest state buffer to add to
 * @iden: guest state identity
 * @v: generic value
 */
#define gse_put(gsb, iden, v)					\
	(_Generic((v),						\
		  u64 : __gse_put_reg,				\
		  long unsigned int : __gse_put_reg,		\
		  u32 : __gse_put_reg,				\
		  struct gs_buff_info : gse_put_buff_info,	\
		  struct gs_proc_table : gse_put_proc_table,	\
		  struct gs_part_table : gse_put_part_table,	\
		  vector128 : gse_put_vector128)(gsb, iden, v))

/**
 * gse_get - return the data of a guest state element
 * @gsb: guest state element to add to
 * @v: generic value pointer to return in
 */
#define gse_get(gse, v)						\
	(*v = (_Generic((v),					\
			u64 * : __gse_get_reg,			\
			unsigned long * : __gse_get_reg,	\
			u32 * : __gse_get_reg,			\
			vector128 * : gse_get_vector128)(gse)))

/**************************************************************************
 * Guest State Bitmap
 **************************************************************************/

bool gsbm_test(struct gs_bitmap *gsbm, u16 iden);
void gsbm_set(struct gs_bitmap *gsbm, u16 iden);
void gsbm_clear(struct gs_bitmap *gsbm, u16 iden);
u16 gsbm_next(struct gs_bitmap *gsbm, u16 prev);

/**
 * gsbm_zero - zero the entire bitmap
 * @gsbm: guest state buffer bitmap
 */
static inline void gsbm_zero(struct gs_bitmap *gsbm)
{
	bitmap_zero(gsbm->bitmap, GSE_IDEN_COUNT);
}

/**
 * gsbm_fill - fill the entire bitmap
 * @gsbm: guest state buffer bitmap
 */
static inline void gsbm_fill(struct gs_bitmap *gsbm)
{
	bitmap_fill(gsbm->bitmap, GSE_IDEN_COUNT);
	clear_bit(0, gsbm->bitmap);
}

/**
 * gsbm_for_each - iterate the present guest state IDs
 * @gsbm: guest state buffer bitmap
 * @iden: current guest state ID
 */
#define gsbm_for_each(gsbm, iden) \
	for (iden = gsbm_next(gsbm, 0); iden != 0; iden = gsbm_next(gsbm, iden))


/**************************************************************************
 * Guest State Parser
 **************************************************************************/

void gsp_insert(struct gs_parser *gsp, u16 iden, struct gs_elem *gse);
struct gs_elem *gsp_lookup(struct gs_parser *gsp, u16 iden);

/**
 * gsp_for_each - iterate the <guest state IDs, guest state element> pairs
 * @gsp: guest state buffer bitmap
 * @iden: current guest state ID
 * @gse: guest state element
 */
#define gsp_for_each(gsp, iden, gse)                              \
	for (iden = gsbm_next(&(gsp)->iterator, 0),               \
	    gse = gsp_lookup((gsp), iden);                        \
	     iden != 0; iden = gsbm_next(&(gsp)->iterator, iden), \
	    gse = gsp_lookup((gsp), iden))

/**************************************************************************
 * Guest State Message
 **************************************************************************/

/**
 * gsm_for_each - iterate the guest state IDs included in a guest state message
 * @gsp: guest state buffer bitmap
 * @iden: current guest state ID
 * @gse: guest state element
 */
#define gsm_for_each(gsm, iden)                            \
	for (iden = gsbm_next(&gsm->bitmap, 0); iden != 0; \
	     iden = gsbm_next(&gsm->bitmap, iden))

int gsm_init(struct gs_msg *mgs, struct gs_msg_ops *ops, void *data,
	     unsigned long flags);

struct gs_msg *gsm_new(struct gs_msg_ops *ops, void *data, unsigned long flags,
		       gfp_t gfp_flags);
void gsm_free(struct gs_msg *gsm);
size_t gsm_size(struct gs_msg *gsm);
int gsm_fill_info(struct gs_msg *gsm, struct gs_buff *gsb);
int gsm_refresh_info(struct gs_msg *gsm, struct gs_buff *gsb);

/**
 * gsm_include - indicate a guest state ID should be included when serializing
 * @gsm: guest state message
 * @iden: guest state ID
 */
static inline void gsm_include(struct gs_msg *gsm, u16 iden)
{
	gsbm_set(&gsm->bitmap, iden);
}

/**
 * gsm_includes - check if a guest state ID will be included when serializing
 * @gsm: guest state message
 * @iden: guest state ID
 */
static inline bool gsm_includes(struct gs_msg *gsm, u16 iden)
{
	return gsbm_test(&gsm->bitmap, iden);
}

/**
 * gsm_includes - indicate all guest state IDs should be included when serializing
 * @gsm: guest state message
 * @iden: guest state ID
 */
static inline void gsm_include_all(struct gs_msg *gsm)
{
	gsbm_fill(&gsm->bitmap);
}

/**
 * gsm_include - clear the guest state IDs that should be included when serializing
 * @gsm: guest state message
 */
static inline void gsm_reset(struct gs_msg *gsm)
{
	gsbm_zero(&gsm->bitmap);
}

/**
 * gsb_receive_data - flexibly update values from a guest state buffer
 * @gsb: guest state buffer
 * @gsm: guest state message
 *
 * Requests updated values for the guest state values included in the guest
 * state message. The guest state message will then deserialize the guest state
 * buffer.
 */
static inline int gsb_receive_data(struct gs_buff *gsb, struct gs_msg *gsm)
{
	int rc;

	rc = gsm_fill_info(gsm, gsb);
	if (rc < 0)
		return rc;

	rc = gsb_recv(gsb, gsm->flags);
	if (rc < 0)
		return rc;

	rc = gsm_refresh_info(gsm, gsb);
	if (rc < 0)
		return rc;
	return 0;
}

/**
 * gsb_recv - receive a single guest state ID
 * @gsb: guest state buffer
 * @gsm: guest state message
 * @iden: guest state identity
 */
static inline int gsb_receive_datum(struct gs_buff *gsb, struct gs_msg *gsm,
				    u16 iden)
{
	int rc;

	gsm_include(gsm, iden);
	rc = gsb_receive_data(gsb, gsm);
	if (rc < 0)
		return rc;
	gsm_reset(gsm);
	return 0;
}

/**
 * gsb_send_data - flexibly send values from a guest state buffer
 * @gsb: guest state buffer
 * @gsm: guest state message
 *
 * Sends the guest state values included in the guest state message.
 */
static inline int gsb_send_data(struct gs_buff *gsb, struct gs_msg *gsm)
{
	int rc;

	rc = gsm_fill_info(gsm, gsb);
	if (rc < 0)
		return rc;
	rc = gsb_send(gsb, gsm->flags);

	return rc;
}

/**
 * gsb_recv - send a single guest state ID
 * @gsb: guest state buffer
 * @gsm: guest state message
 * @iden: guest state identity
 */
static inline int gsb_send_datum(struct gs_buff *gsb, struct gs_msg *gsm,
				 u16 iden)
{
	int rc;

	gsm_include(gsm, iden);
	rc = gsb_send_data(gsb, gsm);
	if (rc < 0)
		return rc;
	gsm_reset(gsm);
	return 0;
}

#endif /* _ASM_POWERPC_GUEST_STATE_BUFFER_H */
