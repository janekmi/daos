#!/bin/bash

export PMEMOBJ_CONF="sds.at_create=0"
BIN=/home/beside/work/daos-stack/daos/build/debug/afl-gcc/src/common/tests/btree

# KNOWN ISSUES:
# - unknown key: 36, 42, 84, 90
# - SIGSEGV: 39, 45, 87, 93 all basically the same and using BTR_FEAT_EMBEDDED
#   - the issue goes away when the BTR_FEAT_EMBEDDED feature is turned off
# - patch for all of these: https://github.com/daos-stack/daos/pull/15815

# gdbserver localhost:2345 $BIN --from-file testcase_dir/$1.bin

set -e
for bin in testcase_dir/*.bin; do
    echo "$BIN --from-file $bin"
    $BIN --from-file $bin
done
