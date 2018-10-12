#!/bin/bash

set -euxo pipefail

./configure_cmake.sh --enable-gdb --prefix=$build_path
cd build
make -j $(nproc) install
