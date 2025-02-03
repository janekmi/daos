/**
 * (C) Copyright 2016-2025 Intel Corporation.
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */
#define D_LOGFAC	DD_FAC(tests)

#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <setjmp.h>
#include <cmocka.h>

#include <daos/btree.h>
#include <daos/dtx.h>
#include <daos/tests_lib.h>
#include "utest_common.h"

enum ik_btr_opc {
	BTR_OPC_UPDATE,
	BTR_OPC_LOOKUP,
	BTR_OPC_DELETE,
	BTR_OPC_DELETE_RETAIN,
};

struct test_input_value {
	bool				input;
	enum	 ik_btr_opc		opc;
	char				*optval;
};

struct test_input_value tst_fn_val;

/**
 * An example for integer key btree.
 */

#define IK_ORDER_DEF	16

static int ik_order = IK_ORDER_DEF;

struct utest_context		*ik_utx;
struct umem_attr		*ik_uma;
static umem_off_t		 ik_root_off;
static struct btr_root		*ik_root;
static daos_handle_t		 ik_toh;

static bool ik_inplace;


/** integer key record */
struct ik_rec {
	uint64_t	ir_key;
	uint32_t	ir_val_size;
	uint32_t	ir_val_msize;
	umem_off_t	ir_val_off;
};

static char	**test_group_args;
static int	test_group_start;
static int	test_group_stop;

#define IK_TREE_CLASS	100
#define POOL_NAME "/mnt/daos/btree-test"
#define POOL_SIZE ((1024 * 1024 * 1024ULL))

/** customized functions for btree */
static int
ik_hkey_size(void)
{
	struct ik_rec irec;
	return sizeof(irec.ir_key);
}

static void
ik_hkey_gen(struct btr_instance *tins, d_iov_t *key_iov, void *hkey)
{
	uint64_t	*ikey;

	ikey = (uint64_t *)key_iov->iov_buf;
	/* ikey = dummy_hash(ikey); */
	memcpy(hkey, ikey, sizeof(*ikey));
}

static int
ik_key_cmp(struct btr_instance *tins, struct btr_record *rec, d_iov_t *key_iov)
{
	int            key1 = *(uint64_t *)key_iov->iov_buf;
	struct ik_rec *irec = umem_off2ptr(&tins->ti_umm, rec->rec_off);

	if (irec->ir_key < key1)
		return BTR_CMP_LT;
	return irec->ir_key > key1 ? BTR_CMP_GT : BTR_CMP_EQ;
}

static int
ik_rec_alloc(struct btr_instance *tins, d_iov_t *key_iov,
		  d_iov_t *val_iov, struct btr_record *rec, d_iov_t *val_out)
{
	umem_off_t		 irec_off;
	struct ik_rec		*irec;
	char			*vbuf;

	irec_off = umem_zalloc(&tins->ti_umm, sizeof(struct ik_rec));
	D_ASSERT(!UMOFF_IS_NULL(irec_off)); /* lazy bone... */

	irec = umem_off2ptr(&tins->ti_umm, irec_off);

	irec->ir_key      = *(uint64_t *)key_iov->iov_buf;
	irec->ir_val_size = irec->ir_val_msize = val_iov->iov_len;

	irec->ir_val_off = umem_alloc(&tins->ti_umm, val_iov->iov_len);
	D_ASSERT(!UMOFF_IS_NULL(irec->ir_val_off));

	vbuf = umem_off2ptr(&tins->ti_umm, irec->ir_val_off);
	memcpy(vbuf, (char *)val_iov->iov_buf, val_iov->iov_len);

	rec->rec_off = irec_off;
	return 0;
}

static int
ik_rec_free(struct btr_instance *tins, struct btr_record *rec, void *args)
{
	struct umem_instance *umm = &tins->ti_umm;
	struct ik_rec *irec;

	irec = umem_off2ptr(umm, rec->rec_off);

	if (args != NULL) {
		umem_off_t *rec_ret = (umem_off_t *) args;
		 /** Provide the buffer to user */
		*rec_ret	= rec->rec_off;
		return 0;
	}
	utest_free(ik_utx, irec->ir_val_off);
	utest_free(ik_utx, rec->rec_off);

	return 0;
}

static int
ik_rec_fetch(struct btr_instance *tins, struct btr_record *rec,
		 d_iov_t *key_iov, d_iov_t *val_iov)
{
	struct ik_rec	*irec;
	char		*val;
	int		 val_size;
	int		 key_size;

	if (key_iov == NULL && val_iov == NULL)
		return -EINVAL;

	irec = (struct ik_rec *)umem_off2ptr(&tins->ti_umm, rec->rec_off);
	val_size = irec->ir_val_size;
	key_size = sizeof(irec->ir_key);

	val = umem_off2ptr(&tins->ti_umm, irec->ir_val_off);
	if (key_iov != NULL) {
		key_iov->iov_len = key_size;
		if (key_iov->iov_buf == NULL)
			key_iov->iov_buf = &irec->ir_key;
		else if (key_iov->iov_buf_len >= key_size)
			memcpy(key_iov->iov_buf, &irec->ir_key, key_size);
	}

	if (val_iov != NULL) {
		val_iov->iov_len = val_size;
		if (val_iov->iov_buf == NULL)
			val_iov->iov_buf = val;
		else if (val_iov->iov_buf_len >= val_size)
			memcpy(val_iov->iov_buf, val, val_size);

	}
	return 0;
}

static char *
ik_rec_string(struct btr_instance *tins, struct btr_record *rec,
		  bool leaf, char *buf, int buf_len)
{
	struct ik_rec	*irec = NULL;
	char		*val;
	int		 nob;
	uint64_t	 ikey;

	if (!leaf) { /* NB: no record body on intermediate node */
		memcpy(&ikey, &rec->rec_hkey[0], sizeof(ikey));
		snprintf(buf, buf_len, DF_U64, ikey);
		return buf;
	}

	irec = (struct ik_rec *)umem_off2ptr(&tins->ti_umm, rec->rec_off);
	ikey = irec->ir_key;
	nob = snprintf(buf, buf_len, DF_U64, ikey);

	buf[nob++] = ':';
	buf_len -= nob;

	val = umem_off2ptr(&tins->ti_umm, irec->ir_val_off);
	strncpy(buf + nob, val, min(irec->ir_val_size, buf_len));

	return buf;
}

