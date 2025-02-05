#!/usr/bin/env python3

import argparse
from enum import IntEnum

import yaml

INPUT = 'btree_cmds.yml'
OUTPUT_DEBUG = 'testcases.yml'
OUTPUT_DIR = 'testcase_dir'

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

IK_SEP = ','
IK_SEP_VAL = ':'

class Opt(IntEnum):
    USE_PMEM = 1
    USE_DYNAMIC_ROOT = 2

class Op(IntEnum):
    CREATE = 0
    CLOSE = 1
    DESTROY = 2
    OPEN = 3
    UPDATE = 4
    ITER = 5
    QUERY = 6
    LOOKUP = 7
    DELETE = 8
    DRAIN = 9

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

FIRST_OPTS_SHIFT=(8 - 2)
FIRST_SEED_MASK=0b00111111

OP_CREATE_OPTS_SHIFT=(8 - 3)
OP_CREATE_ORDER_MASK=0b00011111

def op_create_open(optarg: str):
    # i,o:11
    opts = 0
    order = 0 # max 64
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
    # combine opts and order to limit the number of irrelevant bits
    opts_and_order = (opts << OP_CREATE_OPTS_SHIFT) + (order & OP_CREATE_ORDER_MASK)
    output = [Op.CREATE.value, opts_and_order]
    # print(output)
    return output, True

def op_perf(optarg):
    return [], True

def op_close_destroy(_):
    output = [Op.DESTROY.value]
    # print(output)
    return output, False

def op_close_destroy2(_):
    output = [Op.CLOSE.value]
    # print(output)
    return output, False

def op_create_open2(_):
    output = [Op.OPEN.value]
    # print(output)
    return output, False

def op_update(optarg: str):
    if optarg != '7:loaded,3:that,5:dice,2:knows,4:the,6:are,1:Everybody':
        raise ValueError(f'Unexpected update value: {optarg}')
    # Instead of:
    # - hardcoding keys and values
    # a more robust scenario:
    # - generate keys and values at random
    output = [Op.UPDATE.value, 7]
    # print(output)
    return output, True

def op_iterate(optarg: str):
    # b
    # f:3
    values = optarg.split(IK_SEP_VAL)
    # Instead of:
    # - hardcoding either going backwards or forward
    # a more robust scenario:
    # - start either at the front, at the back or at a key picked by random
    # - move at random
    output = [Op.ITER.value, int(values[1]) if len(values) > 1 else 0]
    # print(output)
    return output, True

def op_query(_):
    output = [Op.QUERY.value]
    # print(output)
    return output, False

def op_lookup(optarg: str):
    if optarg != '3,6,5,7,2,1,4':
        raise ValueError(f'Unexpected update value: {optarg}')
    # Instead of:
    # - hardcoding keys
    # a more robust scenario:
    # - pick up keys at random
    # - including non-existing and deleted
    output = [Op.LOOKUP.value, 7]
    # print(output)
    return output, True

def op_delete(optarg):
    if optarg != '3,6,5,7,2,1,4':
        raise ValueError(f'Unexpected update value: {optarg}')
    # Instead of:
    # - hardcoding keys
    # a more robust scenario:
    # - pick up keys at random
    # - including non-existing and deleted
    output = [Op.DELETE.value, 7]
    # print(output)
    return output, True

def op_delete_retain(optarg):
    return [], True

def op_batch(optarg):
    if optarg != '10':
        raise ValueError(f'Unexpected update value: {optarg}')
    # Instead of:
    # - hardcoding a sequence
    # a more robust scenario:
    # - write it down and expose it to AFL
    output = [
        Op.UPDATE.value, 10,
        Op.UPDATE.value, 10,
        Op.QUERY.value,
        Op.LOOKUP.value, 10,
        Op.DELETE.value, 10,
        Op.LOOKUP.value, 10,
        Op.DELETE.value, 10,
    ]
    return output, True

def op_drain(_):
    output = [
        Op.UPDATE.value, 10,
        Op.DRAIN.value, 7]
    # print(output)
    return output, False

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
    # [Opt, Seed]
    opt = (Opt.USE_PMEM.value if args.use_pmem else 0) + \
        (Opt.USE_DYNAMIC_ROOT.value if args.use_dynamic_root else 0)
    seed = 73 # arbitrary pseudo-random seed
    opt_and_seed = (opt << FIRST_OPTS_SHIFT) + (seed & FIRST_SEED_MASK)
    cmdb = [opt_and_seed]
    skip_one = False
    for i in range(len(cmd)):
        if skip_one:
            skip_one = False
            continue
        if cmd[i] in SKIP_OPS:
            continue
        if cmd[i] not in OPS.keys():
            raise ValueError(f'Unknown op: {cmd[i]}')
        output, skip_one = OPS[cmd[i]](cmd[i + 1] if i + 1 < len(cmd) else None)
        cmdb += output
    return cmdb

def compare(a: list, b: list):
    if len(a) != len(b):
        return False
    common = [av for av, bv in zip(a, b) if av == bv]
    return len(common) == len(a)

def main():
    with open(INPUT) as file:
        cmds = yaml.safe_load(file)
    cmdbs = [] # binary commands
    for cmd in cmds:
        cmdb = translate(cmd)
        cmdbs.append(cmdb)
    print(len(cmdbs))
    ucmdbs = [] # unique binary commands
    for a in cmdbs:
        already_there = False
        for b in ucmdbs:
            if compare(a, b):
                already_there = True
                break
        if not already_there:
            ucmdbs.append(a)
    print(len(ucmdbs))
    # dump collected binaries commands as text for inspection
    with open(OUTPUT_DEBUG, 'w') as file:
        yaml.dump(ucmdbs, file)
    # write binary command files
    for i in range(len(ucmdbs)):
        with open(f'{OUTPUT_DIR}/{i}.bin', 'wb') as file:
            file.write(bytes(ucmdbs[i]))

if __name__ == "__main__":
    main()
