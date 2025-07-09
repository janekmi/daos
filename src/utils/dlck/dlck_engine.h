/**
 * (C) Copyright 2025 Hewlett Packard Enterprise Development LP
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */

#ifndef __DLCK_ENGINE__
#define __DLCK_ENGINE__

#include <abt.h>

#include "dlck_args.h"
struct dlck_ult {
	ABT_thread thread;
};

struct dlck_xstream {
	ABT_xstream     xstream;
	ABT_pool        pool;

	int             tgt_id;
	struct dlck_ult nvme_poll;
	ABT_eventual    nvme_poll_done;
};

struct dlck_engine {
	unsigned             targets;
	struct dlck_xstream *xss;
	ABT_mutex            open_mtx;
};

typedef void (*dlck_ult_func)(void *arg);

/**
 * XXX doc missing
 */
int
dlck_engine_start(struct dlck_args_engine *args, struct dlck_engine **engine_ptr);
int
dlck_engine_stop(struct dlck_engine *engine);
int
dlck_engine_xstream_init(struct dlck_xstream *xs);
int
dlck_engine_xstream_fini(struct dlck_xstream *xs);

/** dlck_abt.c */

/**
 * Initialize ABT as it is about to be used by the \p engine.
 *
 * \param[out]	engine	Engine for which ABT is initialized for.
 *
 * \retval DER_SUCCESS	Success.
 * \retval -DER_*	Error.
 */
int
dlck_abt_init(struct dlck_engine *engine);

/**
 * Just create an ABT execution stream.
 *
 * \param[out]	xs	Where the created execution stream will be stored.
 *
 * \retval DER_SUCCESS	Success.
 * \retval -DER_*	Error.
 */
int
dlck_xstream_create(struct dlck_xstream *xs);

/**
 * Create an ABT thread (ULT).
 *
 * \param[in]	pool	Pool to put the created ULT in.
 * \param[in]	func	Function to start on the created ULT.
 * \param[in]	arg	Argument pointer for the function.
 * \param[out]	ult	Created ULT.
 *
 * \retval DER_SUCCESS	Success.
 * \retval -DER_*	Error.
 */
int
dlck_ult_create(ABT_pool pool, dlck_ult_func func, void *arg, struct dlck_ult *ult);

typedef int (*arg_alloc_fn_t)(struct dlck_engine *engine, int idx, void *input_arg,
			      void **output_arg);
typedef int (*arg_free_fn_t)(void **arg);

int
dlck_engine_exec_all(struct dlck_engine *engine, dlck_ult_func exec_one,
		     arg_alloc_fn_t arg_alloc_fn, void *input_arg, arg_free_fn_t arg_free_fn);

#endif /** __DLCK_ENGINE__ */
