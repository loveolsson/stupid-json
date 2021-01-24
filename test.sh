#!/bin/bash
set -e

reldir=`dirname $0`
cd $reldir

mkdir -p build-tests
cd build-tests
cmake -DCMAKE_BUILD_TYPE=Release ../tests/
make -j
ctest -V