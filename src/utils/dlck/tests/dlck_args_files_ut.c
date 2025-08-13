/**
 * (C) Copyright 2025 Hewlett Packard Enterprise Development LP
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <uuid/uuid.h>

#include <argp.h>
#include <daos/debug.h>

#include "../dlck_args.h"

/* Helper to create dummy file entry */
static struct dlck_file *
create_dummy_file(const char *uuid_str)
{
	struct dlck_file *file = malloc(sizeof(*file));
	assert_non_null(file);
	uuid_parse(uuid_str, file->po_uuid);
	D_INIT_LIST_HEAD(&file->link);
	return file;
}

/* Setup and teardown for dlck_args_files */
static int
setup(void **state)
{
	struct dlck_args_files *args = malloc(sizeof(*args));
	assert_non_null(args);
	args_files_init(args);
	*state = args;
	return 0;
}

static int
teardown(void **state)
{
	struct dlck_args_files *args = *state;
	dlck_args_files_free(args);
	free(args);
	return 0;
}

static void
test_args_files_init_should_initialize_list(void **state)
{
	struct dlck_args_files *args = *state;
	assert_true(d_list_empty(&args->list));
}

static void
test_args_files_check_should_fail_if_no_files(void **state)
{
	struct dlck_args_files *args       = *state;
	struct argp_state       argp_state = {0};

	int                     rc = args_files_check(&argp_state, args);
	assert_int_not_equal(rc, 0);
}

static void
test_args_files_parser_should_add_file_to_list(void **state)
{
	struct dlck_args_files *args       = *state;
	struct argp_state       argp_state = {.input = args};

	uuid_t                  expected;
	uuid_parse("12345678-1234-1234-1234-123456789abc", expected);

	const char *arg = "12345678-1234-1234-1234-123456789abc,1,2,3";
	int         rc  = args_files_parser(KEY_FILES, (char *)arg, &argp_state);
	assert_int_equal(rc, 0);
	assert_false(d_list_empty(&args->list));
	;

	struct dlck_file *file = d_list_entry(args->list.next, struct dlck_file, link);
	assert_non_null(file);
	assert_memory_equal(file->po_uuid, expected, 16);
}

static void
test_dlck_args_files_free_should_cleanup_list(void **state)
{
	struct dlck_args_files *args = *state;

	struct dlck_file       *file = create_dummy_file("12345678-1234-1234-1234-123456789abc");
	d_list_add_tail(&file->link, &args->list);

	dlck_args_files_free(args);
	assert_true(d_list_empty(&args->list));
}

int
main(void)
{
	const struct CMUnitTest tests[] = {
	    cmocka_unit_test_setup_teardown(test_args_files_init_should_initialize_list, setup,
					    teardown),
	    cmocka_unit_test_setup_teardown(test_args_files_check_should_fail_if_no_files, setup,
					    teardown),
	    cmocka_unit_test_setup_teardown(test_args_files_parser_should_add_file_to_list, setup,
					    teardown),
	    cmocka_unit_test_setup_teardown(test_dlck_args_files_free_should_cleanup_list, setup,
					    teardown),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
