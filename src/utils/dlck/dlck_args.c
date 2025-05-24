/**
 * (C) Copyright 2025 Hewlett Packard Enterprise Development LP
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */

#define D_LOGFAC DD_FAC(dlck)

#include <daos_errno.h>
#include <daos/debug.h>
#include <daos_version.h>
#include <argp.h>

#include "dlck_args.h"

/** set --version */

#define _STRINGIFY(x) #x
#define STRINGIFY(x)  _STRINGIFY(x)

#define DAOS_VERSION_STR                                                                           \
	STRINGIFY(DAOS_VERSION_MAJOR)                                                              \
	"." STRINGIFY(DAOS_VERSION_MINOR) "." STRINGIFY(DAOS_VERSION_FIX)

const char *argp_program_version = "dlck " DAOS_VERSION_STR;

/** all short options and all groups to avoid conflicts */

#define KEY_COMMON_WRITE_MODE     'w'
#define KEY_COMMON_FILE           'f'
#define KEY_COMMON_CO_UUID        'q'
#define KEY_COMMON_JOBS           'j'
#define KEY_COMMON_CMD            'c'

#define GROUP_COMMON              1
#define GROUP_COMMAND             2
#define GROUP_CMDS_OPTS_NOTE      3
#define GROUP_AVAILABLE_CMDS      4
#define GROUP_CMD_DTX_ACT_RECOVER 5
#define GROUP_CMD_TBD             6
#define GROUP_AUTOMAGIC           -1 /** yes, -1 is the last group */

/** argument parsers and helper functions */

static void
args_init(struct dlck_args *args)
{
	memset(args, 0, sizeof(struct dlck_args));
	/** set defaults */
	args->common.write_mode = false; /** dry run */
	args->common.cmd        = DLCK_CMD_NOT_SET;
}

static enum dlck_cmd
command_parse(const char *arg)
{
	if (strcmp(arg, DLCK_CMD_DTX_ACT_RECOVER_STR) == 0) {
		return DLCK_CMD_DTX_ACT_RECOVER;
	}

	return DLCK_CMD_UNKNOWN;
}

static error_t
common_parser(int key, char *arg, struct argp_state *state)
{
	struct dlck_args *args = state->input;

	if (args->common.cmd != DLCK_CMD_NOT_SET) {
		/** no common options beyond this point */
		return ARGP_ERR_UNKNOWN;
	}

	switch (key) {
	case ARGP_KEY_INIT:
		args_init(args);
		break;
	case KEY_COMMON_WRITE_MODE:
		args->common.write_mode = true;
		break;
	case KEY_COMMON_CMD:
		args->common.cmd = command_parse(arg);
		if (args->common.cmd == DLCK_CMD_UNKNOWN) {
			return ARGP_ERR_UNKNOWN;
		}
		break;
	case ARGP_KEY_END:
		/** TBD result validation? */
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

/** helper definitions */

#define OPT_HEADER(HEADER, GROUP) {0, 0, 0, 0, HEADER, GROUP}

#define LIST_ENTRY(CMD, DESC)     {CMD, 0, 0, OPTION_DOC, DESC}

#define CMD_NO_OPTS_STR(CMD)      "The '" CMD "' command does not offer any additional options."

#define CMD_OPTS_STR(CMD)         "The '" CMD "' command's options:"

/** complete help list (in order) */

static char               usage[] = "--cmd=CMD [CMD OPTION...]\n"
				    "--cmd=CMD [CMD OPTION...] -- [FILES...]";
static char               doc[]   = "\nDAOS Local Consistency Checker (dlck)";

static struct argp_option common_options[] = {
    OPT_HEADER("Common options:", GROUP_COMMON),
    /** entries below inherits the group number of the header entry */
    {"write-mode", KEY_COMMON_WRITE_MODE, 0, 0, "Open VOS files in write mode."},
    {"file", KEY_COMMON_FILE, "FILE", 0,
     "VOS file. Valid only if no additional files are specified. Please see usage."},
    {"co-uuid", KEY_COMMON_CO_UUID, "UUID", 0,
     "UUID of a container to process. If not provided all containers are processed."},
    {"jobs", KEY_COMMON_JOBS, "N", 0, "Allow N jobs at once. One job by default."},
    /** kept in a separate group to highligh its special role */
    {"cmd", KEY_COMMON_CMD, "CMD", 0, "Command (Required). Please see available commands below.",
     GROUP_COMMAND},
    OPT_HEADER("Options specific to a given command should be specified after selecting the "
	       "command. Please see --cmd and usage.",
	       GROUP_CMDS_OPTS_NOTE),
    {0}};

static struct argp_option _cmds_list[] = {
    OPT_HEADER("Available commands:", GROUP_AVAILABLE_CMDS),
    /** entries below inherits the group number of the header entry */
    LIST_ENTRY(DLCK_CMD_DTX_ACT_RECOVER_STR, "Active DTX entries' records recovery."),
    {0}};

static struct argp_option dar_options[] = {
    OPT_HEADER(CMD_NO_OPTS_STR(DLCK_CMD_DTX_ACT_RECOVER_STR), GROUP_CMD_DTX_ACT_RECOVER), {0}};

static struct argp_option _automagic[] = {OPT_HEADER("Other options:", GROUP_AUTOMAGIC), {0}};

/** glue everything together */

static struct argp        cmds_list = {_cmds_list, NULL};

static struct argp        dar_argp = {dar_options, NULL};

static struct argp        automagic = {_automagic, NULL};

static struct argp_child  children[] = {{&cmds_list}, {&dar_argp}, {&automagic}, {0}};

static struct argp        argp = {common_options, common_parser, usage, doc, children};

/** entry point */

void
dlck_args_parse(int argc, char *argv[], struct dlck_args *args)
{
	error_t ret = argp_parse(&argp, argc, argv, ARGP_IN_ORDER, 0, args);

	if (ret != 0) {
		D_ERROR("Parsing arguments failed: %d", ret);
		exit(ret);
	}
}
