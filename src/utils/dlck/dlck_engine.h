/**
 * (C) Copyright 2025 Hewlett Packard Enterprise Development LP
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */

#ifndef __DLCK_ENGINE__
#define __DLCK_ENGINE__

#include <abt.h>

struct dlck_ult {
	ABT_thread thread;
};

struct dlck_xstream {
	ABT_xstream xstream;
	ABT_pool pool;

	int tgt_id;
	struct dlck_ult nvme_poll;
	ABT_eventual rc_init;
};

struct dlck_engine {
	unsigned targets;
	struct dlck_xstream *xss;
};

/**
 * XXX doc missing
 */
int
dlck_engine_start(struct dlck_args *args, struct dlck_engine **engine_ptr);

#endif /** __DLCK_ENGINE__ */