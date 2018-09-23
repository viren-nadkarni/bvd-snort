# bvd-snort

> Fork of snort v3 which uses OpenCL/GPU for packet processing

## Overview

    Project = Snort++
    Binary = snort
    Version = 3.0.0-a4 build 235
    Base = 2.9.8 build 383

## Dependencies

Install this stuff:

* autotools or cmake to build from source
* daq from http://www.snort.org for packet IO
* dnet from https://github.com/dugsong/libdnet.git for network utility functions
* g++ >= 4.8 or other C++11 compiler
* hwloc from https://www.open-mpi.org/projects/hwloc/ for CPU affinity management
* LuaJIT from http://luajit.org for configuration and scripting
* OpenSSL from https://www.openssl.org/source/ for SHA and MD5 file signatures,
  the protected_content rule option, and SSL service detection
* pcap from http://www.tcpdump.org for tcpdump style logging
* pcre from http://www.pcre.org for regular expression pattern matching
* pkgconfig from https://www.freedesktop.org/wiki/Software/pkg-config/ to locate build dependencies
* zlib from http://www.zlib.net for decompression

## Building

Instructions for Ubuntu 16.04.5 LTS

```
sudo apt install ocl-icd-dev ocl-icd-opencl-dev
make
```

## Running

First set up the environment:

```shell
export LUA_PATH=$my_path/include/snort/lua/\?.lua\;\;
export SNORT_LUA_PATH=$my_path/etc/snort
```

Then give it a go:

* Snort++ provides lots of help from the command line.  Here are some examples:

    ```shell
    $my_path/bin/snort --help
    $my_path/bin/snort --help-module suppress
    $my_path/bin/snort --help-config | grep thread
    ```

* Examine and dump a pcap.  In the following, replace a.pcap with your
  favorite:

    ```shell
    $my_path/bin/snort -r a.pcap
    $my_path/bin/snort -L dump -d -e -q -r a.pcap
    ```

* Verify a config, with or w/o rules:

    ```shell
    $my_path/bin/snort -c $my_path/etc/snort/snort.lua
    $my_path/bin/snort -c $my_path/etc/snort/snort.lua -R $my_path/etc/snort/sample.rules
    ```

* Run IDS mode.  In the following, replace pcaps/ with a path to a directory
  with one or more *.pcap files:

    ```shell
    $my_path/bin/snort -c $my_path/etc/snort/snort.lua -R $my_path/etc/snort/sample.rules \
        -r a.pcap -A alert_test -n 100000
    ```

* Let's suppress 1:2123.  We could edit the conf or just do this:

    ```shell
    $my_path/bin/snort -c $my_path/etc/snort/snort.lua -R $my_path/etc/snort/sample.rules \
        -r a.pcap -A alert_test -n 100000 --lua "suppress = { { gid = 1, sid = 2123 } }"
    ```

* Go whole hog on a directory with multiple packet threads:

    ```shell
    $my_path/bin/snort -c $my_path/etc/snort/snort.lua -R $my_path/etc/snort/sample.rules \
        --pcap-filter \*.pcap --pcap-dir pcaps/ -A alert_fast --max-packet-threads 8
    ```

Additional examples are given in doc/usage.txt.