static int
ik_rec_update(struct btr_instance *tins, struct btr_record *rec,
		   d_iov_t *key, d_iov_t *val_iov, d_iov_t *val_out)
{
	struct umem_instance	*umm = &tins->ti_umm;
	struct ik_rec		*irec;
	char			*val;

	irec = umem_off2ptr(umm, rec->rec_off);

	if (irec->ir_val_msize >= val_iov->iov_len) {
		umem_tx_add(umm, irec->ir_val_off, irec->ir_val_msize);

	} else {
		umem_tx_add(umm, rec->rec_off, sizeof(*irec));
		umem_free(umm, irec->ir_val_off);

		irec->ir_val_msize = val_iov->iov_len;
		irec->ir_val_off = umem_alloc(umm, val_iov->iov_len);
		D_ASSERT(!UMOFF_IS_NULL(irec->ir_val_off));
	}
	val = umem_off2ptr(umm, irec->ir_val_off);


	memcpy(val, val_iov->iov_buf, val_iov->iov_len);
	irec->ir_val_size = val_iov->iov_len;
	return 0;
}

static int
ik_rec_stat(struct btr_instance *tins, struct btr_record *rec,
		struct btr_rec_stat *stat)
{
	struct umem_instance	*umm = &tins->ti_umm;
	struct ik_rec		*irec;

	irec = umem_off2ptr(umm, rec->rec_off);

	stat->rs_ksize = sizeof(irec->ir_key);
	stat->rs_vsize = irec->ir_val_size;
	return 0;
}

#define BTR_TRACE_MAX		40

/**
 * btree iterator, it is embedded in btr_context.
 */
struct btr_iterator {
	/** state of the iterator */
	unsigned short			 it_state;
	/** private iterator */
	bool				 it_private;
	/**
	 * Reserved for hash collision:
	 * collisions happened on current hkey.
	 */
	unsigned int			 it_collisions;
};

/**
 * Trace for tree search.
 */
struct btr_trace {
	/** pointer to a tree node */
	umem_off_t			tr_node;
	/** child/record index within this node */
	unsigned int			tr_at;
};

struct btr_trace_info {
	struct btr_trace *ti_trace;
	uint32_t          ti_embedded_info;
};

/**
 * Context for btree operations.
 * NB: object cache will retain this data structure.
 */
struct btr_context {
	/** Tree domain: root pointer, memory pool and memory class etc */
	struct btr_instance		 tc_tins;
	/** embedded iterator */
	struct btr_iterator		 tc_itr;
	/** embedded fake record for the purpose of handling embedded value */
	struct btr_record                tc_record;
	/** This provides space for the hkey for the fake record */
	struct ktr_hkey                  tc_hkey;
	/** cached configured tree order */
	uint16_t			 tc_order;
	/** cached tree depth, avoid loading from slow memory */
	uint16_t			 tc_depth;
	/** credits for drain, see dbtree_drain */
	uint32_t                         tc_creds    : 30;
	/**
	 * credits is turned on, \a tcx::tc_creds should be checked
	 * while draining the tree
	 */
	uint32_t                         tc_creds_on : 1;
	/**
	 * returned value of the probe, it should be reset after upsert
	 * or delete because the probe path could have been changed.
	 */
	int				 tc_probe_rc;
	/** refcount, used by iterator */
	int				 tc_ref;
	/** cached tree class, avoid loading from slow memory */
	int				 tc_class;
	/** cached feature bits, avoid loading from slow memory */
	uint64_t			 tc_feats;
	/** trace for the tree root */
	struct btr_trace_info            tc_trace;
	/** trace buffer */
	struct btr_trace		 tc_traces[BTR_TRACE_MAX];
};

static void
ik_tree_print() {
	struct btr_context *tcx = (struct btr_context *)ik_toh.cookie;
	struct btr_instance *tins = &tcx->tc_tins;
	struct umem_instance *umm = &tins->ti_umm;
	struct btr_root *root = tins->ti_root;
	struct btr_node *node;
	struct btr_record *rec;
	
	if (tcx->tc_feats & BTR_FEAT_EMBEDDED) {
		rec = &tcx->tc_record;
		printf("%lu\n", rec->rec_off);
		return;
	}

	if (root->tr_node == UMOFF_NULL) {
		printf("(empty)\n\n");
		return;
	}

	node = umem_off2ptr(umm, root->tr_node);

	while (node != NULL) {
		for (int i = 0; i < node->tn_keyn; ++i) {
			rec = &node->tn_recs[i];
			printf("%lu\n", rec->rec_off);
		}

		if (node->tn_child == UMOFF_NULL) {
			break;
		}

		node = umem_off2ptr(umm, node->tn_child);
	}
	printf("\n");
}

static btr_ops_t ik_ops = {
    .to_hkey_size  = ik_hkey_size,
    .to_hkey_gen   = ik_hkey_gen,
    .to_key_cmp    = ik_key_cmp,
    .to_rec_alloc  = ik_rec_alloc,
    .to_rec_free   = ik_rec_free,
    .to_rec_fetch  = ik_rec_fetch,
    .to_rec_update = ik_rec_update,
    .to_rec_string = ik_rec_string,
    .to_rec_stat   = ik_rec_stat,
};

#define IK_SEP		','
#define IK_SEP_VAL	':'

static void
ik_btr_open_create(void **state)
{
	bool		inplace = false;
	uint64_t	feats = 0;
	int		rc;
	bool	create;
	char	*arg;
	char	outbuf[64];

	create = tst_fn_val.input;
	arg = tst_fn_val.optval;

	if (daos_handle_is_valid(ik_toh)) {
		fail_msg("Tree has been opened\n");
	}

	if (create && arg != NULL) {
		if (arg[0] == '+') {
			feats = BTR_FEAT_UINT_KEY;
			arg += 1;
		} else if (arg[0] == '%') {
			feats = BTR_FEAT_EMBED_FIRST;
			arg += 1;
		}
		if (arg[0] == 'i') { /* inplace create/open */
			inplace = true;
			if (arg[1] != IK_SEP) {
				sprintf(outbuf, "wrong parameter format %s\n",
						arg);
				fail_msg("%s", outbuf);
			}
			arg += 2;
		}

		if (arg[0] != 'o' || arg[1] != IK_SEP_VAL) {
			sprintf(outbuf, "incorrect format for tree order: %s\n",
					arg);
			fail_msg("%s", outbuf);
		}

		ik_order = atoi(&arg[2]);
		if (ik_order < BTR_ORDER_MIN || ik_order > BTR_ORDER_MAX) {
			sprintf(outbuf, "Invalid tree order %d\n", ik_order);
			fail_msg("%s", outbuf);
		}
	} else if (!create) {
		inplace = (ik_root->tr_class != 0);
		if (UMOFF_IS_NULL(ik_root_off) && !inplace)
			fail_msg("Please create tree first\n");
	}

	if (create) {
		D_PRINT("Create btree with order %d%s feats "DF_X64"\n",
			ik_order, inplace ? " inplace" : "", feats);
		if (inplace) {
			rc = dbtree_create_inplace(IK_TREE_CLASS, feats,
						   ik_order, ik_uma, ik_root,
						   &ik_toh);
		} else {
			rc = dbtree_create(IK_TREE_CLASS, feats, ik_order,
					   ik_uma, &ik_root_off, &ik_toh);
		}
	} else {
		D_PRINT("Open btree%s\n", inplace ? " inplace" : "");
		if (inplace) {
			rc = dbtree_open_inplace(ik_root,
						 ik_uma,
						 &ik_toh);
		} else {
			rc = dbtree_open(ik_root_off, ik_uma, &ik_toh);
		}
	}
	if (rc != 0) {
		sprintf(outbuf, "Tree %s failed: %d\n",
				create ? "create" : "open", rc);
		fail_msg("%s", outbuf);
	}

	ik_tree_print();
}

