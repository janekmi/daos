/**
 * (C) Copyright 2025 Hewlett Packard Enterprise Development LP
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */

#include <daos/mem.h>
#include <daos_srv/dlck.h>
#include <daos_srv/mgmt_tgt_common.h>
#include <daos_srv/vos.h>

#include "../dlck_args.h"
#include "../dlck_bitmap.h"
#include "../dlck_engine.h"
#include "../dlck_pool.h"
#include "../dlck_print.h"
#include "../dlck_report.h"

static int
pool_process(struct xstream_arg *xa, struct dlck_file *file, struct dlck_print *main_dp,
	     struct dlck_print *dp)
{
	char         *path;
	daos_handle_t poh;
	int           rc_abt;
	int           rc;

	/** generate a VOS file path */
	rc = ds_mgmt_file(xa->ctrl->engine.storage_path, file->po_uuid, VOS_FILE, &xa->xs->tgt_id,
			  &path);
	if (rc != DER_SUCCESS) {
		rc = -DER_NOMEM;
		DLCK_PRINTF_ERR(dp, "VOS file path allocation failed: " DF_RC "\n", DP_RC(xa->rc));
		return rc;
	}

	/** cannot concurrently ask sys_db for details required to open a pool */
	rc_abt = ABT_mutex_lock(xa->engine->open_mtx);
	if (rc_abt != ABT_SUCCESS) {
		rc = dss_abterr2der(rc_abt);
		DLCK_PRINTF_ERR(
		    dp, "Failed to lock synchronization mutex while opening the pool: " DF_RC "\n",
		    DP_RC(xa->rc));
		return rc;
	}

	rc = vos_pool_open_metrics(path, file->po_uuid, DLCK_POOL_OPEN_FLAGS, NULL, dp, &poh);
	if (rc == DER_SUCCESS) {
		(void)vos_pool_close(poh);
	}
	D_FREE(path);

	/** unlock ASAP */
	rc_abt = ABT_mutex_unlock(xa->engine->open_mtx);

	if (rc != DER_SUCCESS) {
		DLCK_PRINTF_ERR(main_dp, "[%d] Pool %s check failed: " DF_RC "\n", xa->xs->tgt_id,
				path, DP_RC(xa->rc));
		/** ignore a possible error from the unlock */
		return rc;
	}

	/** unlock error is an error */
	if (rc_abt != ABT_SUCCESS) {
		rc = dss_abterr2der(rc_abt);
		DLCK_PRINTF_ERR(
		    dp,
		    "Failed to unlock synchronization mutex while opening the pool: " DF_RC "\n",
		    DP_RC(xa->rc));
		return rc;
	}

	return DER_SUCCESS;
}

#define DLCK_POOL_CHECK_RESULT_PREFIX_FMT "[%d] pool " DF_UUIDF "check result: "

static void
exec_one(void *arg)
{
	struct xstream_arg *xa = arg;
	struct dlck_file *file;
	struct dlck_print  *main_dp = &xa->ctrl->print;
	struct dlck_print   dp;
	int                 rc;
	int                 rc2;

	rc = dlck_engine_xstream_init(xa->xs);
	if (rc != DER_SUCCESS) {
		xa->rc = rc;
		(void)dlck_xstream_progress_end(xa, main_dp);
		return;
	}

	d_list_for_each_entry(file, &xa->ctrl->files.list, link) {
		/** do not process the given file if the target is excluded */
		if (dlck_bitmap_isclr32(file->targets_bitmap, xa->xs->tgt_id)) {
			rc = dlck_xstream_progress_inc(xa, main_dp);
			if (rc != DER_SUCCESS) {
				dlck_xstream_set_rc(xa, rc);
				break;
			}
			continue;
		}

		rc = dlck_print_worker_init(xa->ctrl->log_dir, file->po_uuid, xa->xs->tgt_id, main_dp, &dp);
		if (rc != DER_SUCCESS) {
			dlck_xstream_set_rc(xa, rc);
			(void)dlck_xstream_progress_end(xa, main_dp);
			break;
		}

		rc = pool_process(xa, file, main_dp, &dp);
		if (rc != DER_SUCCESS) {
			dlck_xstream_set_rc(xa, rc);
			DLCK_PRINTF_ERR(main_dp, DLCK_POOL_CHECK_RESULT_PREFIX_FMT DF_RC "\n",
					xa->xs->tgt_id, DP_UUID(file->po_uuid), DP_RC(rc));
			/** Continue regardless of the result. */
		} else {
			DLCK_PRINTF(main_dp, DLCK_POOL_CHECK_RESULT_PREFIX_FMT DLCK_OK_SUFFIX "\n",
				    xa->xs->tgt_id, DP_UUID(file->po_uuid));
		}

		dlck_print_worker_fini(&dp);

		rc2 = dlck_xstream_progress_inc(xa, main_dp);
		if (rc2 != DER_SUCCESS) {
			if (rc == DER_SUCCESS) {
				rc = rc2;
				dlck_xstream_set_rc(xa, rc);
			}
			break;
		}
	}

	if (xa->rc != DER_SUCCESS) {
		(void)dlck_engine_xstream_fini(xa->xs);
		return;
	}

	xa->rc = dlck_engine_xstream_fini(xa->xs);
}

