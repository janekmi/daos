#include <stdbool.h>

#include <daos/mem.h>
#include <daos/btree.h>

struct ik_rec {
	uint64_t	ir_key;
	uint32_t	ir_val_size;
	uint32_t	ir_val_msize;
	umem_off_t	ir_val_off;
};

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

/**
 * Tree node types.
 * NB: a node can be both root and leaf.
 */
enum btr_node_type {
	BTR_NODE_LEAF		= (1 << 0),
	BTR_NODE_ROOT		= (1 << 1),
};

static inline bool
btr_node_is_leaf(struct btr_node *nd)
{
	return nd->tn_flags & BTR_NODE_LEAF;
}

static inline btr_ops_t *
btr_ops(struct btr_context *tcx)
{
	return tcx->tc_tins.ti_ops;
}

#define BTR_IS_DIRECT_KEY(feats) ((feats) & BTR_FEAT_DIRECT_KEY)
#define BTR_IS_UINT_KEY(feats) ((feats) & BTR_FEAT_UINT_KEY)

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

static uint32_t
btr_hkey_size(struct btr_context *tcx)
{
	return btr_hkey_size_const(btr_ops(tcx), tcx->tc_feats);
}

static inline uint32_t
btr_rec_size(struct btr_context *tcx)
{
	return btr_hkey_size(tcx) + sizeof(struct btr_record);
}

static struct btr_record *
btr_node_rec_at(struct btr_context *tcx, struct btr_node *nd, unsigned int at)
{
	char		*addr = (char *)&nd[1];
	return (struct btr_record *)&addr[btr_rec_size(tcx) * at];
}

static void
btr_print_node(struct btr_context *tcx, struct umem_instance *umm, struct btr_node *node)
{
	struct btr_node *child;
	struct btr_record *record;
	struct ik_rec *irec;

	if (btr_node_is_leaf(node)) {
		for (int i = 0; i < node->tn_keyn; ++i) {
			record = btr_node_rec_at(tcx, node, i);
			irec = umem_off2ptr(umm, record->rec_off);
			printf("%lu\n", irec->ir_key);
		}
		return;
	}

	child = umem_off2ptr(umm, node->tn_child);
	btr_print_node(tcx, umm, child);

	for (int i = 0; i < node->tn_keyn; ++i) {
		record = btr_node_rec_at(tcx, node, i);
		child = umem_off2ptr(umm, record->rec_off);
		btr_print_node(tcx, umm, child);
	}
}

extern daos_handle_t ik_toh;

void
ik_tree_print() {
	struct btr_context *tcx = (struct btr_context *)ik_toh.cookie;
	struct btr_instance *tins = &tcx->tc_tins;
	struct umem_instance *umm = &tins->ti_umm;
	struct btr_root *root = tins->ti_root;
	struct btr_node *node;
	struct btr_record *rec;
	struct ik_rec *irec;
	
	if (tcx->tc_feats & BTR_FEAT_EMBEDDED) {
		rec = &tcx->tc_record;
		irec = umem_off2ptr(&tins->ti_umm, rec->rec_off);
		printf("%lu\n", irec->ir_key);
		return;
	}

	if (root->tr_node == UMOFF_NULL) {
		printf("(empty)\n\n");
		return;
	}

	node = umem_off2ptr(umm, root->tr_node);

	btr_print_node(tcx, umm, node);

	printf("\n");
}
