meta:
  id: btree_in
  title: btree_fuzz input file
  license: BSD-2-Clause-Patent
  ks-version: 0.11
  endian: le
doc: |
  File format introduced to be both easy to fuzz and describe a nontrivial sequence of btree
  operations.
seq:
  - id: params
    type: btree_parameters
  - id: op
    type: btree_op
    repeat: eos

types:
  btree_parameters:
    seq:
      - id: tree_order
        type: u1
      # tree_feats BTR_FEAT_DIRECT_KEY etc. ?
      - id: seed
        type: u4
      - id: key_num
        type: u1
      - id: value_num
        type: u1
  btree_op:
    seq:
      - id: type
        type: u1
      - id: key_id
        type: u1
      - id: value_id
        type: u1
        if: type % 3 == 0

enums:
  consts:
    3: ops_num

# switch type % ops_num:
# 0: update(key_id, value_id)
# 1: delete(key_id)
# 2: fetch(key_id)
