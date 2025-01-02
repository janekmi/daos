#!/usr/bin/env python3

import yaml
import sys

START_TEST = '--start-test'

OUTPUT = 'btree_cmds.yml'

def main():
    argv = sys.argv[2:]
    if argv[0] != START_TEST:
        raise ValueError(f'The first argument is different than expected: {argv[0]} != {START_TEST}')
    if argv[1][0] == '-':
        raise ValueError(f'Unexpected option on the way: "{argv[1]}"')
    argv = argv[2:]
    # print(argv)
    try:
        with open(OUTPUT) as file:
            cmds = yaml.safe_load(file)
    except FileNotFoundError:
        cmds = []
    cmds.append(argv)
    with open(OUTPUT, 'w') as file:
        yaml.dump(cmds, file)

if __name__ == "__main__":
    main()
