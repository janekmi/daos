/**
 * (C) Copyright 2025 Hewlett Packard Enterprise Development LP
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */

#include <stdlib.h>
#include <stdio.h>
#include <daos/mem.h>
#define VOS_STANDALONE
#include <daos_srv/vos.h>
#include <daos_srv/dlck.h>
#include <daos_version.h>

#include "dlck_args.h"

#define DB_PATH_TEMPLATE "/tmp/dlck_XXXXXX"
#define CO_UUIDS_GROW_BY 10

static unsigned int flags =
    VOS_POF_EXCL | VOS_POF_EXTERNAL_FLUSH | VOS_POF_FOR_FEATURE_FLAG | VOS_POF_SKIP_UUID_CHECK;

/**
 * Just add the container's UUID to the provided array.
 */
static int
cont_list(daos_handle_t ih, vos_iter_entry_t *entry, vos_iter_type_t type, vos_iter_param_t *param,
	  void *cb_arg, unsigned int *acts)
{
	struct dlck_array *co_uuids = cb_arg;
	return dlck_array_append(co_uuids, entry->ie_couuid);
}

void *TLS;

struct vos_tls *
dlck_tls_get()
{
	return TLS;
}

void *
thread_function(void *arg)
{
	struct dlck_array da  = {0};
	struct dlck_stats ds  = {0};
	daos_handle_t    *coh = (daos_handle_t *)arg;
	int               rc;

	dlck_array_init(sizeof(struct dlck_dtx_rec), 10, &da);

	TLS = vos_standalone_tls_alloc(DAOS_TGT_TAG);

	rc = dlck_vos_cont_rec_get_active(*coh, &da, &ds);
	assert(rc == 0);

	printf("dda.touched = %u\n", ds.touched);

	vos_standalone_tls_free(TLS);

	return NULL;
}

/**
 * It seems what we aim to achieve at the moment does not require
 * using the actual sys_db. Processing a number of VOS files it is
 * not even obvious which of the sys_dbs available we should open.
 * Creating a new temporary sys_db seems a good way around it.
 */
static char *
temp_db_path_init()
{
	static char db_path[] = DB_PATH_TEMPLATE;
	if (mkdtemp(db_path) == NULL) {
		D_ERROR("Can not create a temporary directory at %s", DB_PATH_TEMPLATE);
		return NULL;
	}

	return db_path;
}

void
temp_db_path_fini(char *db_path)
{
	D_ASSERT(db_path != NULL);

	int rc = rmdir(db_path);
	if (rc != 0) {
		D_WARN("Failed to remove the temporary directory '%s'", db_path);
	}
}

static int
vos_init(const char *db_path)
{
	int rc;

	rc = vos_standalone_tls_init(DAOS_TGT_TAG);
	if (rc != 0) {
		return rc;
	}

	rc = vos_self_init(db_path, false, BIO_STANDALONE_TGT_ID);
	if (rc) {
		D_ERROR("Error initializing VOS instance");

		vos_standalone_tls_fini();

		return rc;
	}

	return 0;
}

int
dlck_dtx_act_recs_recover(struct dlck_args *args)
{
	char         *db_path = NULL;
	uuid_t        uuid    = {0};
	daos_handle_t poh     = DAOS_HDL_INVAL;
	// daos_handle_t coh = DAOS_HDL_INVAL;
	int           rc;

	db_path = temp_db_path_init();
	if (db_path == NULL) {
		return -1;
	}

	rc = vos_init(db_path);
	if (rc != 0) {
		temp_db_path_fini(db_path);
		return rc;
	}

	/**
	 * VOS_POF_SKIP_UUID_CHECK flag is unsupported ATM
	 */
	rc = vos_pool_open(args->files[0], uuid, flags, &poh);
	assert(rc == 0);

	/** loop over containers */
	vos_iter_param_t        param   = {0};
	struct vos_iter_anchors anchors = {0};

	param.ip_hdl        = poh;
	param.ip_epr.epr_hi = DAOS_EPOCH_MAX;
	param.ip_flags      = VOS_IT_FOR_CHECK;

	struct dlck_array co_uuids = {0};
	dlck_array_init(sizeof(uuid), CO_UUIDS_GROW_BY, &co_uuids);

	rc =
	    vos_iterate(&param, VOS_ITER_COUUID, false, &anchors, cont_list, NULL, &co_uuids, NULL);
	assert(rc == 0);

	assert(co_uuids.da_len > 0);

	char uuid_str[UUID_STR_LEN];
	uuid_unparse(dlck_array_entry(&co_uuids, 0), uuid_str);
	printf("%s\n", uuid_str);

	/** -- */

	// assert(uuid_is_null(args.common.co_uuid) == 0);

	// rc = vos_cont_open(poh, args.common.co_uuid, &coh);
	// assert(rc == 0);

	// vos_tls_getter_set(dlck_tls_get);

	// pthread_t thread;

	// if (pthread_create(&thread, NULL, thread_function, &coh) != 0) {
	// 	perror("Failed to create thread");
	// 	return 1;
	// }

	// Wait for the thread to complete
	// pthread_join(thread, NULL);

	// struct dlck_dtx_rec_array dda = {0};

	// rc = dlck_vos_cont_rec_get_active(coh, &dda);
	// assert(rc == 0);

	// printf("dda.touched = %u\n", dda.touched);

	return 0;
}
