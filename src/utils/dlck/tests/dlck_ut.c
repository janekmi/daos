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