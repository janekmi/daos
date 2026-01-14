/**
 * (C) Copyright 2026 Hewlett Packard Enterprise Development LP
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */
#define D_LOGFAC DD_FAC(tests)

#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <getopt.h>
#include <kaitai/kaitaistream.h>

#include "generated/btree_in.h"

#include <daos/common.h>
#include <daos/debug.h>
#include <daos/btree.h>

/** options */

static struct option btr_ops[] = {
    {"batch", required_argument, NULL, 'b'},
    {NULL, 0, NULL, 0},
};

#define BTR_SHORTOPTS "+b:"

/** btree test class definitions */

extern btr_ops_t ik_ops;

#define IK_TREE_CLASS 100

struct test_state {
	daos_handle_t toh;
	std::vector<uint64_t>    keys;
	std::vector<std::string> values;
	std::map<uint64_t, std::string> tree;
};

#define MAX_KEY_NUM   256
#define MAX_VALUE_NUM 256
#define MAX_VALUE_LEN 32

std::string
random_string()
{
	static const char charset[] = "0123456789"
				      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
				      "abcdefghijklmnopqrstuvwxyz";

	std::string       s;
	unsigned          length = random() % MAX_VALUE_LEN;

	s.reserve(length);

	for (int i = 0; i < length; ++i) {
		s += charset[random() % (ARRAY_SIZE(charset) - 1)];
	}

	return s;
}

static int
init(btree_in_t::btree_parameters_t *params, struct test_state *ts)
{
	D_ASSERT(params != NULL);

	/* BTR_FEAT_DYNAMIC_ROOT ? */
	uint64_t     tree_class_feats = BTR_FEAT_EMBED_FIRST | BTR_FEAT_UINT_KEY;
	uint64_t     tree_feats       = 0;
	unsigned int tree_order       = params->tree_order();
	umem_attr    uma              = {UMEM_CLASS_VMEM, 0};
	unsigned     key_num;
	unsigned     value_num;

	int          rc = dbtree_class_register(IK_TREE_CLASS, tree_class_feats, &ik_ops);
	D_ASSERT(rc == 0);

	tree_feats = 0;

	rc = dbtree_create(IK_TREE_CLASS, tree_feats, tree_order, &uma, NULL, &ts->toh);
	D_ASSERT(rc == 0);

	srand(params->seed());

	key_num   = rand() % MAX_KEY_NUM;
	value_num = rand() % MAX_VALUE_NUM;

	for (int i = 0; i < key_num; ++i) {
		ts->keys.emplace_back(rand());
		// printf("[%d] = %" PRIu64 "\n", i, ts->keys.back());
	}

	for (int i = 0; i < value_num; ++i) {
		ts->values.emplace_back(random_string());
		// printf("[%d] = %s\n", i, ts->values.back().c_str());
	}

	return 0;
}

enum op_type_t {
	OP_TYPE_UPDATE = 0,
	OP_TYPE_DELETE,
	OP_TYPE_FETCH,
	OP_TYPE_MAX,
};

static int
ops_exec_update(btree_in_t::btree_op_t *op, struct test_state *ts)
{
	uint64_t    key;
	const char *value;
	d_iov_t key_iov;
	d_iov_t val_iov;
	int         rc;

	D_ASSERT(op->type() % OP_TYPE_MAX == (int)OP_TYPE_UPDATE);

	// printf("update %d %d\n", op->key_id(), op->value_id());

	key = ts->keys[op->key_id() % ts->keys.size()];
	d_iov_set(&key_iov, &key, sizeof(key));

	value = ts->values[op->value_id() % ts->values.size()].c_str();
	d_iov_set(&val_iov, (void *)value, strlen(value) + 1);

	rc = dbtree_update(ts->toh, &key_iov, &val_iov);
	D_ASSERT(rc == DER_SUCCESS);

	ts->tree[key] = ts->values[op->value_id() % ts->values.size()];

	return 0;
}

static int
ops_exec_delete(btree_in_t::btree_op_t *op, struct test_state *ts)
{
	uint64_t key;
	d_iov_t  key_iov;
	int      rc;

	D_ASSERT(op->type() % OP_TYPE_MAX == (int)OP_TYPE_DELETE);
	D_ASSERT(op->_is_null_value_id());

	// printf("delete %d\n", op->key_id());

	key = ts->keys[op->key_id() % ts->keys.size()];
	d_iov_set(&key_iov, &key, sizeof(key));

	rc = dbtree_delete(ts->toh, BTR_PROBE_EQ, &key_iov, NULL);

	if (ts->tree.find(key) != ts->tree.end()) {
		D_ASSERT(rc == DER_SUCCESS);
		rc = ts->tree.erase(key);
		D_ASSERT(rc == 1);
	} else {
		D_ASSERT(rc == -DER_NONEXIST);
	}

	return 0;
}

static int
ops_exec_fetch(btree_in_t::btree_op_t *op, struct test_state *ts)
{
	uint64_t    key;
	char        value[MAX_VALUE_LEN + 1];
	d_iov_t     key_iov;
	d_iov_t     val_iov;
	const char *value_exp;
	int         rc;

	D_ASSERT(op->type() % OP_TYPE_MAX == (int)OP_TYPE_FETCH);
	D_ASSERT(op->_is_null_value_id());

	// printf("fetch %d\n", op->key_id());

	key = ts->keys[op->key_id() % ts->keys.size()];
	d_iov_set(&key_iov, &key, sizeof(key));
	d_iov_set(&val_iov, value, ARRAY_SIZE(value));

	rc = dbtree_fetch(ts->toh, BTR_PROBE_EQ, DAOS_INTENT_DEFAULT, &key_iov, NULL, &val_iov);

	if (ts->tree.find(key) != ts->tree.end()) {
		D_ASSERT(rc == DER_SUCCESS);
		value_exp = ts->tree[key].c_str();
		D_ASSERT(strcmp(value, value_exp) == 0);
	} else {
		D_ASSERT(rc == -DER_NONEXIST);
	}

	return 0;
}

static int
ops_exec_one(btree_in_t::btree_op_t *op, struct test_state *ts)
{
	D_ASSERT(op != NULL);

	enum op_type_t type = (enum op_type_t)(op->type() % OP_TYPE_MAX);

	switch (type) {
	case OP_TYPE_UPDATE:
		return ops_exec_update(op, ts);
	case OP_TYPE_DELETE:
		return ops_exec_delete(op, ts);
	case OP_TYPE_FETCH:
		return ops_exec_fetch(op, ts);
	default:
		D_ERROR("Unknown op type: %" PRIu8 "\n", op->type());
		return -1;
	}
}

static int
ops_exec(std::vector<btree_in_t::btree_op_t *> *ops, struct test_state *ts)
{
	int rc;

	D_ASSERT(ops != NULL);

	for (auto *op : *ops) {
		rc = ops_exec_one(op, ts);
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
	struct test_state ts = {};

	if (!ifs) {
		D_ERROR("Cannot open file: %s\n", file_name);
		return -1;
	}

	/* wrap file in Kaitai stream and parse */
	kaitai::kstream ks(&ifs);
	btree_in_t      btree_in(&ks);

	rc = init(btree_in.params(), &ts);
	D_ASSERT(rc == DER_SUCCESS);

	rc = ops_exec(btree_in.op(), &ts);
	D_ASSERT(rc == 0);

	// rc = fini(&ts);
	D_ASSERT(rc == 0);

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
