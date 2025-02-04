#!/bin/bash

scons COMPILER=afl-gcc BUILD_TYPE=debug PREFIX=/opt/daos install -j 16 --build-deps=yes --config=force
