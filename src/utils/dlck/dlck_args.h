/**
 * (C) Copyright 2025 Hewlett Packard Enterprise Development LP
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */

#ifndef __DLCK_ARGS__
#define __DLCK_ARGS__

enum dlck_cmd { DLCK_CMD_NOT_SET, DLCK_CMD_UNKNOWN, DLCK_CMD_DTX_ACT_RECOVER };

#define DLCK_CMD_DTX_ACT_RECOVER_STR "dtx_act_recs_recover"

struct dlck_args_common {
	bool          write_mode; /** false for a dry run (default) */
	enum dlck_cmd cmd;
};

struct dlck_args {
	struct dlck_args_common common;
	/** Hint: Command-specific options should be placed here. */
};

/**
 * \brief Parse provided argc/argv, validate and write down into \p args state.
 *
 * It may close the calling process if requested for version or help.
 *
 * \param[in]   argc  Length of the \p argv array.
 * \param[in]   argv  Standard list of arguments.
 * \param[out]	args	Parsed arguments.
 */
void
dlck_args_parse(int argc, char *argv[], struct dlck_args *args);

#endif /** __DLCK_ARGS__ */