static void
ik_btr_close_destroy(void **state)
{
	int		rc;
	bool	destroy;
	char	outbuf[64];

	destroy = tst_fn_val.input;

	if (daos_handle_is_inval(ik_toh)) {
		fail_msg("Invalid tree open handle\n");
	}

	if (destroy) {
		D_PRINT("Destroy btree\n");
		rc = dbtree_destroy(ik_toh, NULL);
	} else {
		D_PRINT("Close btree\n");
		rc = dbtree_close(ik_toh);
	}

	ik_toh = DAOS_HDL_INVAL;
	if (rc != 0) {
		sprintf(outbuf, "Tree %s failed: %d\n",
			destroy ? "destroy" : "close", rc);
		fail_msg("%s", outbuf);
	}
}

static int
btr_rec_verify_delete(umem_off_t *rec, d_iov_t *key)
{
	struct umem_instance	*umm;
	struct ik_rec		*irec;

	umm = utest_utx2umm(ik_utx);

	irec	  = umem_off2ptr(umm, *rec);

	if ((sizeof(irec->ir_key) != key->iov_len) ||
		(irec->ir_key != *((uint64_t *)key->iov_buf))) {
		D_ERROR("Preserved record mismatch while delete\n");
		return -1;
	}

	utest_free(ik_utx, irec->ir_val_off);
	utest_free(ik_utx, *rec);

	return 0;
}

static char *
btr_opc2str(void)
{
	enum ik_btr_opc opc;

	opc = tst_fn_val.opc;
	switch (opc) {
	default:
		return "unknown";
	case BTR_OPC_UPDATE:
		return "update";
	case BTR_OPC_LOOKUP:
		return "lookup";
	case BTR_OPC_DELETE:
		return "delete";
	case BTR_OPC_DELETE_RETAIN:
		return "delete and retain";
	}
}

static void
ik_btr_kv_operate(void **state)
{
	int					count = 0;
	umem_off_t				rec_off;
	int					rc;
	enum	ik_btr_opc	opc;
	char				*str;
	bool				verbose;
	char				outbuf[64];

	opc = tst_fn_val.opc;
	str = tst_fn_val.optval;
	verbose = tst_fn_val.input;

	if (daos_handle_is_inval(ik_toh)) {
		fail_msg("Can't find opened tree\n");
	}

	while (str != NULL && !isspace(*str) && *str != '\0') {
		char	   *val = NULL;
		d_iov_t	key_iov;
		d_iov_t	val_iov;
		uint64_t	key;

		key = strtoul(str, NULL, 0);

		if (opc == BTR_OPC_UPDATE) {
			val = strchr(str, IK_SEP_VAL);
			if (val == NULL) {
				sprintf(outbuf,
				"Invalid parameters %s(errno %d)\n",
					str, errno);
				fail_msg("%s", outbuf);
			}
			str = ++val;
		}

		str = strchr(str, IK_SEP);
		if (str != NULL) {
			*str = '\0';
			str++;
		}

		d_iov_set(&key_iov, &key, sizeof(key));
		switch (opc) {
		default:
			fail_msg("Invalid opcode\n");
			break;
		case BTR_OPC_UPDATE:
			d_iov_set(&val_iov, val, strlen(val) + 1);
			rc = dbtree_update(ik_toh, &key_iov, &val_iov);
			if (rc != 0) {
				sprintf(outbuf,
				"Failed to update "DF_U64":%s\n", key, val);
				fail_msg("%s", outbuf);
			}
			ik_tree_print();
			break;

		case BTR_OPC_DELETE:
			rc = dbtree_delete(ik_toh, BTR_PROBE_EQ,
					   &key_iov, NULL);
			if (rc != 0) {
				sprintf(outbuf,
					"Failed to delete "DF_U64"\n", key);
				fail_msg("%s", outbuf);
			}
			if (verbose)
				D_PRINT("Deleted key "DF_U64"\n", key);

			if (dbtree_is_empty(ik_toh) && verbose)
				D_PRINT("Tree is empty now\n");
			ik_tree_print();
			break;

		case BTR_OPC_DELETE_RETAIN:
			rc = dbtree_delete(ik_toh, BTR_PROBE_EQ,
					   &key_iov, &rec_off);
			if (rc != 0) {
				sprintf(outbuf,
					"Failed to delete "DF_U64"\n", key);
				fail_msg("%s", outbuf);
			}

			/** Verify and delete rec_off here */
			rc = btr_rec_verify_delete(&rec_off, &key_iov);
			if (rc != 0) {
				fail_msg("Failed to verify and delete rec\n");
			}

			if (verbose)
				D_PRINT("Deleted key "DF_U64"\n", key);
			if (dbtree_is_empty(ik_toh) && verbose)
				D_PRINT("Tree is empty now\n");
			break;

		case BTR_OPC_LOOKUP:
			D_DEBUG(DB_TEST, "Looking for "DF_U64"\n", key);

			d_iov_set(&val_iov, NULL, 0); /* get address */
			rc = dbtree_lookup(ik_toh, &key_iov, &val_iov);
			if (rc != 0) {
				sprintf(outbuf,
					"Failed to lookup "DF_U64"\n", key);
				fail_msg("%s", outbuf);
			}

			if (verbose) {
				D_PRINT("Found key "DF_U64", value %s\n",
					key, (char *)val_iov.iov_buf);
			}
			break;
		}
		count++;
	}
	if (verbose)
		D_PRINT("%s %d record(s)\n", btr_opc2str(), count);
}

