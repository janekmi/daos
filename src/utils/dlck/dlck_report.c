/**
 * (C) Copyright 2025 Hewlett Packard Enterprise Development LP
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */
#define D_LOGFAC DD_FAC(dlck)

#include <abt.h>
#include <stdbool.h>
#include <limits.h>

#include <daos_errno.h>
#include <daos_srv/daos_engine.h>

#include "dlck_print.h"
#include "dlck_report.h"

/**
 * Return boolean true if the main output stream is a terminal
 *
 * \param[in]	dpm	Main print utility (custom payload only).
 */
static inline bool
is_tty(struct dlck_print_main *dpm)
{
	return isatty(fileno(dpm->stream)) == 1;
}

/**
 * Check fprintf()/snprintf() return code. Return in case of an error.
 */
#define _PRINTF_CHECK_RC(rc)                                                                       \
	do {                                                                                       \
		if (rc < 0) {                                                                      \
			return -DER_MISC;                                                          \
		}                                                                                  \
	} while (0)

#define DLCK_PROGRESS_BAR_LEN               10
#define DLCK_PROGRESS_BAR_PERCENT_SPACE_LEN (1 + 3 + 1) /** excluding \0 */
/** "[#######...] 100%" */
#define DLCK_PROGRESS_RECORD_MAX_LEN                                                               \
	(1 + DLCK_PROGRESS_BAR_LEN + 1 + DLCK_PROGRESS_BAR_PERCENT_SPACE_LEN + 1)

/**
 * Reports progress for a single target: [#######...] 100%
 *
 * \param[in]	dpm		Main print utility.
 * \param[in]	progress	What has been done.
 * \param[in]	progress_max	What was requested to be done.
 *
 * \retval DER_SUCCESS	Success.
 * \retval -DER_MISC	Print error.
 */
static int
report_target_progress(struct dlck_print_main *dpm, unsigned progress, unsigned progress_max,
		       int tgt_id)
{
	static char record[DLCK_PROGRESS_RECORD_MAX_LEN];
	unsigned    percent = progress * 100 / progress_max;
	int         rc;

	/** When printing to the terminal, we can entertain the user with a simple progress bar. */
	record[0] = '[';
	memset(&record[1], '.', DLCK_PROGRESS_BAR_LEN);
	memset(&record[1], '#', percent / 10);
	record[1 + DLCK_PROGRESS_BAR_LEN] = ']';
	rc                                = snprintf(&record[1 + DLCK_PROGRESS_BAR_LEN + 1],
						     DLCK_PROGRESS_BAR_PERCENT_SPACE_LEN + 1, "%*u%%",
						     DLCK_PROGRESS_BAR_PERCENT_SPACE_LEN - 1, percent);
	/** ignore rc >= maxlen; truncated output is better than no output at all */
	_PRINTF_CHECK_RC(rc);

	rc = fprintf(dpm->stream, "[%d] %s\n", tgt_id, record);
	_PRINTF_CHECK_RC(rc);

	return DER_SUCCESS;
}

#define DLCK_RESULT_FMT "[%d] result: "

static int
report_target_result(struct dlck_print_main *dpm, int tgt_id, int tgt_rc)
{
	int rc;

	if (is_tty(dpm)) {
		/** clear the line */
		rc = fprintf(dpm->stream, "\033[2K");
	}
	if (tgt_rc == DER_SUCCESS) {
		rc = fprintf(dpm->stream, DLCK_RESULT_FMT DLCK_OK_SUFFIX "\n", tgt_id);
	} else {
		rc = fprintf(dpm->stream, DLCK_RESULT_FMT DF_RC "\n", tgt_id, DP_RC(tgt_rc));
	}
	_PRINTF_CHECK_RC(rc);

	return DER_SUCCESS;
}

#define DLCK_PROGRESS_HEADER     "Targets:"
#define DLCK_PROGRESS_HEADER_LEN (sizeof(DLCK_PROGRESS_HEADER) - 1) /** excluding \0 */

/**
 * Produce and provide a simple separator:
 *
 * ========
 */
static inline char *
get_separator()
{
	static char separator[DLCK_PROGRESS_HEADER_LEN] = {0};
	static bool initialized                         = false;

	if (unlikely(!initialized)) {
		memset(separator, '=', DLCK_PROGRESS_HEADER_LEN);
		initialized = true;
	}

	return separator;
}

/**
 * \var Call_count_cache
 *
 * Dedicated to record when the report has been printed for the first time onto tty.
 * It allows to go back to it and update it with time.
 */
static unsigned Call_count_cache = UINT_MAX;

/**
 * Jump to where the report was printed first in order to update it.
 *
 * \param[in]	dpm	Main print utility.
 * \param[in]	targets	Number of targets.
 *
 * \retval DER_SUCCESS	Success.
 * \retval -DER_MISC	Print error.
 */
