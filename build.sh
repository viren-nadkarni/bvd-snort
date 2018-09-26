#!/bin/bash

set -euxo pipefail

sudo ./configure_cmake.sh --prefix=$build_path
cd build
make -j $(nproc) install
