/**
 * (C) Copyright 2016-2023 Intel Corporation.
 * (C) Copyright 2025 Hewlett Packard Enterprise Development LP.
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */
/**
 * This file is part of vos
 * src/vos/tests/vos_tests.c
 * Launcher for all tests
 */
#define D_LOGFAC DD_FAC(tests)

#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include "vts_common.h"
#include "vts_io.h"
#include <cmocka.h>
#include <getopt.h>
#include <daos_srv/vos.h>
#include <vos_internal.h>

#define FORCE_CSUM         0x1001
#define FORCE_NO_ZERO_COPY 0x1002

#define FOREACH_OTYPE(ACTION)                                                                      \
	ACTION(DAOS_OT_MULTI_HASHED)                                                               \
	ACTION(DAOS_OT_DKEY_UINT64)                                                                \
	ACTION(DAOS_OT_AKEY_UINT64)                                                                \
	ACTION(DAOS_OT_MULTI_UINT64)                                                               \
	ACTION(DAOS_OT_DKEY_LEXICAL)                                                               \
	ACTION(DAOS_OT_AKEY_LEXICAL)                                                               \
	ACTION(DAOS_OT_MULTI_LEXICAL)

#define OT_ENUM_VALUE(otype)   otype,

#define OT_HELP_MESSAGE(otype) print_message("        %d: " #otype "\n", (otype));

static void
print_usage()
{
	print_message("Use one of these opt(s) for specific test\n");
	print_message("vos_tests -p|--pool\n");
	print_message("vos_tests -c|--container\n");
	print_message("vos_tests -i|--io <otype>\n");
	print_message("    otype:\n");
	FOREACH_OTYPE(OT_HELP_MESSAGE);
	print_message("vos_tests -d |--discard\n");
	print_message("vos_tests -a |--aggregate\n");
	print_message("vos_tests -X|--dtx\n");
	print_message("vos_tests -l|--ilog\n");
	print_message("vos_tests -z|--csum\n");
	print_message("vos_tests -A|--all <size>\n");
	print_message("vos_tests -m|--punch_model\n");
	print_message("vos_tests -C|--mvcc\n");
	print_message("vos_tests -w|--wal\n");
	print_message("vos_tests -r|--run_vos_cmd <command>\n");
	print_message("-S|--storage <storage path>\n");
	print_message("vos_tests -h|--help\n");
	print_message("Default <vos_tests> runs all tests\n");
	print_message("The following options can be used with any of the above:\n");
	print_message("  -f|--filter <filter>\n");
	print_message("  -e|--exclude <filter>\n");
	print_message("  --force_checksum\n");
	print_message("  --force_no_zero_copy\n");
}

void
test_args_init(struct io_test_args *args, uint64_t pool_size);

#define SRAND_SEED 1743171631

static inline int
run_all_tests(int keys)
{
	daos_epoch_t        epoch = d_hlc_get();
	uint64_t            dkey_buf;
	daos_key_t          dkey;
	daos_iod_t          iod = {0};
	uint64_t            akey_buf;
	daos_key_t          akey;
	char               *value = "Aloha";
	d_sg_list_t         sgl;
	int                 rc;

	struct io_test_args args;
	srand(SRAND_SEED);
	test_args_init(&args, VPOOL_SIZE);

	vts_key_gen((char *)&dkey_buf, sizeof(dkey_buf), true, &args);
	set_iov(&dkey, (char *)&dkey_buf, true);
	vts_key_gen((char *)&akey_buf, sizeof(akey_buf), false, &args);
	set_iov(&akey, (char *)&akey_buf, true);
	iod.iod_name  = akey;
	iod.iod_type  = DAOS_IOD_SINGLE;
	iod.iod_recxs = NULL;
	iod.iod_nr    = 1;
	iod.iod_size  = strlen(value);
	rc            = d_sgl_init(&sgl, 1);
	assert_rc_equal(rc, 0);
	d_iov_set(&sgl.sg_iovs[0], (void *)value, iod.iod_size);

	rc = vos_obj_update(args.ctx.tc_co_hdl, args.oid, epoch, 0, 0, &dkey, 1, &iod, NULL, &sgl);
	assert_int_equal(rc, 0);

	d_sgl_fini(&sgl, false);

	/* cleanup omitted intentionally to keep the created files */

	return 0;
}