static int
tty_jump_to_report(struct dlck_print_main *dpm, unsigned targets)
{
	int diff = dpm->call_count - Call_count_cache;
	int rc;

	D_ASSERT(is_tty(dpm));
	D_ASSERT(Call_count_cache != UINT_MAX);

	/** move cursor a few lines up (each target occupies one row; +1 footer) */
	rc = fprintf(dpm->stream, "\033[%uA", targets + 1 + diff);
	_PRINTF_CHECK_RC(rc);

	return DER_SUCCESS;
}

/**
 * Jump back to the end of the output after updating the report.
 *
 * \param[in]	dpm	Main print utility.
 *
 * \retval DER_SUCCESS	Success.
 * \retval -DER_MISC	Print error.
 */
static int
tty_jump_back(struct dlck_print_main *dpm)
{
	int diff = dpm->call_count - Call_count_cache;
	int rc;

	D_ASSERT(is_tty(dpm));
	D_ASSERT(Call_count_cache != UINT_MAX);

	rc = fprintf(dpm->stream, "\033[%dB", diff);
	_PRINTF_CHECK_RC(rc);

	return DER_SUCCESS;
}

/**
 * Print the report header:
 *
 * ========
 * Targets:
 * ========
 *
 * \param[in]	dpm	Main print utility (just the custom payload).
 * \param[in]	targets	Number of targets.
 *
 * \retval DER_SUCCESS	Success.
 * \retval -DER_MISC	Print failed.
 */
static int
report_header(struct dlck_print_main *dpm, unsigned targets)
{
	char *separator   = get_separator();
	int   rc;

	if (Call_count_cache != UINT_MAX) {
		return tty_jump_to_report(dpm, targets);
	} else {
		/** print header */
		rc = fprintf(dpm->stream, "%s\n", separator);
		_PRINTF_CHECK_RC(rc);
		rc = fprintf(dpm->stream, DLCK_PROGRESS_HEADER "\n");
		_PRINTF_CHECK_RC(rc);
		rc = fprintf(dpm->stream, "%s\n", separator);
		_PRINTF_CHECK_RC(rc);

		return DER_SUCCESS;
	}
}

/**
 * Print the report footer:
 *
 * ========
 *
 * \param[in]	dpm	Main print utility (just the custom payload).
 *
 * \retval DER_SUCCESS	Success.
 * \retval -DER_MISC	Print failed.
 */
static int
report_footer(struct dlck_print_main *dpm)
{
	char *separator = get_separator();
	int   rc;

	rc = fprintf(dpm->stream, "%s\n", separator);
	_PRINTF_CHECK_RC(rc);

	if (Call_count_cache != UINT_MAX) {
		return tty_jump_back(dpm);
	} else {
		/** Cache when the report was printed for the first time. */
		++dpm->call_count;
		Call_count_cache = dpm->call_count;
	}

	return DER_SUCCESS;
}

int
dlck_report_progress(unsigned *progress, unsigned progress_max, unsigned targets,
		     struct dlck_print *dp)
{
	struct dlck_print_main *dpm = dlck_print_main_get_custom(dp);
	int                     rc_abt;
	int                     rc;

	if (!is_tty(dpm)) {
		return DER_SUCCESS;
	}

	/** lock the main output stream */
	rc_abt = ABT_mutex_lock(dpm->stream_mutex);
	if (rc_abt != ABT_SUCCESS) {
		rc = dss_abterr2der(rc_abt);
		D_ERROR(DLCK_PRINT_MAIN_LOCK_FAIL_FMT, DP_RC(rc));
		return rc;
	}

	/** print header */
	rc = report_header(dpm, targets);
	if (rc != DER_SUCCESS) {
		return rc;
	}

	/** print records */
	for (int i = 0; i < targets; ++i) {
		rc = report_target_progress(dpm, progress[i], progress_max, i);
		if (rc != DER_SUCCESS) {
			return rc;
		}
	}

	/** print footer */
	rc = report_footer(dpm);
	if (rc != DER_SUCCESS) {
		return rc;
	}

	/** unlock the main output stream */
	rc_abt = ABT_mutex_unlock(dpm->stream_mutex);
	if (rc_abt != ABT_SUCCESS) {
		rc = dss_abterr2der(rc_abt);
		D_ERROR(DLCK_PRINT_MAIN_LOCK_FAIL_FMT, DP_RC(rc));
		return rc;
	}

	return DER_SUCCESS;
}

/**
 * \note This function is called when no other threads are running in parallel. No locks are
 * necessary.
 */
int
dlck_report_results(int *rcs, unsigned targets, struct dlck_print *dp)
{
	struct dlck_print_main *dpm = dlck_print_main_get_custom(dp);
	int                     rc;

	/** print header */
	rc = report_header(dpm, targets);
	if (rc != DER_SUCCESS) {
		return rc;
	}

	/** print records */
	for (int i = 0; i < targets; ++i) {
		rc = report_target_result(dpm, i, rcs[i]);
		if (rc != DER_SUCCESS) {
			return rc;
		}
	}

	/** print footer */
	return report_footer(dpm);
}
