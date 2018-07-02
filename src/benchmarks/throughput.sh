#!/bin/bash


src_dir="/home/odroid/snort_GPU_system/Master/src/search_engines"
make_dir="/home/odroid/snort_GPU_system/Master/build"
exec_dir="/home/odroid/snort_GPU_system_build"

log_dir="/home/odroid/snort_GPU_system_logs"
log_name="throughput."$(date +%Y-%m-%d_%H:%M)".log"
config="/home/odroid/Downloads/clort.lua"

echo $log_dir"/"$log_name
versions=(0 1 2)
#0-CPU, 1-GPU single, 2-GPU double

patterns=(\
	/home/odroid/Downloads/snort3-community-rules/snort3-community.rules \
	)

datasets=(\
	/home/odroid/Downloads/smallFlows.pcap \
	/home/odroid/Downloads/bigFlows.pcap \
	)


for ver in "${versions[@]}"
do
	cd $src_dir
	sed -i -e "s/USE_GPU [0-2]/USE_GPU $ver/g" acsmx2.h
	cd $make_dir
	make -j 8 install || exit 1
	cd $exec_dir
	for data in "${datasets[@]}"
	do
		for pat in "${patterns[@]}"
		do
			echo $ver $data $pat
			res="$(./bin/snort -c $config -r $data -R $pat |grep -oP "seconds: [+-]?([0-9]*[.])?[0-9]+")"
			echo "version: "$ver "patterns: "$pat "dataset: "$data $res
			echo "version: "$ver "patterns: "$pat "dataset: "$data $res >> $log_dir"/"$log_name
			
		done
	done
done