static int
wait_all(struct dlck_engine *engine, struct dlck_exec *de, unsigned progress_max,
	 struct dlck_print *dp)
{
	unsigned *progress;
	unsigned *progress_cache;
	bool      change;
	bool      all_concluded;
	int       rc = DER_SUCCESS;

	D_ALLOC_ARRAY(progress, engine->targets);
	if (progress == NULL) {
		rc = -DER_NOMEM;
		DLCK_PRINTF_ERR(dp, "Failed to allocate a progress array: " DF_RC "\n", DP_RC(rc));
		return rc;
	}

	D_ALLOC_ARRAY(progress_cache, engine->targets);
	if (progress_cache == NULL) {
		rc = -DER_NOMEM;
		DLCK_PRINTF_ERR(dp, "Failed to allocate a progress cache array: " DF_RC "\n",
				DP_RC(rc));
		D_FREE(progress);
		return rc;
	}

	/** initialize the cache so in the first round we will print the progress report */
	memset(progress_cache, 0xf, sizeof(unsigned) * engine->targets);

	/** waiting loop */
	do {
		change        = false;
		all_concluded = true;

		for (int i = 0; i < engine->targets; ++i) {
			rc = dlck_xstream_progress_get(de->ult_args[i], &progress[i]);
			if (rc != DER_SUCCESS) {
				DLCK_PRINTF_ERR(
				    dp, "Failed to read progress for thread %d: " DF_RC "\n", i,
				    DP_RC(rc));
				goto free_arrays;
			}

			/** check for change against the cached value */
			if (progress[i] != progress_cache[i]) {
				change       = true;
				progress_cache[i] = progress[i];
			}

			/** at least one of the threads did not conclude means we have to keep
			 * waiting */
			if (progress[i] < progress_max) {
				all_concluded = false;
			}
		}

		if (change) {
			dlck_report_progress(progress, progress_max, engine->targets, dp);
		}

		sleep(1);

	} while (!all_concluded);

free_arrays:
	D_FREE(progress_cache);
	D_FREE(progress);

	return rc;
}

int
dlck_cmd_check(struct dlck_control *ctrl)
{
	struct dlck_print  *dp                 = &ctrl->print;
	unsigned            file_num           = dlck_args_files_num(&ctrl->files);
	char                log_dir_template[] = "/tmp/dlck_check_XXXXXX";
	struct dlck_engine *engine = NULL;
	struct dlck_exec    de                 = {0};
	int                *rcs;
	int                 rc;

	if (ctrl == NULL) {
		return -DER_INVAL;
	}

	D_ALLOC_ARRAY(rcs, ctrl->engine.targets);
	if (rcs == NULL) {
		DLCK_PRINT(dp, DLCK_ERROR_INFIX "Out of memory.\n");
		return -DER_NOMEM;
	}

	/** create log dir */
	ctrl->log_dir = mkdtemp(log_dir_template);
	if (ctrl->log_dir == NULL) {
		rc = daos_errno2der(errno);
		DLCK_PRINTF_ERR(dp, "Cannot create log directory: " DF_RC "\n", DP_RC(rc));
		D_FREE(rcs);
		return rc;
	}
	DLCK_PRINTF(dp, "Log directory: %s\n", ctrl->log_dir);

	/** create pools directories */
	rc = dlck_pool_mkdir_all(ctrl->engine.storage_path, &ctrl->files.list, dp);
	if (rc != DER_SUCCESS) {
		D_FREE(rcs);
		return rc;
	}

	/** start the engine */
	rc = dlck_engine_start(&ctrl->engine, &engine);
	if (rc != DER_SUCCESS) {
		DLCK_PRINTF_ERR(dp, "Cannot start the engine: " DF_RC "\n", DP_RC(rc));
		D_FREE(rcs);
		return rc;
	}
	DLCK_PRINT(dp, "Engine started succesfully.\n");

	rc = dlck_engine_exec_all_async(engine, exec_one, dlck_engine_xstream_arg_alloc, ctrl,
					dlck_engine_xstream_arg_free, &de);
	if (rc != DER_SUCCESS) {
		DLCK_PRINTF_ERR(dp, "Cannot start execution ULTs: " DF_RC "\n", DP_RC(rc));
		(void)dlck_engine_stop(engine);
		return rc;
	}
	DLCK_PRINT(dp, "Targets started succesfully.\n");

	rc = wait_all(engine, &de, file_num, dp);

	rc = dlck_engine_join_all(engine, &de, rcs);
	if (rc != DER_SUCCESS) {
		DLCK_PRINTF_ERR(dp, "Cannot stop execution ULTs: " DF_RC "\n", DP_RC(rc));
		return rc;
	}

	dlck_report_results(rcs, engine->targets, dp);

	D_FREE(rcs);

	return dlck_engine_stop(engine);
}
