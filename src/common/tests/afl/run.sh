#!/bin/bash

export LD_LIBRARY_PATH=/opt/daos/prereq/release/pmdk/lib/pmdk_debug:/opt/daos/lib64
export PMEMOBJ_CONF="sds.at_create=0"

# KNOWN ISSUES:
# - unknown key: 36, 42, 84, 90
# - SIGSEGV: 39, 45, 87, 93 all basically the same and using BTR_FEAT_EMBEDDED
#   - the issue goes away when the BTR_FEAT_EMBEDDED feature is turned off

gdbserver localhost:2345 ./btree --from-file testcase_dir/$1.bin

# set -e
# for bin in `seq 0 95`; do
#     echo "./btree --from-file testcase_dir/$bin.bin"
#     ./btree --from-file testcase_dir/$bin.bin
# done
