#!/bin/bash

mkdir ./build_io
cd ./build_io
export CC=clang
export CXX=clang++
cmake -DCMAKE_BUILD_TYPE=Release -DCOLORIST_TEST_IO=1 ..
make colorist-test-io
