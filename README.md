# bvd-snort

Snort fork with OpenCL/GPGPU-based packet processing engine

## Info

    Project = Snort++
    Binary = snort
    Version = 3.0.0-a4 build 235
    Base = 2.9.8 build 383

The Aho-Corasick pattern search algorithm has been implemented in OpenCL and can be used by specifying `ac_gpu` as `search_method` in config. The new engine is just a prototype and is not fully integrated into Snort yet. For example, logging and alerts do not work. There are also some issues with false positives when using more complex rulesets.

## Requirements

* daq from https://www.snort.org/downloads/snortplus/daq-2.2.2.tar.gz
* dnet from https://github.com/dugsong/libdnet.git
* `sudo apt install -y build-essential pkg-config libhwloc-dev hwloc luajit libluajit-5.1-dev libssl-dev libpcap-dev libpcre3-dev flex bison zlib1g-dev zlibc ocl-icd-dev ocl-icd-opencl-dev clinfo cmake`
* Tested on odroid-xu4 with Ubuntu 18.04 LTS

If OpenCL is not detected by the system (check with `clinfo`), then setup Mali driver:

```
sudo apt install mali-fbdev
sudo mkdir -p /etc/OpenCL/vendors
sudo bash -c 'echo "/usr/lib/arm-linux-gnueabihf/mali-egl/libOpenCL.so" > /etc/OpenCL/vendors/armocl.icd'
```

## Building

```
export build_path=~/snort_build
mkdir -p $build_path

./configure_cmake.sh --prefix=$build_path
cd build && make -j $(nproc) install
```

or simply use the helper scripts:

```
. env.sh
./build.sh
```

If build fails with `fatal error: dnet/sctp.h: No such file or directory`:

```
sudo cp ~/libdnet/include/dnet/* /usr/local/include/dnet/
```

## Running

```shell
export LUA_PATH=$build_path/include/snort/lua/\?.lua\;\;
export SNORT_LUA_PATH=$build_path/etc/snort

$build_path/bin/snort -c snort.lua -R test.rules -r ~/smallFlows.pcap
```

Use appropriate paths. Sample pcap files are available [here](http://tcpreplay.appneta.com/wiki/captures.html).

---

License: GPLv2
