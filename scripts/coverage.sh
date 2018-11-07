#!/bin/bash

mkdir ./build_coverage
cd ./build_coverage
export CC=clang
export CXX=clang++
cmake -G Ninja -DCOLORIST_COVERAGE=1 ..
ninja colorist-test-ccov
