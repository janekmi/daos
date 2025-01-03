#!/bin/bash

export LD_LIBRARY_PATH=/opt/daos/lib64

gdbserver localhost:2345 ./btree --from-file testcase_dir/0.bin
