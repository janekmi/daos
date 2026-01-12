#!/usr/bin/env python3
"""
  (C) Copyright 2026 Hewlett Packard Enterprise Development LP

  SPDX-License-Identifier: BSD-2-Clause-Patent
"""

import random
from typing import BinaryIO

# TODO: Consider using Kaitai also for generating the input buffer.
# https://doc.kaitai.io/serialization.html


def btree_parameters(f: BinaryIO) -> int:
    border = random.randint(3, 30)
    seed = random.randint(1, 1 << 32)
    bseed = seed.to_bytes(4, byteorder="little")
    bkey_num = random.randint(10, 255)
    bvalue_num = random.randint(10, 255)
    params = [border] + list(bseed) + [bkey_num, bvalue_num]
    f.write(bytes(params))
    return len(params)


def btree_op(f: BinaryIO) -> int:
    type_ = random.randint(0, 2)
    key_id = random.randint(0, 255)
    value_id = random.randint(0, 255)
    if type_ == 0:
        op = [type_, key_id, value_id]
    else:
        op = [type_, key_id]
    f.write(bytes(op))
    return len(op)


random.seed(123)

with open("input.bin", "wb") as f:
    size = btree_parameters(f)
    while size < 1024:
        size += btree_op(f)