static void
ik_btr_query(void **state)
{
	struct btr_attr		attr;
	struct btr_stat		stat;
	int			rc;
	char		outbuf[64];

	rc = dbtree_query(ik_toh, &attr, &stat);
	if (rc != 0) {
		sprintf(outbuf, "Failed to query btree: %d\n", rc);
		fail_msg("%s", outbuf);
	}

	D_PRINT("tree	[order=%d, depth=%d]\n", attr.ba_order, attr.ba_depth);
	D_PRINT("node	[total="DF_U64"]\n"
		"record [total="DF_U64"]\n"
		"key	[total="DF_U64", max="DF_U64"]\n"
		"val	[total="DF_U64", max="DF_U64"]\n",
		stat.bs_node_nr, stat.bs_rec_nr,
		stat.bs_key_sum, stat.bs_key_max,
		stat.bs_val_sum, stat.bs_val_max);

}

// static void
// ik_btr_iterate(void **state)
// {
// 	daos_handle_t	ih;
// 	int		i;
// 	int		d;
// 	int		del;
// 	int		rc;
// 	int		opc;
// 	char		*err;
// 	char		*arg;

// 	arg = tst_fn_val.optval;

// 	if (daos_handle_is_inval(ik_toh)) {
// 		fail_msg("Can't find opened tree\n");
// 	}

// 	rc = dbtree_iter_prepare(ik_toh, BTR_ITER_EMBEDDED, &ih);
// 	if (rc != 0) {
// 		err = "Failed to initialize tree\n";
// 		goto failed;
// 	}

// 	if (arg[0] == 'b')
// 		opc = BTR_PROBE_LAST;
// 	else
// 		opc = BTR_PROBE_FIRST;

// 	if (arg[1] == ':')
// 		del = atoi(&arg[2]);
// 	else
// 		del = 0;

// 	for (i = d = 0;; i++) {
// 		d_iov_t	key_iov;
// 		d_iov_t	val_iov;
// 		uint64_t	key;

// 		if (i == 0 || (del != 0 && d <= del)) {
// 			rc = dbtree_iter_probe(ih, opc, DAOS_INTENT_DEFAULT,
// 						   NULL, NULL);
// 			if (rc == -DER_NONEXIST)
// 				break;

// 			if (rc != 0) {
// 				err = "probe failure\n";
// 				goto failed;
// 			}

// 			if (del != 0) {
// 				if (d == del)
// 					del = d = 0; /* done */
// 				else
// 					d++;
// 			}
// 		}

// 		d_iov_set(&key_iov, NULL, 0);
// 		d_iov_set(&val_iov, NULL, 0);
// 		rc = dbtree_iter_fetch(ih, &key_iov, &val_iov, NULL);
// 		if (rc != 0) {
// 			err = "fetch failure\n";
// 			goto failed;
// 		}

// 		D_ASSERT(key_iov.iov_len == sizeof(key));
// 		memcpy(&key, key_iov.iov_buf, sizeof(key));

// 		if (d != 0) { /* delete */
// 			D_PRINT("Delete "DF_U64": %s\n",
// 				key, (char *)val_iov.iov_buf);
// 			rc = dbtree_iter_delete(ih, NULL);
// 			if (rc != 0) {
// 				err = "delete failure\n";
// 				goto failed;
// 			}

// 		} else { /* iterate */
// 			D_PRINT(DF_U64": %s\n", key, (char *)val_iov.iov_buf);

// 			if (opc == BTR_PROBE_LAST)
// 				rc = dbtree_iter_prev(ih);
// 			else
// 				rc = dbtree_iter_next(ih);

// 			if (rc == -DER_NONEXIST)
// 				break;

// 			if (rc != 0) {
// 				err = "move failure\n";
// 				goto failed;
// 			}
// 		}
// 	}

// 	D_PRINT("%s iterator: total %d, deleted %d\n",
// 		opc == BTR_PROBE_FIRST ? "forward" : "backward", i, d);
// 	dbtree_iter_finish(ih);
// 	goto pass;

// failed:
// 	dbtree_iter_finish(ih);
// 	fail_msg("%s", err);

// pass:
// 	print_message("Test Passed\n");
// }

/* fill in @arr with natural number from 1 to key_nr, randomize their order */
void
ik_btr_gen_keys(unsigned int *arr, unsigned int key_nr)
{
	int		nr;
	int		i;

	for (i = 0; i < key_nr; i++)
		arr[i] = i + 1;

	for (nr = key_nr; nr > 0; nr--) {
		unsigned int	tmp;
		int		j;

		j = rand() % nr;
		if (j != nr - 1) {
			tmp = arr[j];
			arr[j] = arr[nr - 1];
			arr[nr - 1] = tmp;
		}
	}
}

#define DEL_BATCH	10000
/**
 * batch btree operations:
 * 1) insert @key_nr number of integer keys
 * 2) lookup all the rest keys
 * 3) delete nr=DEL_BATCH keys
 * 4) repeat 2) and 3) util all keys are deleted
 */
static void
ik_btr_batch_oper(void **state)
{
	unsigned int	*arr;
	char		 buf[64];
	int		 i;
	unsigned int	key_nr;
	bool		 verbose;

	key_nr = atoi(tst_fn_val.optval);
	verbose = key_nr < 20;

	if (key_nr == 0 || key_nr > (1U << 28)) {
		D_PRINT("Invalid key number: %d\n", key_nr);
		fail();
	}

	D_ALLOC_ARRAY(arr, key_nr);
	if (arr == NULL) {
		fail_msg("Array allocation failed");
		return;
	}

	D_PRINT("Batch add %d records.\n", key_nr);
	ik_btr_gen_keys(arr, key_nr);
	for (i = 0; i < key_nr; i++) {
		sprintf(buf, "%d:%d", arr[i], arr[i]);
		tst_fn_val.opc = BTR_OPC_UPDATE;
		tst_fn_val.optval = buf;
		tst_fn_val.input = verbose;
		ik_btr_kv_operate(NULL);
	}

	ik_btr_query(NULL);
	/* lookup all rest records, delete 10000 of them, and repeat until
	 * deleting all records.
	 */
	ik_btr_gen_keys(arr, key_nr);
	for (i = 0; i < key_nr;) {
		int	j;

		D_PRINT("Batch lookup %d records.\n", key_nr - i);
		for (j = i; j < key_nr; j++) {
			sprintf(buf, "%d", arr[j]);
			tst_fn_val.opc = BTR_OPC_LOOKUP;
			tst_fn_val.optval = buf;
			tst_fn_val.input = verbose;
			ik_btr_kv_operate(NULL);
		}

		D_PRINT("Batch delete %d records.\n",
			min(key_nr - i, DEL_BATCH));

		for (j = 0; i < key_nr && j < DEL_BATCH; i++, j++) {
			sprintf(buf, "%d", arr[i]);
			tst_fn_val.opc = BTR_OPC_DELETE;
			tst_fn_val.optval = buf;
			tst_fn_val.input = verbose;
			ik_btr_kv_operate(NULL);
		}
	}
	ik_btr_query(NULL);
	D_FREE(arr);
}

