# bvd-snort

Snort fork with OpenCL/GPGPU-based packet processing

## Info

    Project = Snort++
    Binary = snort
    Version = 3.0.0-a4 build 235
    Base = 2.9.8 build 383

## Requirements

* odroid-xu4 with Ubuntu 16.04.5 LTS
* daq from http://www.snort.org for packet IO
* dnet from https://github.com/dugsong/libdnet.git
* `apt install -y build-essential pkg-config libhwloc-dev hwloc luajit libluajit-5.1-dev libssl-dev libpcap-dev libpcre-dev flex bison zlib1g-dev zlibc ocl-icd-dev ocl-icd-opencl-dev`
* EnergyMonitor from https://github.com/SimonKinds/EnergyMonitor for power consumption benchmarking

## Building

```
mkdir ~/snort_build
export build_path=~/snort_build

bash build.sh
```

## Running

Confirm that OpenCL is detected by the system with `clinfo`. If output is something like:

```
$ clinfo
Number of platforms                               0
```

then setup Mali framebuffer driver and use the vendor ICD file:

```
sudo apt install mali-fbdev
sudo mkdir -p /etc/OpenCL/vendors
sudo bash -c 'echo "/usr/lib/arm-linux-gnueabihf/mali-egl/libOpenCL.so" > /etc/OpenCL/vendors/armocl.icd'
```

To run snort:

```shell
export LUA_PATH=$build_path/include/snort/lua/\?.lua\;\;
export SNORT_LUA_PATH=$build_path/etc/snort

$build_path/bin/snort -c ~/bvd-snort/clort.lua -r ~/testbed-12jun_1.pcap
```
Use appropriate path for the pcap file

### Other usage examples

* help:

    ```shell
    $build_path/bin/snort --help
    $build_path/bin/snort --help-module suppress
    $build_path/bin/snort --help-config | grep thread
    ```

* Examine and dump a pcap

    ```shell
    $build_path/bin/snort -r a.pcap
    $build_path/bin/snort -L dump -d -e -q -r a.pcap
    ```

* Verify a config, with or w/o rules:

    ```shell
    $build_path/bin/snort -c $build_path/etc/snort/snort.lua
    $build_path/bin/snort -c $build_path/etc/snort/snort.lua -R $build_path/etc/snort/sample.rules
    ```

* Run IDS mode. Replace pcaps/ with a path to a directory pcap files:

    ```shell
    $build_path/bin/snort -c $build_path/etc/snort/snort.lua -R $build_path/etc/snort/sample.rules \
        -r a.pcap -A alert_test -n 100000
    ```

---

License: GPLv2
