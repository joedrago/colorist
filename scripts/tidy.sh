#!/bin/bash

if [ ! -d ./build_tidy ]; then
    mkdir ./build_tidy
fi
cd ./build_tidy

cmake -G Ninja -D CMAKE_EXPORT_COMPILE_COMMANDS=ON ..
run-clang-tidy.py -header-filter=lib/include/colorist/.* -checks='*,-android-cloexec-fopen,-*-braces-around-statements,-cert-dcl03-c,-cert-err34-c,-clang-analyzer-security.insecureAPI.*,-google-readability-todo,-hicpp-signed-bitwise,-hicpp-static-assert,-llvm-header-guard,-misc-static-assert' files lib/include/colorist/* files lib/src/*
