#!/bin/bash

if ! [ -x "$(command -v meson)" ]; then
  echo 'Error: meson is not installed.' >&2
  exit 1
fi
if ! [ -x "$(command -v cargo)" ]; then
  echo 'Error: cargo (Rust) is not installed.' >&2
  exit 1
fi
if ! [ -x "$(command -v nasm)" ]; then
  echo 'Error: nasm is not installed.' >&2
  exit 1
fi
if ! [ -x "$(command -v ninja)" ]; then
  echo 'Error: ninja is not installed.' >&2
  exit 1
fi
if ! [ -x "$(command -v cmake)" ]; then
  echo 'Error: cmake is not installed.' >&2
  exit 1
fi
if ! [ -x "$(command -v git)" ]; then
  echo 'Error: git is not installed.' >&2
  exit 1
fi

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
cd "$DIR/.."
COLORIST_ROOT=`pwd`
echo Colorist Root: $COLORIST_ROOT 

cd ext/avif/ext
$SHELL ./dav1d.cmd
$SHELL ./rav1e.cmd
cd ../../..
mkdir build
cd build
cmake -G Ninja -DAVIF_CODEC_DAV1D=1 -DAVIF_LOCAL_DAV1D=1 -DAVIF_CODEC_RAV1E=1 -DAVIF_LOCAL_RAV1E=1 -DCMAKE_BUILD_TYPE=Release ..
ninja

echo If there are no errors above, "$COLORIST_ROOT/build/bin/colorist/colorist" should be available. Copy/link it somewhere in your PATH.