int
main(int argc, char **argv)
{
	int                  rc             = 0;
	int                  nr_failed      = 0;
	int                  opt            = 0;
	int                  index          = 0;
	const char          *vos_command    = NULL;
	const char          *short_options  = "apcdglzni:mXA:S:hf:e:tCwr:";
	static struct option long_options[] = {
	    {"all", required_argument, 0, 'A'},
	    {"pool", no_argument, 0, 'p'},
	    {"container", no_argument, 0, 'c'},
	    {"io", required_argument, 0, 'i'},
	    {"discard", no_argument, 0, 'd'},
	    {"aggregate", no_argument, 0, 'a'},
	    {"dtx", no_argument, 0, 'X'},
	    {"punch_model", no_argument, 0, 'm'},
	    {"garbage_collector", no_argument, 0, 'g'},
	    {"ilog", no_argument, 0, 'l'},
	    {"epoch_cache", no_argument, 0, 't'},
	    {"mvcc", no_argument, 0, 'C'},
	    {"wal", no_argument, 0, 'w'},
	    {"csum", no_argument, 0, 'z'},
	    {"run_vos_cmd", required_argument, 0, 'r'},
	    {"help", no_argument, 0, 'h'},
	    {"filter", required_argument, 0, 'f'},
	    {"exclude", required_argument, 0, 'e'},
	    {"storage", required_argument, 0, 'S'},
	    {"force_csum", no_argument, 0, FORCE_CSUM},
	    {"force_no_zero_copy", no_argument, 0, FORCE_NO_ZERO_COPY},
	    {NULL},
	};

	d_register_alt_assert(mock_assert);

	rc = daos_debug_init(DAOS_LOG_DEFAULT);
	if (rc) {
		print_error("Error initializing debug system\n");
		return rc;
	}

	gc            = 0;
	bool test_run = false;

	while ((opt = getopt_long(argc, argv, short_options, long_options, &index)) != -1) {
		switch (opt) {
		case 'S':
			if (strnlen(optarg, STORAGE_PATH_LEN) >= STORAGE_PATH_LEN) {
				print_error("%s is longer than STORAGE_PATH_LEN.\n", optarg);
				goto exit_0;
			}
			strncpy(vos_path, optarg, STORAGE_PATH_LEN);
			break;
		case 'h':
			print_usage();
			goto exit_0;

		case 'e':
#if CMOCKA_FILTER_SUPPORTED == 1 /** requires cmocka 1.1.5 */
			cmocka_set_skip_filter(optarg);
#else
			D_PRINT("filter not enabled");
#endif

			break;
		case 'f':
#if CMOCKA_FILTER_SUPPORTED == 1 /** requires cmocka 1.1.5 */
		{
			/** Add wildcards for easier filtering */
			char filter[sizeof(optarg) + 2];

			sprintf(filter, "*%s*", optarg);
			cmocka_set_test_filter(filter);
			printf("Test filter: %s\n", filter);
		}
#else
			D_PRINT("filter not enabled");
#endif
		break;
		case FORCE_CSUM:
			g_force_checksum = true;
			break;
		case FORCE_NO_ZERO_COPY:
			g_force_no_zero_copy = true;
			break;
		default:
			break;
		}
	}

	if (vos_path[0] == '\0') {
		strcpy(vos_path, "/mnt/daos");
	}

	rc = vos_self_init(vos_path, true, BIO_STANDALONE_TGT_ID);
	if (rc) {
		print_error("Error initializing VOS instance\n");
		goto exit_0;
	}

	index  = 0;
	optind = 0;

	while ((opt = getopt_long(argc, argv, short_options, long_options, &index)) != -1) {
		switch (opt) {
		case 'r':
			if (vos_command != NULL)
				print_error("Only one -r option is supported\n");
			vos_command = optarg;
			test_run    = true;
			break;
		case 'S':
		case 'f':
		case 'e':
		case FORCE_CSUM:
		case FORCE_NO_ZERO_COPY:
			/** already handled */
			break;
		default:
			print_error("Unknown option\n");
			print_usage();
			goto exit_1;
		}
	}

	/** options didn't include specific tests, just run them all */
	if (!test_run)
		nr_failed = run_all_tests(0);

	if (nr_failed)
		print_error("ERROR, %i TEST(S) FAILED\n", nr_failed);
	else
		print_message("\nSUCCESS! NO TEST FAILURES\n");

exit_1:
	vos_self_fini();
exit_0:
	daos_debug_fini();
	return nr_failed;
}
