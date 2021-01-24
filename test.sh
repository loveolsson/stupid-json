#!/bin/bash
set -e

reldir=`dirname $0`
cd $reldir

mkdir -p build-tests
cd build-tests
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ../tests/
#cmake -DCMAKE_BUILD_TYPE=Debug ../tests/
make -j
ctest -V