static void
ik_btr_perf(void **state)
{
	unsigned int	*arr;
	char		 buf[64];
	int		 i;
	double		 then;
	double		 now;
	unsigned int	key_nr;

	key_nr = atoi(tst_fn_val.optval);

	if (key_nr == 0 || key_nr > (1U << 28)) {
		D_PRINT("Invalid key number: %d\n", key_nr);
		fail();
	}

	D_PRINT("Btree performance test, order=%u, keys=%u\n",
		ik_order, key_nr);

	D_ALLOC_ARRAY(arr, key_nr);
	if (arr == NULL)
		fail_msg("Array allocation failed\n");

	/* step-1: Insert performance */
	ik_btr_gen_keys(arr, key_nr);
	then = dts_time_now();

	for (i = 0; i < key_nr; i++) {
		sprintf(buf, "%d:%d", arr[i], arr[i]);
		tst_fn_val.opc = BTR_OPC_UPDATE;
		tst_fn_val.optval = buf;
		tst_fn_val.input = false;
		ik_btr_kv_operate(NULL);
	}
	now = dts_time_now();
	D_PRINT("insert = %10.2f/sec\n", key_nr / (now - then));

	/* step-2: lookup performance */
	ik_btr_gen_keys(arr, key_nr);
	then = dts_time_now();

	for (i = 0; i < key_nr; i++) {
		sprintf(buf, "%d", arr[i]);
		tst_fn_val.opc = BTR_OPC_LOOKUP;
		tst_fn_val.optval = buf;
		tst_fn_val.input = false;
		ik_btr_kv_operate(NULL);
	}
	now = dts_time_now();
	D_PRINT("lookup = %10.2f/sec\n", key_nr / (now - then));

	/* step-3: delete performance */
	ik_btr_gen_keys(arr, key_nr);
	then = dts_time_now();

	for (i = 0; i < key_nr; i++) {
		sprintf(buf, "%d", arr[i]);
		tst_fn_val.opc = BTR_OPC_DELETE;
		tst_fn_val.optval = buf;
		tst_fn_val.input = false;
		ik_btr_kv_operate(NULL);
	}
	now = dts_time_now();
	D_PRINT("delete = %10.2f/sec\n", key_nr / (now - then));
	D_FREE(arr);
}


static void
ik_btr_drain(void **state)
{
	static const int drain_keys  = 10;
	static const int drain_creds = 23;

	unsigned int	*arr;
	unsigned int	 drained = 0;
	char		 buf[64];
	int		 i;

	D_ALLOC_ARRAY(arr, drain_keys);
	if (arr == NULL) {
		fail_msg("Array allocation failed");
		return;
	}

	D_PRINT("Batch add %d records.\n", drain_keys);
	ik_btr_gen_keys(arr, drain_keys);
	for (i = 0; i < drain_keys; i++) {
		sprintf(buf, "%d:%d", arr[i], arr[i]);
		tst_fn_val.opc	  = BTR_OPC_UPDATE;
		tst_fn_val.optval = buf;
		tst_fn_val.input  = false;

		ik_btr_kv_operate(NULL);
	}

	ik_btr_query(NULL);
	while (1) {
		int	creds = drain_creds;
		bool	empty = false;
		int	rc;

		rc = dbtree_drain(ik_toh, &creds, NULL, &empty);
		if (rc) {
			fail_msg("Failed to drain btree: %s\n", d_errstr(rc));
			fail();
		}
		drained += drain_creds - creds;
		D_PRINT("Drained %d of %d KVs, empty=%d\n",
			drained, drain_keys, empty);
		if (empty)
			break;
	}

	D_FREE(arr);
}

static struct option btr_ops[] = {
	{ "create",	required_argument,	NULL,	'C'	},
	{ "destroy",	no_argument,		NULL,	'D'	},
	{ "drain",	no_argument,		NULL,	'e'	},
	{ "open",	no_argument,		NULL,	'o'	},
	{ "close",	no_argument,		NULL,	'c'	},
	{ "update",	required_argument,	NULL,	'u'	},
	{ "find",	required_argument,	NULL,	'f'	},
	{ "dyn_tree",	no_argument,		NULL,	't'	},
	{ "delete",	required_argument,	NULL,	'd'	},
	{ "del_retain", required_argument,	NULL,	'r'	},
	{ "query",	no_argument,		NULL,	'q'	},
	{ "iterate",	required_argument,	NULL,	'i'	},
	{ "batch",	required_argument,	NULL,	'b'	},
	{ "perf",	required_argument,	NULL,	'p'	},
	{ NULL,		0,			NULL,	0	},
};

static int
use_pmem() {

	int rc;

	D_PRINT("Using pmem\n");
	rc = utest_pmem_create(POOL_NAME, POOL_SIZE,
			       sizeof(*ik_root), NULL,
			       &ik_utx);
	D_ASSERT(rc == 0);
	return rc;
}

static void op_iter(int entries_num);

