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

void
args_files_init(struct dlck_args_files *args);
int
		   args_files_check(struct argp_state *state, struct dlck_args_files *args);
extern struct argp argp_file;

/** mocks & globals */

struct argp_state  Argp_state;
#define MOCK_ARGP_STATE (&Argp_state)
struct dlck_file File;
#define MOCK_ARG ((void *)0xDEADBEEF)
struct dlck_args_files Args;

#define MOCK_KEY_UKNOWN 9999

void
__wrap_argp_failure(struct argp_state *state, int status, int errnum, const char *fmt, ...)
{
	assert_ptr_equal(state, MOCK_ARGP_STATE);
	check_expected(status);
	check_expected(errnum);
	assert_non_null(fmt);
}

int
parse_file(const char *arg, struct argp_state *state, struct dlck_file **file_ptr)
{
	assert_ptr_equal(arg, MOCK_ARG);
	assert_ptr_equal(state, MOCK_ARGP_STATE);
	assert_non_null(file_ptr);
	int rc = mock_type(int);
	if (rc == 0) {
		memset(&File, 0, sizeof(File));
		*file_ptr = &File;
	}
	return rc;
}

void *
__wrap_d_calloc(size_t count, size_t eltsize)
{
	return test_calloc(count, eltsize);
}

void
__wrap_d_free(void *ptr)
{
	test_free(ptr);
}

/** setup (no teardown necessary) */
static int
setup(void **unused)
{
	args_files_init(&Args);
	Argp_state.input = &Args;
	return 0;
}

/** tests */

static void
test_check_no_files_fail(void **unused)
{
	expect_value(__wrap_argp_failure, status, EINVAL);
	expect_value(__wrap_argp_failure, errnum, EINVAL);

	int rc = args_files_check(MOCK_ARGP_STATE, &Args);
	assert_int_equal(rc, EINVAL);
}

static void
test_check_success(void **unused)
{
	d_list_add_tail(&File.link, &Args.list);

	int rc = args_files_check(MOCK_ARGP_STATE, &Args);
	assert_int_equal(rc, 0);
}

static void
test_parser_KEY_INIT(void **unused)
{
	memset(&Args, 0xf, sizeof(Args));

	int rc = argp_file.parser(ARGP_KEY_INIT, NULL, &Argp_state);
	assert_int_equal(rc, 0);

	assert_true(d_list_empty(&Args.list));
}

static void
test_parser_KEY_END_without_files_fails(void **unused)
{
	expect_value(__wrap_argp_failure, status, EINVAL);
	expect_value(__wrap_argp_failure, errnum, EINVAL);

	Argp_state.input = &Args;
	int rc           = argp_file.parser(ARGP_KEY_END, NULL, &Argp_state);
	assert_int_equal(rc, EINVAL);
}

static void
test_parser_KEY_END_with_files_success(void **unused)
{
	d_list_add_tail(&File.link, &Args.list);

	int rc = argp_file.parser(ARGP_KEY_END, NULL, &Argp_state);

	assert_int_equal(rc, 0);
}

static void
helper_KEY_is_noop(int key)
{
	d_list_add_tail(&File.link, &Args.list);
	int rc = argp_file.parser(key, NULL, &Argp_state);
	assert_int_equal(rc, 0);
	assert_false(d_list_empty(&Args.list));
}

static void
test_parser_KEY_SUCCESS_is_noop(void **unused)
{
	helper_KEY_is_noop(ARGP_KEY_SUCCESS);
}

static void
test_parser_KEY_FINI_is_noop(void **unused)
{
	helper_KEY_is_noop(ARGP_KEY_FINI);
}

static void
test_KEY_FILE_success(void **unused)
{
	will_return(parse_file, 0);

	int rc = argp_file.parser(KEY_FILES, MOCK_ARG, &Argp_state);
	assert_int_equal(rc, 0);

	assert_false(d_list_empty(&Args.list));

	struct dlck_file *file = d_list_entry(Args.list.next, struct dlck_file, link);
	assert_ptr_equal(file, &File);
}

static void
test_KEY_FILE_fail(void **unused)
{
	will_return(parse_file, EINVAL);

	int rc = argp_file.parser(KEY_FILES, MOCK_ARG, &Argp_state);
	assert_int_equal(rc, EINVAL);
	assert_true(d_list_empty(&Args.list));
}

static void
test_KEY_UNKNOWN_fail(void **unused)
{
	int rc = argp_file.parser(MOCK_KEY_UKNOWN, NULL, &Argp_state);
	assert_int_equal(rc, ARGP_ERR_UNKNOWN);
}

static void
test_files_free_empty_list(void **unused)
{
	assert_true(d_list_empty(&Args.list));
	dlck_args_files_free(&Args);
	assert_true(d_list_empty(&Args.list));
}

static void
helper_files_free(int file_num)
{
	for (int i = 0; i < file_num; i++) {
		struct dlck_file *file;
		D_ALLOC_PTR(file);
		d_list_add_tail(&file->link, &Args.list);
	}

	dlck_args_files_free(&Args);
	assert_true(d_list_empty(&Args.list));
}

static void
test_files_free_one_file(void **unused)
{
	helper_files_free(1);
}

static void
test_files_free_three_files(void **state)
{
	helper_files_free(3);
}

static const struct CMUnitTest tests_all[] = {
    {"ARGFILES100: args_files_check() no files", test_check_no_files_fail, setup, NULL},
    {"ARGFILES101: args_files_check() success", test_check_success, setup, NULL},
    {"ARGFILES102: parser ARGP_KEY_INIT", test_parser_KEY_INIT, setup, NULL},
    {"ARGFILES103: parser ARGP_KEY_END no files", test_parser_KEY_END_without_files_fails, setup, NULL},
    {"ARGFILES104: parser ARGP_KEY_END success", test_parser_KEY_END_with_files_success, setup, NULL},
	{"ARGFILES105: parser ARGP_KEY_SUCCESS noop", test_parser_KEY_SUCCESS_is_noop, setup, NULL},
    {"ARGFILES106: parser ARGP_KEY_FINI noop", test_parser_KEY_FINI_is_noop, setup, NULL},
	{"ARGFILES107: KEY_FILES success", test_KEY_FILE_success, setup, NULL},
    {"ARGFILES108: KEY_FILES fail", test_KEY_FILE_fail, setup, NULL},
    {"ARGFILES109: unknown key", test_KEY_UNKNOWN_fail, setup, NULL},
    {"ARGFILES110: dlck_args_files_free() no files", test_files_free_empty_list, setup, NULL},
    {"ARGFILES111: dlck_args_files_free() one file", test_files_free_one_file, setup, NULL},
    {"ARGFILES112: dlck_args_files_free() multiple files", test_files_free_three_files, setup, NULL},
};

int
main(void)
{
	const char *test_name = "dlck_args_files.c tests";

	return cmocka_run_group_tests_name(test_name, tests_all, NULL, NULL);
}
