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

/** mocks */

struct argp_state  Argp_state;
#define MOCK_ARGP_STATE (&Argp_state)
struct dlck_file File;
#define MOCK_ARG ((void *)0xDEADBEEF)
struct dlck_args_files Args;

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
	if (rc == DER_SUCCESS) {
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

/* Setup for dlck_args_files */
static int
setup(void **state)
{
	args_files_init(&Args);
	Argp_state.input = &Args;
	*state           = &Args;
	return 0;
}

/** tests */

static void
test_check_should_fail_if_no_files(void **unused)
{
	expect_value(__wrap_argp_failure, status, EINVAL);
	expect_value(__wrap_argp_failure, errnum, EINVAL);

	int rc = args_files_check(MOCK_ARGP_STATE, &Args);
	assert_int_equal(rc, EINVAL);
}

static void
test_check_should_succeed_with_file(void **state)
{
	struct dlck_args_files *args = *state;

	d_list_add_tail(&File.link, &args->list);

	int rc = args_files_check(MOCK_ARGP_STATE, args);
	assert_int_equal(rc, 0);
}

static void
test_parser_key_init(void **state)
{
	struct dlck_args_files args;
	args_files_init(&args);
	struct dlck_file *dummy;
	D_ALLOC_PTR(dummy);
	d_list_add_tail(&dummy->link, &args.list);
	assert_false(d_list_empty(&args.list));

	Argp_state.input = &args;

	int rc = argp_file.parser(ARGP_KEY_INIT, NULL, &Argp_state);
	assert_int_equal(rc, 0);

	assert_true(d_list_empty(&args.list));

	D_FREE(dummy);
}

static void
test_parser_end_without_files_triggers_failure(void **unused)
{
	expect_value(__wrap_argp_failure, status, EINVAL);
	expect_value(__wrap_argp_failure, errnum, EINVAL);

	Argp_state.input = &Args;
	int rc           = argp_file.parser(ARGP_KEY_END, NULL, &Argp_state);
	assert_int_equal(rc, EINVAL);
}

static void
test_parser_end_with_files_returns_zero(void **state)
{
	struct dlck_args_files *args = *state;

	struct dlck_file       *f;
	D_ALLOC_PTR(f);
	d_list_add_tail(&f->link, &args->list);

	Argp_state.input = args;
	int rc           = argp_file.parser(ARGP_KEY_END, NULL, &Argp_state);

	assert_int_equal(rc, 0);

	D_FREE(f);
}

static void
test_parser_success_and_fini_are_noop(void **state)
{
	struct dlck_args_files *args = *state;

	struct dlck_file       *d1, *d2;
	D_ALLOC_PTR(d1);
	D_ALLOC_PTR(d2);
	d_list_add_tail(&d1->link, &args->list);
	d_list_add_tail(&d2->link, &args->list);

	assert_false(d_list_empty(&args->list));

	Argp_state.input = args;

	/* SUCCESS */
	int rc1 = argp_file.parser(ARGP_KEY_SUCCESS, NULL, &Argp_state);
	assert_int_equal(rc1, 0);
	assert_false(d_list_empty(&args->list));

	/* FINI */
	int rc2 = argp_file.parser(ARGP_KEY_FINI, NULL, &Argp_state);
	assert_int_equal(rc2, 0);
	assert_false(d_list_empty(&args->list));

	D_FREE(d1);
	D_FREE(d2);
}

static void
test_parser_should_add_file_to_list(void **state)
{
	will_return(parse_file, 0);

	int rc = argp_file.parser(KEY_FILES, MOCK_ARG, &Argp_state);
	assert_int_equal(rc, 0);

	assert_false(d_list_empty(&Args.list));

	struct dlck_file *file = d_list_entry(Args.list.next, struct dlck_file, link);
	assert_ptr_equal(file, &File);
}

static void
test_key_files_parse_file_fails(void **state)
{
	struct dlck_args_files args;
	args_files_init(&args);

	will_return(parse_file, EINVAL);

	int rc = argp_file.parser(KEY_FILES, MOCK_ARG, &Argp_state);
	assert_int_equal(rc, EINVAL);
	assert_true(d_list_empty(&args.list));
}

static void
test_parser_unknown_key_should_return_zero(void **state)
{
	struct dlck_args_files *args = *state;
	Argp_state.input             = args;

	int rc = argp_file.parser(9999, NULL, &Argp_state);
	assert_int_equal(rc, ARGP_ERR_UNKNOWN);
}

static void
test_files_free_empty_list(void **unused)
{
	assert_true(d_list_empty(&Args.list));
	dlck_args_files_free(&Args);
}

static void
test_free_should_cleanup_list(void **state)
{
	struct dlck_args_files *args = *state;

	struct dlck_file       *file;
	D_ALLOC_PTR(file);

	d_list_add_tail(&file->link, &args->list);

	dlck_args_files_free(args);
}

static void
test_free_should_cleanup_multiple_files(void **state)
{
	struct dlck_args_files *args = *state;

	for (int i = 0; i < 3; i++) {
		struct dlck_file *file;
		D_ALLOC_PTR(file);
		d_list_add_tail(&file->link, &args->list);
	}

	dlck_args_files_free(args);
	assert_true(d_list_empty(&args->list));
}

static const struct CMUnitTest tests_all[] = {
    {"DARG100: check fail", test_check_should_fail_if_no_files, setup, NULL},
    {"DARG101: check success", test_check_should_succeed_with_file, setup, NULL},
    {"DARG102: key init", test_parser_key_init, setup, NULL},
    {"DARG103: key end fail", test_parser_end_without_files_triggers_failure, setup, NULL},
    {"DARG104: key end", test_parser_end_with_files_returns_zero, setup, NULL},
    {"DARG105: key success and fini", test_parser_success_and_fini_are_noop, setup, NULL},
    {"DARG106: parser add", test_parser_should_add_file_to_list, setup, NULL},
    {"DARG107: parser fail", test_key_files_parse_file_fails, setup, NULL},
    {"DARG108: unknown key", test_parser_unknown_key_should_return_zero, setup, NULL},
    {"DARG109: free empty", test_files_free_empty_list, setup, NULL},
    {"DARG110: free list", test_free_should_cleanup_list, setup, NULL},
    {"DARG111: free list multiple", test_free_should_cleanup_multiple_files, setup, NULL},
};

int
main(void)
{
	const char *test_name = "dlck_args_files.c tests";

	return cmocka_run_group_tests_name(test_name, tests_all, NULL, NULL);
}