static void
ts_group(void **state) {

	int	opt = 0;
	void	**st = NULL;

	while ((opt = getopt_long(test_group_stop-test_group_start+1,
				  test_group_args+test_group_start,
				  "tmC:Deocqu:d:r:f:i:b:p:",
				  btr_ops,
				  NULL)) != -1) {
		tst_fn_val.optval = optarg;
		tst_fn_val.input = true;

		switch (opt) {
		case 'C':
			ik_btr_open_create(st);
			break;
		case 'D':
			tst_fn_val.input = true;
			ik_btr_close_destroy(st);
			break;
		case 'o':
			tst_fn_val.input = false;
			tst_fn_val.optval = NULL;
			ik_btr_open_create(st);
			break;
		case 'c':
			tst_fn_val.input = false;
			ik_btr_close_destroy(st);
			break;
		case 'e':
			ik_btr_drain(st);
			break;
		case 'q':
			ik_btr_query(st);
			break;
		case 'u':
			tst_fn_val.opc = BTR_OPC_UPDATE;
			ik_btr_kv_operate(st);
			break;
		case 'f':
			tst_fn_val.opc = BTR_OPC_LOOKUP;
			ik_btr_kv_operate(st);
			break;
		case 'd':
			tst_fn_val.opc = BTR_OPC_DELETE;
			ik_btr_kv_operate(st);
			break;
		case 'r':
			tst_fn_val.opc = BTR_OPC_DELETE_RETAIN;
			ik_btr_kv_operate(st);
			break;
		case 'i':
			// ik_btr_iterate(st);
			op_iter(2);
			break;
		case 'b':
			ik_btr_batch_oper(st);
			break;
		case 'p':
			ik_btr_perf(st);
			break;
		case 'm':
		case 't':
			/* Ought to be handled at the earlier stage. */
			fail_msg("Unexpected command %c\n", opt);
			break;
		default:
			fail_msg("Unsupported command %c\n", opt);
		}
	}
}

static int
run_cmd_line_test(char *test_name, char **args, int start_idx, int stop_idx)
{

	const struct CMUnitTest btree_test[] = {
		{test_name, ts_group, NULL, NULL},
	};

	test_group_args = args;
	test_group_start = start_idx;
	test_group_stop = stop_idx;

	return cmocka_run_group_tests_name(test_name,
					   btree_test,
					   NULL,
					   NULL);
}

enum {
	USE_PMEM = 1 << 0,
	USE_DYNAMIC_ROOT = 1 << 1
};

#define FIRST_OPTS_SHIFT     (8 - 2)
#define FIRST_SEED_MASK      0b00111111

#define OP_CREATE_OPTS_SHIFT (8 - 3)
#define OP_CREATE_ORDER_MASK 0b00011111

enum Op {
    OP_CREATE = 1,
    OP_CLOSE = 2,
    OP_DESTROY = 3,
    OP_OPEN = 4,
    OP_UPDATE = 5,
    OP_ITER = 6,
    OP_QUERY = 7,
    OP_LOOKUP = 8,
    OP_DELETE = 9,
    OP_DRAIN = 10,
};

enum OpCreateOpt {
	INPLACE = 1 << 0,
	UINT_KEY = 1 << 1,
	EMBED_FIRST = 1 << 2
};

static void
op_create(char opts, char order) {
	uint64_t feats = 0;
	int rc;

	if (opts & UINT_KEY) {
		feats += BTR_FEAT_UINT_KEY;
	}
	if (opts & EMBED_FIRST) {
		feats += BTR_FEAT_EMBED_FIRST;
	}

	if (order != 0){
		ik_order = order;
	}

	ik_inplace = ((opts & INPLACE) != 0);

	if (ik_inplace) {
		rc = dbtree_create_inplace(IK_TREE_CLASS, feats,
						ik_order, ik_uma, ik_root,
						&ik_toh);
	} else {
		rc = dbtree_create(IK_TREE_CLASS, feats, ik_order,
					ik_uma, &ik_root_off, &ik_toh);
	}

	assert_rc_equal(rc, 0);
}

static void
op_open() {
	int rc;

	if (ik_inplace) {
		rc = dbtree_open_inplace(ik_root, ik_uma, &ik_toh);
	} else {
		rc = dbtree_open(ik_root_off, ik_uma, &ik_toh);
	}

	assert_rc_equal(rc, 0);
}

/*
 * Records is just a different name for copies of entries kept by test for
 * validation.
 */
struct record {
	uint64_t key;
	size_t value_pos;
	char *value;
	size_t value_size;
};

#define RECORDS_INCREASE 10

size_t records_total = 0;
size_t records_used = 0;
struct record *records = NULL;

#define VALUES_INCREASE 10
#define VALUE_MAX 256

size_t values_size = 0;
size_t values_pos = 0;
char *values = NULL;

static size_t
record_get_empty() {
	if (records_used == records_total) {
		records = (struct record *)realloc(records,
			sizeof(struct record) * (records_total + RECORDS_INCREASE));
		records_total += RECORDS_INCREASE;
	}
	records[records_used].value = NULL;
	return records_used++;
}

static void
record_check(uint64_t key, char *value, size_t value_size) {
	for (int i = 0; i < records_used; ++i) {
		if (records[i].key == key) {
			assert_int_equal(records[i].value_size, value_size);
			assert_int_equal
				(memcmp(records[i].value, value, sizeof(char) * value_size), 0);
			return;
		}
	}
	fail_msg("Can not find a record of key: %lu", key);
}

#define RECORD_IDX_UNKNOWN (-1)

static void
record_delete(uint64_t key, int idx) {
	if (idx == RECORD_IDX_UNKNOWN) {
		for (int i = 0; i < records_used; ++i) {
			if (records[i].key == key) {
				idx = i;
				break;
			}
		}
		assert_int_not_equal(idx, RECORD_IDX_UNKNOWN);
	}
	printf("Deleted [%d]: %lu\n", idx, records[idx].key);
	if (idx != records_used - 1) {
		memcpy(&records[idx], &records[idx + 1],
			sizeof(struct record) * (records_used - idx - 1));
	}
	records_used -= 1;
}

static void
value_rand(struct record *rec)
{
	rec->value_size = rand() % VALUE_MAX;
	if (values_pos + rec->value_size > values_size) {
		unsigned increase = (rec->value_size / VALUES_INCREASE + 1) * VALUES_INCREASE;
		values = (char *)realloc(values,
			sizeof(char) * (values_size + increase));
		values_size += increase;
		// fix pointers in the records
		for (int i = 0; i < records_used; ++i) {
			if (records[i].value == NULL) {
				continue;
			}
			records[i].value = &values[records[i].value_pos];
		}
	}
	rec->value_pos = values_pos;
	rec->value = &values[values_pos];
	values_pos += rec->value_size;
	for (int i = 0; i < rec->value_size; ++i) {
		rec->value[i] = rand() % 256;
		rec->value[i] = 'a'; // XXX
	}
	// rec->value_size = 2; // XXX
	// rec->value[1] = '\0';
}

