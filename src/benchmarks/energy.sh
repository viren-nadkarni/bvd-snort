#!/bin/bash


src_dir="/home/odroid/snort_GPU_system/Master/src/search_engines"
make_dir="/home/odroid/snort_GPU_system/Master/build"
exec_dir="/home/odroid/snort_GPU_system_build"
monitor_dir="/home/odroid/snort_simon_benchmarks/EnergyMonitor/build"

log_dir="/home/odroid/snort_GPU_system_logs"
log_name="energy."$(date +%Y-%m-%d_%H:%M)".log"
config="/home/odroid/Downloads/clort.lua"

versions=(0 1 2 3)
#0-CPU, 1-GPU single, 2-GPU double

patterns=(\
	/home/odroid/Downloads/snort3-community-rules/snort3-community.rules \
	)

datasets=(\
	#/home/odroid/Downloads/smallFlows.pcap \
	#/home/odroid/Downloads/bigFlows.pcap \
	/home/odroid/Downloads/testbed-12jun_1.pcap \
	#/home/odroid/Downloads/testbed-12jun_2.pcap \
	#/home/odroid/Downloads/testbed-12jun_3.pcap \
	#/home/odroid/Downloads/testbed-12jun_4.pcap \
	#/home/odroid/Downloads/testbed-13jun_1.pcap \
	)


for ver in "${versions[@]}"
do
	cd $src_dir
	sed -i -e "s/USE_GPU [0-9]/USE_GPU $ver/g" acsmx2.h
	cd $make_dir
	make -j 8 install || exit 1

	

	for data in "${datasets[@]}"
	do
		for pat in "${patterns[@]}"
		do
				
			cd $monitor_dir
			./energy-monitor > $ver"_energy" &
			pkill -USR1 energy-monitor
			sleep 3

			cd $exec_dir
			echo $ver $data $pat
			./bin/snort -c $config -r $data -R $pat >\devnull
			echo "version: "$ver "patterns: "$pat "dataset: "$data 
			
			sleep 3
			pkill -SIGTERM energy-monitor

		done
	done
done

