#!/bin/bash

mkdir ./build_analyze
cd ./build_analyze
scan-build cmake -G Ninja ..
scan-build ninja
