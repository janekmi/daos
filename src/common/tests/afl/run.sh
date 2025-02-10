#!/bin/bash

set -x

export PMEMOBJ_CONF="sds.at_create=0"
BIN=/home/beside/work/daos-stack/daos/build/debug/afl-gcc/src/common/tests/btree
BIN=/home/beside/work/daos-stack/daos/build/debug/gcc/src/common/tests/btree

# KNOWN ISSUES:
# - unknown key: 36, 42, 84, 90
# - SIGSEGV: 39, 45, 87, 93 all basically the same and using BTR_FEAT_EMBEDDED
#   - the issue goes away when the BTR_FEAT_EMBEDDED feature is turned off
# - patch for all of these: https://github.com/daos-stack/daos/pull/15815

LIMIT_MB=100
GDB="gdbserver localhost:2345"
# PRELOAD="/home/beside/work/AFL/libdislocator/libdislocator.so"
# (ulimit -Sv $[LIMIT_MB << 10]; LD_PRELOAD=$PRELOAD $GDB $BIN --from-file $1)
LD_PRELOAD=$PRELOAD $GDB $BIN --from-file $1
# gdb $BIN $1

# set -e
# for bin in findings_dir/crashes/*; do
#     echo "$BIN --from-file $bin"
#     $BIN --from-file $bin
# done
