// SPDX-License-Identifier: GPL-2.0-or-later

#include <linux/init.h>
#include <linux/log2.h>

#include <asm/guest-state-buffer.h>

#define check(x)                                                               \
	do {                                                                   \
		if (!(x))                                                      \
			pr_err("guest-state-buffer: test failed at line %d\n", \
			       __LINE__);                                      \
	} while (0)

static void __init test_creating_buffer(void)
{
	struct gs_buff *gsb;
	size_t size = 0x100;

	gsb = gsb_new(size, 0, 0, GFP_KERNEL);
	check(gsb);
	if (!gsb)
		return;
	check(gsb->hdr);
	if (!gsb->hdr)
		return;
	check(gsb->capacity == roundup_pow_of_two(size));
	check(gsb->len == sizeof(__be32));

	gsb_free(gsb);
}

static void __init test_adding_element(void)
{
	const struct gs_elem *head, *curr;
	union {
		__vector128 v;
		u64 dw[2];
	} u;
	int rem;
	struct gs_buff *gsb;
	size_t size = 0x1000;
	int i, rc;
	u64 data;

	gsb = gsb_new(size, 0, 0, GFP_KERNEL);
	check(gsb);
	if (!gsb)
		return;

	/* Single elements, direct use of __gse_put() */
	data = 0xdeadbeef;
	rc = __gse_put(gsb, GSID_GPR0, 8, &data);
	check(rc >= 0);

	head = gsb_data(gsb);
	check(gse_iden(head) == GSID_GPR0);
	check(gse_len(head) == 8);
	data = 0;
	memcpy(&data, gse_data(head), 8);
	check(data == 0xdeadbeef);

	/* Multiple elements, simple wrapper */
	rc = gse_put_u64(gsb, GSID_GPR1, 0xcafef00d);
	check(rc >= 0);

	u.dw[0] = 0x1;
	u.dw[1] = 0x2;
	rc = gse_put_vector128(gsb, GSID_VSRS0, u.v);
	check(rc >= 0);
	u.dw[0] = 0x0;
	u.dw[1] = 0x0;

	gsb_for_each_elem(i, curr, gsb, rem) {
		switch (i) {
		case 0:
			check(gse_iden(curr) == GSID_GPR0);
			check(gse_len(curr) == 8);
			check(gse_get_be64(curr) == 0xdeadbeef);
			break;
		case 1:
			check(gse_iden(curr) == GSID_GPR1);
			check(gse_len(curr) == 8);
			check(gse_get_u64(curr) == 0xcafef00d);
			break;
		case 2:
			check(gse_iden(curr) == GSID_VSRS0);
			check(gse_len(curr) == 16);
			u.v = gse_get_vector128(curr);
			check(u.dw[0] == 0x1);
			check(u.dw[1] = 0x2);
			break;
		}
	}
	check(i == 3);

	gsb_reset(gsb);
	check(gsb_nelems(gsb) == 0);
	check(gsb_len(gsb) == sizeof(struct gs_header));

	gsb_free(gsb);
}

static void __init test_gs_parsing(void)
{
	struct gs_elem *gse;
	struct gs_parser gsp = { 0 };
	struct gs_buff *gsb;
	size_t size = 0x1000;
	u64 tmp1, tmp2;

	gsb = gsb_new(size, 0, 0, GFP_KERNEL);
	check(gsb);
	if (!gsb)
		return;

	tmp1 = 0xdeadbeefull;
	gse_put(gsb, GSID_GPR0, tmp1);

	check(gse_parse(&gsp, gsb) >= 0);

	gse = gsp_lookup(&gsp, GSID_GPR0);
	if (!gse)
		return;
	gse_get(gse, &tmp2);
	check(tmp2 == 0xdeadbeefull);

	gsb_free(gsb);
}

