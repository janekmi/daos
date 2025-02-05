#!/bin/bash

PATH=/home/beside/work/AFL:$PATH
BIN=/home/beside/work/daos-stack/daos/build/debug/afl-gcc/src/common/tests/btree

# afl-cmin -i testcase_dir -o findings_dir $BIN --from-file @@

afl-fuzz -i testcase_min_dir -o findings_dir $BIN --from-file @@

# afl-cmin produces no log on crash/failure. The line could be helpful in this case.
# Copied directly from the afl-cmin script.
# AFL_CMIN_ALLOW_ANY=1 "/home/beside/work/AFL/afl-showmap" -m "100" -t "none" -o "findings_dir/.traces/.run_test" -A "findings_dir/.traces/.cur_input" -- ./btree --from-file @@
