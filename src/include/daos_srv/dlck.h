#ifndef __DAOS_DLCK_H__
#define __DAOS_DLCK_H__

#include <daos/mem.h>

enum DLCK_btree_node_fault {
	DNF_NODE_OFF_INVALID,
	DNF_DIRECT_KEY_OFF_INVALID,
	DNF_CHILD_OFF_INVALID,
};

struct DLCK_btree_faulty_node {
	umem_off_t                 dbn_off;
	enum DLCK_btree_node_fault dbn_fault;
};

#define DLCK_BTREE_NODES_GROW_BY 10

struct DLCK_btree_faulty_nodes_array {
	struct DLCK_btree_faulty_node *dba_nodes;
	int                            dba_nodes_num;
};

int
dlck_vos_pool_containers_check(daos_handle_t poh, struct DLCK_btree_faulty_nodes_array *array);

int
dlck_vos_cont_dtx_recover(daos_handle_t coh);

int
dlck_dbtree_check(daos_handle_t toh, struct DLCK_btree_faulty_nodes_array *array);

bool
DLCK_check_array_zero(uint64_t *array, int num);

#define DLCK_CHECK_ARRAY_ZERO(array) DLCK_check_array_zero(array, ARRAY_SIZE(array))

#define STRINGIFY(x)                 #x

struct DLCK_value {
	uint32_t    dv_value;
	const char *dv_value_name;
};

#define DLCK_VALUE_NUM_MAX 10

typedef bool (*DLCK_ask_yes_no)(const char *);
typedef int (*DLCK_ask_fix_value_callback)(struct DLCK_value *, int);
typedef bool (*DLCK_check_offset)(umem_off_t);

struct DLCK_callbacks {
	DLCK_ask_yes_no             dc_ask_yes_no;
	DLCK_ask_fix_value_callback dc_ask_value;
	DLCK_check_offset           dc_check_offset;
};

extern struct DLCK_callbacks *DLCK_Callbacks;

#ifdef DLCK_ENABLED
#define DLCK_CALL_CHECK(check_func, rc_var, ...)                                                   \
	do {                                                                                       \
		if (rc_var == DER_SUCCESS) {                                                       \
			rc_var = check_func(__VA_ARGS__);                                          \
		}                                                                                  \
	} while (0)
#define DLCK_CALL_CHECK_GOTO(check_func, rc_var, go_to_on_error, ...)                              \
	do {                                                                                       \
		rc_var = check_func(__VA_ARGS__);                                                  \
		if (rc_var != DER_SUCCESS) {                                                       \
			goto go_to_on_error;                                                       \
		}                                                                                  \
	} while (0)
#define DLCK_CALL_CHECK_RETURN(check_func, ...)                                                    \
	do {                                                                                       \
		int ret = check_func(__VA_ARGS__);                                                 \
		if (ret != DER_SUCCESS) {                                                          \
			return ret;                                                                \
		}                                                                                  \
	} while (0)
#else
#define DLCK_CALL_CHECK(check_func, rc_var, ...)
#define DLCK_CALL_CHECK_GOTO(check_func, rc_var, go_to_on_error, ...)
#define DLCK_CALL_CHECK_RETURN(check_func, ...)
#endif

struct dlck_dtx_rec {
	uint32_t   lid;
	umem_off_t umoff;
};

struct dlck_dtx_rec_array {
	uint32_t             dda_len;     /** Current length of array */
	uint32_t             dda_max_len; /** Allocated length of array */
	struct dlck_dtx_rec *dda_rec;     /** Entries in array */
};

#define DLCK_DTX_REC_ARRAY_GROW_BY 10

/**
 * XXX
 */
int
dlck_dtx_rec_array_append(struct dlck_dtx_rec_array *dda, struct dlck_dtx_rec *rec);

/**
 * XXX
 */
void
dlck_dtx_rec_array_move(struct dlck_dtx_rec_array *dst, struct dlck_dtx_rec_array *src);

/**
 * XXX
 */
void
dlck_dtx_rec_array_free(struct dlck_dtx_rec_array *dda);

#endif /* __DAOS_DLCK_H__ */
