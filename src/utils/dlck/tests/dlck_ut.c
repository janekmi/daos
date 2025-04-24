/**
 * (C) Copyright 2025 Hewlett Packard Enterprise Development LP
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */

#include <stdio.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>

#include <daos_srv/dlck.h>

#include <vos_internal.h>

/**
 * The smaller tree order allows the creation of more complex tree structures using a smaller number
 * of records.
 */
#define VOS_CONT_ORDER_ALT BTR_ORDER_MIN

static const char co_uuid_str_tmpl[] = "a367beed-8857-461c-a532-92ca618e589_";

struct cont_df_args {
	struct vos_cont_df *ca_cont_df;
	struct vos_pool    *ca_pool;
};

static void
setup(daos_handle_t *poh)
{
	static struct vos_pool pool           = {0};
	static umem_ops_t      umm_ops        = {0};
	static struct btr_root pool_cont_root = {0};
	struct umem_attr       uma            = {.uma_id = UMEM_CLASS_VMEM, .uma_pool = NULL};
	uuid_t                 co_uuid;
	int                    rc;

	/** register required btree classes */
	rc = vos_cont_tab_register();
	assert_int_equal(rc, 0);
	rc = vos_obj_tab_register();
	assert_int_equal(rc, 0);

	pool.vp_umm.umm_ops = &umm_ops;
	*poh                = vos_pool2hdl(&pool);

	/**
	 * this ought to mimic how an actual container table is created when a VOS pool is created
	 */
	rc = dbtree_create_inplace(VOS_BTR_CONT_TABLE, 0, VOS_CONT_ORDER_ALT, &uma, &pool_cont_root,
				   &pool.vp_cont_th);
	assert_int_equal(rc, 0);

	/** populate the container table */
	char co_uuid_str[UUID_STR_LEN];
	for (int i = 0; i < 16; ++i) {
		snprintf(co_uuid_str, UUID_STR_LEN, "%.35s%x", co_uuid_str_tmpl, i);
		rc = uuid_parse(co_uuid_str, co_uuid);
		assert_int_equal(rc, 0);
		rc = vos_cont_create(*poh, co_uuid);
		assert_int_equal(rc, 0);
	}

	/**
	 * XXX should be used to validate the containers' tree after the tree is validated and
	 * fixed.
	 */
	daos_handle_t       ih;
	d_iov_t             val;
	struct cont_df_args args;
	char                uuid_str[UUID_STR_LEN];
	rc = dbtree_iter_prepare(pool.vp_cont_th, 0, &ih);
	assert_int_equal(rc, 0);
	rc = dbtree_iter_probe(ih, BTR_PROBE_FIRST, DAOS_INTENT_DEFAULT, NULL, NULL);
	assert_int_equal(rc, 0);
	val.iov_buf = &args;
	do {
		rc = dbtree_iter_fetch(ih, NULL, &val, NULL);
		assert_int_equal(rc, 0);
		uuid_unparse(args.ca_cont_df->cd_id, uuid_str);
		printf("%s\n", uuid_str);
		rc = dbtree_iter_next(ih);
	} while (rc == 0);
	rc = dbtree_iter_finish(ih);
	assert_int_equal(rc, 0);
}

int
main(int argc, char **argv)
{
	struct DLCK_btree_faulty_nodes_array array;
	daos_handle_t                        poh;
	int                                  rc;

	setup(&poh);

	rc = dlck_vos_pool_containers_check(poh, &array);
	assert_int_equal(rc, 0);

	return 0;
}