#!/bin/bash

export LD_LIBRARY_PATH=/opt/daos/lib64
export PMEMOBJ_CONF="sds.at_create=0"

gdbserver localhost:2345 ./btree --from-file testcase_dir/$1.bin