static void
op_update(char entries_num) {
	const int use_existing_chance = 30;
	int idx;
	d_iov_t	key_iov;
	d_iov_t	val_iov;
	int rc;

	for (int i = 0; i < entries_num; ++i) {
		// if possible give a chance to use an existing key
		bool use_existing = ((rand() % 100) < use_existing_chance);
		if (records_used == 0) {
			use_existing = false;
		}
		if (use_existing) {
			idx = rand() % records_used;
		} else {
			idx = record_get_empty();
			records[idx].key = rand();
		}
		// rand a new value
		value_rand(&records[idx]);
		// update the tree
		d_iov_set(&key_iov, &records[idx].key, sizeof(records[idx].key));
		d_iov_set(&val_iov, records[idx].value,
			sizeof(char) * records[idx].value_size);
		rc = dbtree_update(ik_toh, &key_iov, &val_iov);
		assert_rc_equal(rc, 0);
		printf("%s [%d]: %lu\n", use_existing ? "Updated" : "Added", idx,
			records[idx].key);
	}
}

#define ITER_STEPS_MAX 10

static int
op_iter_probe_rand(daos_handle_t ih) {
	// if (records_used == 0) {
	// 	return -DER_NONEXIST;
	// }
	dbtree_probe_opc_t opc = BTR_PROBE_FIRST;
	int idx;
	d_iov_t	key_iov;
	d_iov_t	*key_iovp = NULL;
	int rc;
	// switch(rand() % 3) {
	// 	case 0:
	// 		opc = BTR_PROBE_FIRST;
	// 		break;
	// 	case 1:
	// 		opc = BTR_PROBE_LAST;
	// 		break;
	// 	case 2:
	// 		opc = BTR_PROBE_EQ;
	// 		break;
	// 	/* XXX: Probes? */
	// }
	if (opc == BTR_PROBE_EQ) {
		idx = rand() % records_used;
		d_iov_set(&key_iov, &records[idx].key, sizeof(records[idx].key));
		key_iovp = &key_iov;
	}
	/* XXX: Other intents? */
	/* XXX: Anchors? */
	rc = dbtree_iter_probe(ih, opc, DAOS_INTENT_DEFAULT, key_iovp, NULL);
	assert_rc_equal(rc, 0);
	return rc;
}

static void
op_iter(int entries_num) {
	daos_handle_t ih;
	d_iov_t	key_iov;
	d_iov_t	val_iov;
	int rc;
	assert_rc_equal(dbtree_iter_prepare(ik_toh, BTR_ITER_EMBEDDED, &ih), 0);
	if (op_iter_probe_rand(ih) != 0) {
		return;
	}
	int steps_num = entries_num; // + rand() % ITER_STEPS_MAX;
	// bool prev;
	for (int i = 0; i < steps_num; ++i) {
		// int steps_remaining = steps_num - i;
		// if (rand() % 2 == 0) {
			/* fetch and check the value is as expected */
			d_iov_set(&key_iov, NULL, 0);
			d_iov_set(&val_iov, NULL, 0);
			rc = dbtree_iter_fetch(ih, &key_iov, &val_iov, NULL);
			assert_rc_equal(rc, 0);
			// record_check(*((uint64_t *)key_iov.iov_buf),
			// 	(char *)val_iov.iov_buf, val_iov.iov_len);
		// }
		// if (rand() % steps_num > steps_remaining) {
		// 	/* fetch the key to remove it from the records */
		// 	d_iov_set(&key_iov, NULL, 0);
		// 	d_iov_set(&val_iov, NULL, 0);
		// 	rc = dbtree_iter_fetch(ih, &key_iov, &val_iov, NULL);
		// 	assert_rc_equal(rc, 0);
		// 	record_delete(*(uint64_t *)key_iov.iov_buf, RECORD_IDX_UNKNOWN);
		// 	/* delete the entry */
		// 	assert_rc_equal(dbtree_iter_delete(ih, NULL), 0);
		// 	/* re-probe since after the delete the iterator is not ready */
		// 	if (op_iter_probe_rand(ih) != 0) {
		// 		return;
		// 	}
		// }
		// prev = rand() % 2;
		// if (prev) {
		// 	rc = dbtree_iter_prev(ih);
		// } else {
			rc = dbtree_iter_next(ih);
		// }
		assert_true(rc == 0 || rc == -DER_NONEXIST);
		if (rc == -DER_NONEXIST) {
			/* re-probe since after hitting a non-existing entry the iterator is not ready */
			if (op_iter_probe_rand(ih) != 0) {
				return;
			}
		}
	}
	dbtree_iter_finish(ih);
}

static void
op_query() {
	struct btr_attr attr;
	struct btr_stat stat;
	assert_rc_equal(dbtree_query(ik_toh, &attr, &stat), 0);
}

static void
op_lookup(int entries_num) {
	d_iov_t	key_iov;
	d_iov_t	val_iov;
	for (int i = 0; i < entries_num; ++i) {
		int idx = rand() % records_used;
		d_iov_set(&key_iov, &records[idx].key, sizeof(records[idx].key));
		d_iov_set(&val_iov, NULL, 0); /* get address */
		printf("Lookup [%d]: %lu\n", idx, *(uint64_t *)key_iov.iov_buf);
		/* XXX get a value is missing? */
		assert_rc_equal(dbtree_lookup(ik_toh, &key_iov, &val_iov), 0);
		record_check(records[idx].key, (char *)val_iov.iov_buf, val_iov.iov_len);
	}
}

static void
op_delete(int entries_num) {
	d_iov_t	key_iov;
	for (int i = 0; i < entries_num; ++i) {
		if (records_used == 0) {
			return;
		}
		int idx = rand() % records_used;
		/* delete the entry */
		d_iov_set(&key_iov, &records[idx].key, sizeof(records[idx].key));
		assert_rc_equal(dbtree_delete(ik_toh, BTR_PROBE_EQ, &key_iov, NULL), 0);
		/* XXX other probes? */
		/* delete from the records */
		record_delete(0, idx);
	}
}

