#!/usr/bin/env python3

import yaml
import subprocess

TESTS = '../../../utils/utest.yaml'

BTREE_TESTS = ['btree', 'btree_stress']

BTREE = './btree.sh'

ENV = {
    'PMEMOBJ_CONF': 'sds.at_create=0',
    'BAT_NUM': '10'
}

def main():
    with open(TESTS, 'r') as file:
        tests = yaml.safe_load(file)
    for test_group in tests:
        if test_group['name'] not in BTREE_TESTS:
            continue
        for test in test_group['tests']:
            cmd = [BTREE] + test['cmd'][1:]
            ret = subprocess.run(cmd, env=ENV)
            if ret.returncode != 0:
                raise RuntimeError(f'Failed test: {ret.returncode}')

if __name__ == "__main__":
    main()
