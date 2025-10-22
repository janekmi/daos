/**
 * (C) Copyright 2025 Hewlett Packard Enterprise Development LP
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */

#ifndef __DAOS_COMMON_BTREE_INTERNAL__
#define __DAOS_COMMON_BTREE_INTERNAL__

#include <stdint.h>

#include <daos/btree.h>

/**
 * Tree node types.
 * NB: a node can be both root and leaf.
 */
enum btr_node_type {
	BTR_NODE_LEAF = (1 << 0),
	BTR_NODE_ROOT = (1 << 1),
};

/**
 * Btree class definition.
 */
struct btr_class {
	/** class feature bits, e.g. hash type for the key */
	uint64_t   tc_feats;
	/** customized function table */
	btr_ops_t *tc_ops;
};

#define BTR_IS_DIRECT_KEY(feats) ((feats) & BTR_FEAT_DIRECT_KEY)

#define BTR_IS_UINT_KEY(feats)   ((feats) & BTR_FEAT_UINT_KEY)

static inline uint32_t
btr_hkey_size_const(btr_ops_t *ops, uint64_t feats)
{
	uint32_t size;

	if (BTR_IS_DIRECT_KEY(feats))
		return sizeof(umem_off_t);

	if (BTR_IS_UINT_KEY(feats))
		return sizeof(uint64_t);

	size = ops->to_hkey_size();

	D_ASSERT(size <= DAOS_HKEY_MAX);
	return size;
}

#define BTR_TYPE_MAX 1024

extern struct btr_class btr_class_registered[BTR_TYPE_MAX];

/**
 * Calculate tree's features.
 *
 * \param[in] tree_class	Tree's class identified.
 * \param[in,out] tree_feats	Tree's features.
 * \param[in] tc		Tree's class.
 *
 * \retval -DER_PROTO	Unsupported features
 * \retval DER_SUCCESS	Success
 */
int
btr_class_feats_init(unsigned int tree_class, uint64_t *tree_feats, struct btr_class *tc);

#endif /** __DAOS_COMMON_BTREE_INTERNAL__ */