static void
run_cmd_from_file(char *cmds, size_t read)
{
	int rc;
	// initialize pseudo-random generator
	char seed = (cmds[0] & FIRST_SEED_MASK);
	srand(seed);
	// parse options
	char opt = cmds[0] >> FIRST_OPTS_SHIFT;
	if (opt & USE_PMEM) {
		(void) use_pmem();
	} else {
		D_PRINT("Using vmem\n");
		assert_int_equal(utest_vmem_create(sizeof(*ik_root), &ik_utx), 0);
	}
	int dynamic_flag = 0;
	if (opt & USE_DYNAMIC_ROOT) {
		dynamic_flag = BTR_FEAT_DYNAMIC_ROOT;
	}
	// register class and populate globals
	rc = dbtree_class_register(
	    IK_TREE_CLASS, dynamic_flag | BTR_FEAT_EMBED_FIRST | BTR_FEAT_UINT_KEY, &ik_ops);
	assert_int_equal(rc, 0);
	ik_root = utest_utx2root(ik_utx);
	ik_uma = utest_utx2uma(ik_utx);
	// execute operations
	char arg1;
	char arg2;
	for (unsigned pos = 1; pos < read; ++pos) {
		enum Op op = cmds[pos];
		switch (op) {
			case OP_CREATE:
				printf("OP_CREATE\n");
				arg1 = cmds[pos + 1] >> OP_CREATE_OPTS_SHIFT; // opts
				arg2 = cmds[pos + 1] & OP_CREATE_ORDER_MASK;  // order
				pos += 1;
				op_create(arg1, arg2);
				break;
			case OP_CLOSE:
				printf("OP_CLOSE\n");
				assert_rc_equal(dbtree_close(ik_toh), 0);
				ik_toh = DAOS_HDL_INVAL;
				break;
			case OP_DESTROY:
				printf("OP_DESTROY\n");
				assert_rc_equal(dbtree_destroy(ik_toh, NULL), 0);
				ik_toh = DAOS_HDL_INVAL;
				break;
			case OP_OPEN:
				printf("OP_OPEN\n");
				op_open();
				break;
			case OP_UPDATE:
				printf("OP_UPDATE\n");
				arg1 = cmds[pos + 1]; // # of entries
				pos += 1;
				op_update(arg1);
				break;
			case OP_ITER:
				printf("OP_ITER\n");
				arg1 = cmds[pos + 1]; // # of entries
				pos += 1;
				op_iter(arg1);
				break;
			case OP_QUERY:
				printf("OP_QUERY\n");
				op_query();
				break;
			case OP_LOOKUP:
				printf("OP_LOOKUP\n");
				arg1 = cmds[pos + 1]; // # of entries
				pos += 1;
				op_lookup(arg1);
				break;
			case OP_DELETE:
				printf("OP_DELETE\n");
				arg1 = cmds[pos + 1]; // # of entries
				pos += 1;
				op_delete(arg1);
				break;
			case OP_DRAIN:
				printf("OP_DRAIN\n");
				arg1 = cmds[pos + 1]; // # of credits
				pos += 1;
				int creds = arg1;
				bool destroyed;
				assert_rc_equal(dbtree_drain(ik_toh, &creds, NULL, &destroyed), 0);
				break;
			default:
				fail_msg("Unsupported op: %u", op);
		}
	}
	(void) arg1;
	(void) arg2;
	// clean up
	daos_debug_fini();
	assert_int_equal(utest_utx_destroy(ik_utx), 0);
}

int
main(int argc, char **argv)
{
	// struct timeval	tv;
	int		rc = 0;
	int		opt;
	int		dynamic_flag = 0;
	int		start_idx;
	char		*test_name;
	int		stop_idx;

	d_register_alt_assert(mock_assert);

	// gettimeofday(&tv, NULL);
	// srand(tv.tv_usec);

	ik_toh = DAOS_HDL_INVAL;
	ik_root_off = UMOFF_NULL;

	rc = daos_debug_init(DAOS_LOG_DEFAULT);
	if (rc != 0)
		return rc;

	if (argc == 1) {
		print_message("Invalid format.\n");
		return -1;
	}

	stop_idx = argc-1;
	if (strcmp(argv[1], "--start-test") == 0) {
		start_idx = 2;
		test_name = argv[2];
		/* XXX */
		printf("%s\n", test_name);
		printf("argv[3]=%s\n", argv[3]);
		printf("argv[4]=%s\n", argv[4]);
		/* XXX */
		if (strcmp(argv[3], "-t") == 0) {
			++start_idx;
			D_PRINT("Using dynamic tree order\n");
			dynamic_flag = BTR_FEAT_DYNAMIC_ROOT;
			if (strcmp(argv[4], "-m") == 0) {
				++start_idx;
				rc = use_pmem();
			}
		} else if (strcmp(argv[3], "-m") == 0) {
			++start_idx;
			rc = use_pmem();
			if (strcmp(argv[4], "-t") == 0) {
				++start_idx;
				D_PRINT("Using dynamic tree order\n");
				dynamic_flag = BTR_FEAT_DYNAMIC_ROOT;
			}
		}
	} else if (strcmp(argv[1], "--from-file") == 0) {
		#define CMDS_MAX 512
		char cmds[CMDS_MAX];
		FILE *file = fopen(argv[2],"rb");
		assert_non_null(file);
		size_t read = fread(cmds, sizeof(char), CMDS_MAX, file);
		assert_int_not_equal(feof(file), 0);
		assert_int_equal(fclose(file), 0);
		assert_true(read > 2);
		run_cmd_from_file(cmds, read);
		exit(0);
	} else {
		/* TODO: Is this branch still alive? */
		start_idx = 0;
		test_name = "Btree testing tool";
		optind = 0;
		/* Check for -m option first */
		while ((opt = getopt_long(argc, argv, "tmC:Deocqu:d:r:f:i:b:p:",
					  btr_ops, NULL)) != -1) {
			if (opt == 'm') {
				rc = use_pmem();
				break;
			}
			if (opt == 't') {
				D_PRINT("Using dynamic tree order\n");
				dynamic_flag = BTR_FEAT_DYNAMIC_ROOT;
			}
		}
	}

	rc = dbtree_class_register(
	    IK_TREE_CLASS, dynamic_flag | BTR_FEAT_EMBED_FIRST | BTR_FEAT_UINT_KEY, &ik_ops);
	D_ASSERT(rc == 0);

	if (ik_utx == NULL) {
		D_PRINT("Using vmem\n");
		rc = utest_vmem_create(sizeof(*ik_root), &ik_utx);
		D_ASSERT(rc == 0);
	}

	ik_root = utest_utx2root(ik_utx);
	ik_uma = utest_utx2uma(ik_utx);

	/* start over */
	optind = 0;
	rc = run_cmd_line_test(test_name, argv, start_idx, stop_idx);
	daos_debug_fini();
	rc += utest_utx_destroy(ik_utx);
	if (rc != 0)
		printf("Error: %d\n", rc);

	return rc;
}
