/**
 * (C) Copyright 2025 Hewlett Packard Enterprise Development LP
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */

#include <stdio.h>
#include <daos/mem.h>
#include <daos_srv/vos.h>
#include <daos_srv/dlck.h>
// #include "vos_internal.h"

static const char  *db_path  = "/mnt/daos/";
static int          tgt_id   = 0;
static const char  *path     = "/mnt/daos/a367beed-8857-461c-a532-92ca618e589c/vpool.0";
static const char  *uuid_str = "a367beed-8857-461c-a532-92ca618e589c";
static unsigned int flags    = VOS_POF_EXCL | VOS_POF_EXTERNAL_FLUSH | VOS_POF_FOR_FEATURE_FLAG;

static bool
dlck_ask_yes_no(const char *question)
{
	char choice;
	printf("%s (y/[n]): ", question);
	choice = getchar();
	if (choice == 'y') {
		return true;
	} else {
		return false;
	}
}

static int
dlck_ask_value(struct DLCK_value *values, int values_num)
{
	char choice;
	for (int i = 0; i < values_num; ++i) {
		printf("[%d] %s\n", i, values[i].dv_value_name);
	}
	printf("[%d] do not fix (default)\n", values_num);

	printf(": ");
	choice         = getchar();
	int choice_int = choice - '0';
	assert(choice_int < values_num + 1);
	return choice_int;
}

static bool
dlck_check_offset(umem_off_t off)
{
	// TBD hardcoded 3GiB. Ought to be read from umm?
	const uint64_t pool_size = (uint64_t)3 << 30;

	uint64_t       offset = umem_off2offset(off);
	return offset < pool_size;
}

static int
iter_nop(daos_handle_t ih, vos_iter_entry_t *entry, vos_iter_type_t type, vos_iter_param_t *param,
	 void *cb_arg, unsigned int *acts)
{
	return 0;
}

struct DLCK_callbacks callbacks = {.dc_ask_yes_no   = dlck_ask_yes_no,
				   .dc_ask_value    = dlck_ask_value,
				   .dc_check_offset = dlck_check_offset};

int
main(int argc, char *argv[])
{
	uuid_t                  uuid       = {0};
	daos_handle_t           poh        = DAOS_HDL_INVAL;
	vos_iter_param_t        iter_param = {0};
	struct vos_iter_anchors anchors    = {0};
	int                     rc;

	DLCK_Callbacks = &callbacks;

	/**
	 * TODO:
	 * - vos_self_init
	 */
	rc = vos_self_init(db_path, true, tgt_id);
	assert(rc == 0);

	uuid_parse(uuid_str, uuid);
	/**
	 * TODO:
	 * - vea_load
	 */
	rc = vos_pool_open(path, uuid, flags, &poh);
	assert(rc == 0);

	struct DLCK_btree_faulty_nodes_array array = {0};
	rc                                         = dlck_vos_pool_containers_check(poh, &array);
	assert(rc == 0);

	if (array.dba_nodes_num > 0) {
		/**
		 * TBD:
		 * - Describe to the user what faults are present in the container's tree
		 * - Ask the user what they want to do with all of them
		 * - If no fix is allowed exit with error, can't continue working on this pool
		 * - Otherwise...
		 * - Remove all the faulty nodes considering their respective faults to minimalize
		 * the data loss
		 * - Rebalance the tree
		 */
	}

	iter_param.ip_hdl = poh;
	rc = vos_iterate(&iter_param, VOS_ITER_COUUID, false, &anchors, iter_nop, NULL, NULL, NULL);
	assert(rc == 0);

	// rc = vos_iterate_key()

	rc = vos_pool_close(poh);
	assert(rc == 0);

	return 0;
}