static void __init test_gs_bitmap(void)
{
	struct gs_bitmap gsbm = { 0 };
	struct gs_bitmap gsbm1 = { 0 };
	struct gs_bitmap gsbm2 = { 0 };
	u16 iden;
	int i, j;

	i = 0;
	for (u16 iden = GSID_HOST_STATE_SIZE;
	     iden <= GSID_PROCESS_TABLE; iden++) {
		gsbm_set(&gsbm, iden);
		gsbm_set(&gsbm1, iden);
		check(gsbm_test(&gsbm, iden));
		gsbm_clear(&gsbm, iden);
		check(!gsbm_test(&gsbm, iden));
		i++;
	}

	for (u16 iden = GSID_RUN_INPUT; iden <= GSID_VPA;
	     iden++) {
		gsbm_set(&gsbm, iden);
		gsbm_set(&gsbm1, iden);
		check(gsbm_test(&gsbm, iden));
		gsbm_clear(&gsbm, iden);
		check(!gsbm_test(&gsbm, iden));
		i++;
	}

	for (u16 iden = GSID_GPR0; iden <= GSID_CTRL;
	     iden++) {
		gsbm_set(&gsbm, iden);
		gsbm_set(&gsbm1, iden);
		check(gsbm_test(&gsbm, iden));
		gsbm_clear(&gsbm, iden);
		check(!gsbm_test(&gsbm, iden));
		i++;
	}

	for (u16 iden = GSID_CR; iden <= GSID_PSPB; iden++) {
		gsbm_set(&gsbm, iden);
		gsbm_set(&gsbm1, iden);
		check(gsbm_test(&gsbm, iden));
		gsbm_clear(&gsbm, iden);
		check(!gsbm_test(&gsbm, iden));
		i++;
	}

	for (u16 iden = GSID_VSRS0; iden <= GSID_VSRS63;
	     iden++) {
		gsbm_set(&gsbm, iden);
		gsbm_set(&gsbm1, iden);
		check(gsbm_test(&gsbm, iden));
		gsbm_clear(&gsbm, iden);
		check(!gsbm_test(&gsbm, iden));
		i++;
	}

	for (u16 iden = GSID_HDAR; iden <= GSID_ASDR;
	     iden++) {
		gsbm_set(&gsbm, iden);
		gsbm_set(&gsbm1, iden);
		check(gsbm_test(&gsbm, iden));
		gsbm_clear(&gsbm, iden);
		check(!gsbm_test(&gsbm, iden));
		i++;
	}

	j = 0;
	gsbm_for_each(&gsbm1, iden)
	{
		gsbm_set(&gsbm2, iden);
		j++;
	}
	check(i == j);
	check(!memcmp(&gsbm1, &gsbm2, sizeof(gsbm1)));
}

struct gs_msg_test1_data {
	u64 a;
	u32 b;
	struct gs_part_table c;
	struct gs_proc_table d;
	struct gs_buff_info e;
};

static size_t __init test1_get_size(struct gs_msg *gsm)
{
	size_t size = 0;
	u16 ids[] = {
		GSID_PARTITION_TABLE,
		GSID_PROCESS_TABLE,
		GSID_RUN_INPUT,
		GSID_GPR0,
		GSID_CR,
	};

	for (int i = 0; i < ARRAY_SIZE(ids); i++)
		size += gse_total_size(gsid_size(ids[i]));
	return size;
}

static int __init test1_fill_info(struct gs_buff *gsb, struct gs_msg *gsm)
{
	struct gs_msg_test1_data *data = gsm->data;

	if (gsm_includes(gsm, GSID_GPR0))
		gse_put(gsb, GSID_GPR0, data->a);

	if (gsm_includes(gsm, GSID_CR))
		gse_put(gsb, GSID_CR, data->b);

	if (gsm_includes(gsm, GSID_PARTITION_TABLE))
		gse_put(gsb, GSID_PARTITION_TABLE, data->c);

	if (gsm_includes(gsm, GSID_PROCESS_TABLE))
		gse_put(gsb, GSID_PARTITION_TABLE, data->d);

	if (gsm_includes(gsm, GSID_RUN_INPUT))
		gse_put(gsb, GSID_RUN_INPUT, data->e);

	return 0;
}

static int __init test1_refresh_info(struct gs_msg *gsm, struct gs_buff *gsb)
{
	struct gs_parser gsp = { 0 };
	struct gs_msg_test1_data *data = gsm->data;
	struct gs_elem *gse;
	int rc;

	rc = gse_parse(&gsp, gsb);
	if (rc < 0)
		return rc;

	gse = gsp_lookup(&gsp, GSID_GPR0);
	if (gse)
		gse_get(gse, &data->a);

	gse = gsp_lookup(&gsp, GSID_CR);
	if (gse)
		gse_get(gse, &data->b);

	return 0;
}

static struct gs_msg_ops gs_msg_test1_ops __initdata = {
	.get_size = test1_get_size,
	.fill_info = test1_fill_info,
	.refresh_info = test1_refresh_info,
};

static void __init test_gs_msg(void)
{
	struct gs_msg_test1_data test1_data = {
		.a = 0xdeadbeef,
		.b = 0x1,
	};
	struct gs_msg *gsm;
	struct gs_buff *gsb;

	gsm = gsm_new(&gs_msg_test1_ops, &test1_data, GSM_SEND, GFP_KERNEL);
	check(gsm);
	if (!gsm)
		return;

	gsb = gsb_new(gsm_size(gsm), 0, 0, GFP_KERNEL);
	check(gsb);
	if (!gsb)
		return;

	gsm_include(gsm, GSID_PARTITION_TABLE);
	gsm_include(gsm, GSID_PROCESS_TABLE);
	gsm_include(gsm, GSID_RUN_INPUT);
	gsm_include(gsm, GSID_GPR0);
	gsm_include(gsm, GSID_CR);

	gsm_fill_info(gsm, gsb);

	memset(&test1_data, 0, sizeof(test1_data));

	gsm_refresh_info(gsm, gsb);
	check(test1_data.a == 0xdeadbeef);
	check(test1_data.b == 0x1);

	gsm_free(gsm);
}


static int __init test_guest_state_buffer(void)
{
	pr_info("Running guest state buffer self-tests ...\n");

	test_creating_buffer();
	test_adding_element();

	test_gs_bitmap();
	test_gs_parsing();

	test_gs_msg();

	return 0;
}
late_initcall(test_guest_state_buffer);
