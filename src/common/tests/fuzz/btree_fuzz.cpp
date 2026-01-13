/**
 * (C) Copyright 2026 Hewlett Packard Enterprise Development LP
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */
#define D_LOGFAC DD_FAC(tests)

#include <iostream>
#include <fstream>
#include <getopt.h>
#include <kaitai/kaitaistream.h>

#include "generated/btree_in.h"

#include <daos/common.h>
#include <daos/debug.h>

static struct option btr_ops[] = {
    {"batch", required_argument, NULL, 'b'},
    {NULL, 0, NULL, 0},
};

#define BTR_SHORTOPTS "+b:"

static int
init(btree_in_t::btree_parameters_t *params)
{
	D_ASSERT(params != NULL);

	std::cout << "tree_order = " << (int)params->tree_order() << "\n";
	std::cout << "seed = " << (int)params->seed() << "\n";
	std::cout << "key_num = " << (int)params->key_num() << "\n";
	std::cout << "value_num = " << (int)params->value_num() << "\n";

	return 0;
}

enum op_type_t {
	OP_TYPE_UPDATE = 0,
	OP_TYPE_DELETE,
	OP_TYPE_FETCH,
	OP_TYPE_MAX,
};

static int
ops_exec_update(btree_in_t::btree_op_t *op)
{
	D_ASSERT(op->type() % OP_TYPE_MAX == (int)OP_TYPE_UPDATE);

	printf("update %d %d\n", op->key_id(), op->value_id());

	return 0;
}

static int
ops_exec_delete(btree_in_t::btree_op_t *op)
{
	D_ASSERT(op->type() % OP_TYPE_MAX == (int)OP_TYPE_DELETE);

	printf("delete %d\n", op->key_id());

	return 0;
}

static int
ops_exec_fetch(btree_in_t::btree_op_t *op)
{
	D_ASSERT(op->type() % OP_TYPE_MAX == (int)OP_TYPE_FETCH);

	printf("fetch %d\n", op->key_id());

	return 0;
}

static int
ops_exec_one(btree_in_t::btree_op_t *op)
{
	D_ASSERT(op != NULL);

	enum op_type_t type = (enum op_type_t)(op->type() % OP_TYPE_MAX);

	switch (type) {
	case OP_TYPE_UPDATE:
		return ops_exec_update(op);
	case OP_TYPE_DELETE:
		return ops_exec_delete(op);
	case OP_TYPE_FETCH:
		return ops_exec_fetch(op);
	default:
		D_ERROR("Unknown op type: %" PRIu8 "\n", op->type());
		return -1;
	}
}

static int
ops_exec(std::vector<btree_in_t::btree_op_t *> *ops)
{
	int rc;

	D_ASSERT(ops != NULL);

	for (auto *op : *ops) {
		rc = ops_exec_one(op);
		if (rc != NULL) {
			return rc;
		}
	}

	return 0;
}

static int
batch_exec(const char *file_name)
{
	std::ifstream ifs(file_name, std::ifstream::binary);
	int           rc;

	if (!ifs) {
		D_ERROR("Cannot open file: %s\n", file_name);
		return -1;
	}

	/* wrap file in Kaitai stream and parse */
	kaitai::kstream ks(&ifs);
	btree_in_t      btree_in(&ks);

	rc = init(btree_in.params());
	if (rc != DER_SUCCESS) {
		return rc;
	}

	rc = ops_exec(btree_in.op());

	return 0;
}

int
main(int argc, char **argv)
{
	int   opt;
	char *file_name = NULL;
	int   rc;

	D_CASSERT((int)OP_TYPE_MAX == (int)btree_in_t::consts_t::CONSTS_OPS_NUM);

	rc = daos_debug_init_ex(DAOS_LOG_DEFAULT, DLOG_ERR);
	if (rc != 0) {
		fprintf(stderr, "daos_debug_init_ex() failed: %d\n", rc);
		return rc;
	}

	while ((opt = getopt_long(argc, argv, BTR_SHORTOPTS, btr_ops, NULL)) != -1) {
		if (opt == 'b') {
			file_name = optarg;
		} else if (opt == '?') {
			break;
		}
	}
	if (opt == '?') {
		/* invalid option - error message printed on stderr already */
		goto err;
	} else if (argc != optind) {
		D_ERROR("Cannot interpret parameter: \"%s\" at optind: %d.\n", argv[optind],
			optind);
		goto err;
	}

	if (file_name == NULL) {
		D_ERROR("No batch file provided.\n");
		goto err;
	}

	rc = batch_exec(file_name);

err:
	daos_debug_fini();

	return rc;
}
