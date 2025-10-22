/**
 * (C) Copyright 2025 Hewlett Packard Enterprise Development LP
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */
#define D_LOGFAC DD_FAC(tree)

#include <daos_errno.h>
#include <daos/btree.h>
#include <daos/dlck.h>
#include <daos/dtx.h>

#include "btree_internal.h"

#define INVALID_CLASS_FMT        "Invalid class id: %d"
#define UNREGISTERED_CLASS_FMT   "Unregistered class id %d"
#define UNSUPPORTED_FEATURES_FMT "Unsupported features " DF_X64 "/" DF_X64

/**
 * TODO
 */
static int
dlck_btr_root_check(struct btr_root *root, uint64_t *tree_feats, uint32_t *rec_size,
		    struct dlck_print *dp)
{
	unsigned int      tree_class = root->tr_class;
	struct btr_class *tc;
	*tree_feats = root->tr_feats;
	int rc;

	DLCK_PRINT(dp, "Tree class... ");
	if (tree_class >= BTR_TYPE_MAX || DAOS_FAIL_CHECK(DAOS_FAULT_BTREE_OPEN_INV_CLASS)) {
		DLCK_APPENDFL_ERR(dp, INVALID_CLASS_FMT, tree_class);
		return -DER_INVAL;
	}

	tc = &btr_class_registered[tree_class];
	if (tc->tc_ops == NULL || DAOS_FAIL_CHECK(DAOS_FAULT_BTREE_OPEN_UNREG_CLASS)) {
		DLCK_APPENDFL_ERR(dp, UNREGISTERED_CLASS_FMT, tree_class);
		return -DER_NONEXIST;
	}
	DLCK_APPENDL_OK(dp);

	DLCK_PRINT(dp, "Tree features... ");
	rc = btr_class_feats_init(tree_class, tree_feats, tc);
	if (rc != DER_SUCCESS) {
		DLCK_APPENDFL_ERR(dp, UNSUPPORTED_FEATURES_FMT, *tree_feats, tc->tc_feats);
		return rc;
	}
	DLCK_APPENDL_OK(dp);

	*rec_size = btr_hkey_size_const(tc->tc_ops, *tree_feats);

	return DER_SUCCESS;
}

#define DLCK_BTREE_NODE_MALFORMED_STR "malformed - "
#define DLCK_BTREE_NON_ZERO_PADDING_FMT                                                            \
	DLCK_BTREE_NODE_MALFORMED_STR "non-zero padding (%#" PRIx32 ")"
#define DLCK_BTREE_NON_ZERO_GEN_FMT DLCK_BTREE_NODE_MALFORMED_STR "nd_gen != 0 (%#" PRIx32 ")"

/**
 * Validate the integrity of the btree node.
 *
 * \param[in] nd	Node to check.
 * \param[in] nd_off	Node's offset.
 * \param[in] dp	DLCK print utility.
 *
 * \retval DER_SUCCESS	The node is correct.
 * \retval -DER_NOTYPE	The node is malformed.
 */
static int
dlck_btr_node_check(struct btr_node *nd, umem_off_t nd_off, struct dlck_print *dp)
{
	uint16_t unknown_flags;

	D_ASSERT(dp != NULL);
	DLCK_PRINTF(dp, "Node (off=%#x)... ", nd_off);

	unknown_flags = nd->tn_flags & ~(BTR_NODE_LEAF | BTR_NODE_ROOT);
	if (unknown_flags != 0) {
		DLCK_APPENDFL_ERR(dp, DLCK_BTREE_NODE_MALFORMED_STR "unknown flags (%#" PRIx16 ")",
				  unknown_flags);
		return -DER_NOTYPE;
	}

	if (nd->tn_pad_32 != 0) {
		if (dp->options->non_zero_padding == DLCK_EVENT_ERROR) {
			DLCK_APPENDFL_ERR(dp, DLCK_BTREE_NON_ZERO_PADDING_FMT, nd->tn_pad_32);
			return -DER_NOTYPE;
		} else {
			DLCK_APPENDFL_WARN(dp, DLCK_BTREE_NON_ZERO_PADDING_FMT, nd->tn_pad_32);
		}
	}

	if (nd->tn_gen != 0) {
		if (dp->options->non_zero_padding == DLCK_EVENT_ERROR) {
			DLCK_APPENDFL_ERR(dp, DLCK_BTREE_NON_ZERO_GEN_FMT, nd->tn_gen);
			return -DER_NOTYPE;
		} else {
			DLCK_APPENDFL_WARN(dp, DLCK_BTREE_NON_ZERO_GEN_FMT, nd->tn_gen);
		}
	}

	DLCK_APPENDL_OK(dp);

	return DER_SUCCESS;
}

