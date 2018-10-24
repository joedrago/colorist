#!/bin/bash

mkdir ./build_coverage
cd ./build_coverage
export CC=/usr/bin/clang
export CXX=/usr/bin/clang++
cmake -G Ninja -DCOLORIST_COVERAGE=1 ..
ninja colorist-test-ccov
