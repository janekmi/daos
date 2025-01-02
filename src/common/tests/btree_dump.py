#!/usr/bin/env python3

import argparse
import yaml

from enum import IntEnum

OUTPUT = 'btree_cmds.yml'

PARSER = argparse.ArgumentParser(exit_on_error=False)
PARSER.add_argument('-m', '--use_pmem', action='store_true')
PARSER.add_argument('-t', '--use_dynamic_root', action='store_true')
# The order of the options below is crucial so they cannot be used as parsed by argparse
PARSER.add_argument('-C')
PARSER.add_argument('-p')
PARSER.add_argument('-D', action='store_true')
PARSER.add_argument('-c', action='store_true')
PARSER.add_argument('-o', action='store_true')
PARSER.add_argument('-u')
PARSER.add_argument('-i')
PARSER.add_argument('-q', action='store_true')
PARSER.add_argument('-f')
PARSER.add_argument('-d')
PARSER.add_argument('-r')
PARSER.add_argument('-b')
PARSER.add_argument('-e', action='store_true')

SKIP_OPS = ['-m', '-t']

class Op(IntEnum):
    CREATE = 1

class OpCreateOpt(IntEnum):
    INPLACE = 1
    UINT_KEY = 2
    EMBED_FIRST = 4

STR_2_OpCreateOpt = {
    'i': OpCreateOpt.INPLACE,
    '%': OpCreateOpt.EMBED_FIRST,
    '+': OpCreateOpt.UINT_KEY,
}

OpCreateOpt_Special = ['+', '%']

def op_create_open(optarg: str):
    print(optarg)
    # i,o:11
    IK_SEP = ','
    IK_SEP_VAL = ':'
    # output = [Op.CREATE.value, ]
    opts = 0
    order = 0
    if optarg[0] in OpCreateOpt_Special:
        opts += STR_2_OpCreateOpt[optarg[0]]
        optarg = optarg[1:]

    pairs = optarg.split(IK_SEP)
    for pair in pairs:
        kv = pair.split(IK_SEP_VAL)
        if kv[0] == 'o':
            order = int(kv[1])
        elif kv[0] in STR_2_OpCreateOpt.keys():
            opts += STR_2_OpCreateOpt[kv[0]]
        else:
            raise ValueError(f'Unknown key: {kv[0]}')
    return True

def op_perf(optarg):
    return True

def op_close_destroy(_):
    return False

def op_close_destroy2(_):
    return False

def op_create_open2(_):
    return False

def op_update(optarg):
    return True

def op_iterate(optarg):
    return True

def op_query(_):
    return False

def op_lookup(optarg):
    return True

def op_delete(optarg):
    return True

def op_delete_retain(optarg):
    return True

def op_batch(optarg):
    return True

def op_drain(_):
    return False

OPS = {
    '-C': op_create_open,
    '-p': op_perf,
    '-D': op_close_destroy,
    '-c': op_close_destroy2,
    '-o': op_create_open2,
    '-u': op_update,
    '-i': op_iterate,
    '-q': op_query,
    '-f': op_lookup,
    '-d': op_delete,
    '-r': op_delete_retain,
    '-b': op_batch,
    '-e': op_drain,
}

def translate(cmd):
    # print(cmd)
    args = PARSER.parse_args(cmd)
    skip_one = False
    for i in range(len(cmd)):
        if skip_one:
            skip_one = False
            continue
        if cmd[i] in SKIP_OPS:
            continue
        if cmd[i] not in OPS.keys():
            raise ValueError(f'Unknown op: {cmd[i]}')
        skip_one = OPS[cmd[i]](cmd[i + 1] if i + 1 < len(cmd) else None)

def main():
    with open(OUTPUT) as file:
        cmds = yaml.safe_load(file)
    for cmd in cmds:
        cmd_byte = translate(cmd)

if __name__ == "__main__":
    main()
