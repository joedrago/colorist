#!/bin/bash

mkdir ./build_io
cd ./build_io
export CC=clang
export CXX=clang++
cmake -G Ninja -DCOLORIST_TEST_IO=1 ..
ninja colorist-test-io
