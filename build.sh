#!/bin/bash

set -euxo pipefail

sudo ./configure_cmake.sh --prefix=$build_path
cd build
sudo make -j $(nproc) install