struct node_info {
	d_list_t   link;
	umem_off_t nd_off;
};

static inline uint32_t
btr_rec_size(btr_ops_t *tree_ops, uint64_t tree_feats)
{
	return btr_hkey_size_const(tree_ops, tree_feats) + sizeof(struct btr_record);
}

static struct btr_record *
btr_node_rec_at(struct btr_node *nd, uint32_t rec_size, unsigned int at)
{
	char *addr = (char *)&nd[1];

	return (struct btr_record *)&addr[rec_size * at];
}

static umem_off_t
btr_node_child_at(struct btr_node *nd, uint32_t rec_size, unsigned int at)
{
	struct btr_record *rec;

	D_ASSERT(!(nd->tn_flags & BTR_NODE_LEAF));
	/* NB: non-leaf node has +1 children than number of keys */
	if (at == 0) {
		return nd->tn_child;
	}

	rec = btr_node_rec_at(nd, rec_size, at - 1);
	return rec->rec_off;
}

/**
 * Validate the integrity of a btree.
 *
 * \param[in]	toh	Tree handle.
 *
 * \retval DER_SUCCESS		The tree is correct.
 * \retval -DER_NOTYPE		The tree is malformed.
 * \retval -DER_NONEXIST	The tree is malformed.
 * \retval -DER_*		Possibly other errors.
 */
static int
dlck_dbtree_check(struct btr_root *root, uint64_t tree_feats, uint32_t rec_size,
		  struct umem_instance *umm, struct dlck_print *dp)
{
	D_LIST_HEAD(node_list);
	struct node_info *ni;
	struct node_info *ni_tmp;
	struct btr_node  *nd;
	int               rc = DER_SUCCESS;

	D_ASSERT(dp != NULL);

	if (UMOFF_IS_NULL(root->tr_node)) {
		DLCK_PRINT(dp, "Empty tree\n");
		return DER_SUCCESS;
	}

	D_ASSERT((tree_feats & BTR_FEAT_EMBEDDED) == 0);

	D_ALLOC_PTR(ni);
	ni->nd_off = root->tr_node;
	d_list_add_tail(&ni->link, &node_list);

	d_list_for_each_entry_safe(ni, ni_tmp, &node_list, link) {
		nd = umem_off2ptr(umm, ni->nd_off);

		/** check the node */
		rc = dlck_btr_node_check(nd, ni->nd_off, dp);
		if (rc != DER_SUCCESS) {
			return rc;
		}

		/** remove the node from the list */
		d_list_del(&ni->link);
		D_FREE(ni);

		/** a leaf has no child nodes */
		if (nd->tn_flags & BTR_NODE_LEAF) {
			continue;
		}

		/** append the node's children to the list */
		for (int at = 0; at < nd->tn_keyn - 1; ++at) {
			D_ALLOC_PTR(ni);
			ni->nd_off = btr_node_child_at(nd, rec_size, at);
			d_list_add_tail(&ni->link, &node_list);
		}
	}

	return rc;
}

/**
 * Open a btree from the root address.
 *
 * \param[in] root	Address of the tree root.
 * \param[in] uma	Memory class attributes.
 * \param[in] coh	The container open handle.
 * \param[in] priv	Private data for tree opener
 * \param[in] dp	DLCK print utility.
 * \param[out] toh	Returned tree open handle.
 */
int
dbtree_open_inplace_dp(struct btr_root *root, struct umem_attr *uma, daos_handle_t coh, void *priv,
		       struct dlck_print *dp, daos_handle_t *toh)
{
	struct umem_instance umm;
	uint64_t             tree_feats;
	uint32_t             rec_size;
	int                  rc;

	rc = umem_class_init(uma, &umm);
	if (rc != DER_SUCCESS) {
		return rc;
	}

	rc = dlck_btr_root_check(root, &tree_feats, &rec_size, dp);
	if (rc != DER_SUCCESS) {
		return rc;
	}

	if (IS_DLCK(dp)) {
		DLCK_PRINT(dp, "Nodes:\n");
		DLCK_INDENT(dp, rc = dlck_dbtree_check(root, tree_feats, rec_size, &umm, dp));
		if (rc != DER_SUCCESS) {
			return rc;
		}
	}

	return dbtree_open_inplace_ex(root, uma, coh, priv, toh);
}
