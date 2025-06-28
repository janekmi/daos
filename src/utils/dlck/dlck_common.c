/**
 * (C) Copyright 2025 Hewlett Packard Enterprise Development LP
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <daos/mem.h>
#include <daos/btree_class.h>
#include <gurt/telemetry_producer.h>
#include <daos_srv/vos.h>
#include <daos_srv/dlck.h>
#include <daos_srv/daos_mgmt_srv.h>
#include <daos_version.h>

#include <libpmemobj.h>

#include "dlck_args.h"
#include "dlck_engine.h"
#include "dlck_common.h"

int
xxx_vos_preallocate(const char *path, uuid_t uuid, daos_size_t scm_size);

int
dlck_pool_mkdir(const char *storage_path, struct dlck_file *file)
{
	char po_uuid[UUID_STR_LEN];
	char *path;
	int rc;

	uuid_unparse(file->po_uuid, po_uuid);

	rc = asprintf(&path, "%s/%s/", storage_path, po_uuid);
	if (rc < 0){
		return rc;
	}

	rc = mkdir(path, 0777);
	D_FREE(path);
	if (rc != 0 && errno != EEXIST) {
		return errno;
	} else {
		return 0;
	}
}

static int
dlck_recreate(const char *path, uuid_t uuid)
{
	struct smd_pool_info *pool_info = NULL;
	int rc;

	rc = smd_pool_get_info(uuid, &pool_info);
	if (rc != 0) {
		return rc;
	}

	rc = xxx_vos_preallocate(path, uuid, pool_info->spi_scm_sz);
	if (rc != 0) {
		goto out;
	}
	
out:
	smd_pool_free_info(pool_info);

	return rc;
}

int
dlck_pool_open(	const char *storage_path, struct dlck_file *file, int tgt_id, daos_handle_t *poh)
{
	char *path;
	char po_uuid[UUID_STR_LEN];
	const unsigned int flags = VOS_POF_EXCL | VOS_POF_EXTERNAL_FLUSH | VOS_POF_FOR_FEATURE_FLAG;
	int rc;

	uuid_unparse(file->po_uuid, po_uuid);

	rc = asprintf(&path, "%s/%s/" VOS_FILE "%d", storage_path, po_uuid, tgt_id);
	if (rc < 0){
		goto fail;
	}

	rc = dlck_recreate(path, file->po_uuid);
	if (rc != 0) {
		goto fail;
	}

	rc = vos_pool_open(path, file->po_uuid, flags, poh);
	if (rc != 0) {
		goto fail;
	}

fail:
	D_FREE(path);

	return rc;
}
