#!/bin/bash

export build_path=~/bvd_snort_build
mkdir -p $build_path

export LUA_PATH=$build_path/include/snort/lua/\?.lua\;\;
export SNORT_LUA_PATH=$build_path/etc/snort

