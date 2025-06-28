/**
 * (C) Copyright 2025 Hewlett Packard Enterprise Development LP
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */

#include <libgen.h>
#include <stdlib.h>
#include <stdio.h>
#include <daos/mem.h>
#include <daos/btree_class.h>
#include <gurt/telemetry_producer.h>
#include <daos_srv/vos.h>
#include <daos_srv/dlck.h>
#include <daos_version.h>

#include "dlck_engine.h"

int
dlck_xstream_create(struct dlck_xstream *xs)
{
	int rc;

	rc = ABT_xstream_create(ABT_SCHED_NULL, &xs->xstream);
	if (rc != ABT_SUCCESS) {
		/** XXX translate ABT error */
		return rc;
	}
	rc = ABT_xstream_get_main_pools(xs->xstream, 1, &xs->pool);
	if (rc != ABT_SUCCESS) {
		/** XXX translate ABT error */
		return rc;
	}

	return 0;
}

/**
 * XXX missing teardown
 */
int
dlck_xstream_start(struct dlck_xstream *xs)
{
	// struct dlck_ult ult;
	int rc;

	rc = dlck_xstream_create(xs);
	if (rc != 0) {
		return rc;
	}

	// rc = dlck_ult_create(xs->pool, init_func, xs, &ult);
	// if (rc != 0) {
	// 	return rc;
	// }
	
	// ABT_eventual_wait(xs->rc, (void **)&rc_ptr);
	// rc = *rc_ptr;
	// if (rc != 0) {
	// 	/** XXX translate ABT return code */
	// 	return rc;
	// }

	// rc = ABT_eventual_reset(xs->rc);
	// if (rc != 0) {
	// 	/** XXX translate ABT return code */
	// 	return rc;
	// }
	
	// rc = ABT_thread_join(ult.thread);
	// if (rc != 0) {
	// 	/** XXX translate ABT return code */
	// 	return rc;
	// }

	/** ABT_thread free */
	
	return rc;
}
