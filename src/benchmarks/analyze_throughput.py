import sys
import matplotlib.pyplot as plt
from getters import get_versions,get_datasets,get_patterns
from settings import *
from plotters import plot_bars
import subprocess

log_file = sys.argv[1]
Data = {}

with open(log_file,'rb') as log:

    for row in log:
        v = str(row.split("version: ")[1].split()[0])
        pat = row.split("patterns: ")[1].split()[0].split('/')[-1]
        dataset = row.split("dataset: ")[1].split()[0].split('/')[-1]
        time = float(row.split("seconds: ")[1].split()[0].split('/')[-1])
        thr = dataset_sizes[dataset]/time
        print v,pat,dataset,time
        if Data.has_key((v,pat,dataset)):
            Data[(v,pat,dataset)].append(thr)
        else:
            Data[(v,pat,dataset)] = [thr]

#normalize
for x in Data:
    Data[x] = float(sum(Data[x]))/len(Data[x])

#aggregate ISCX12 Full
for v in versions:
    for p in patterns:
        l = [ 'testbed-12jun_1.pcap', 'testbed-12jun_2.pcap', 'testbed-12jun_3.pcap', 'testbed-12jun_4.pcap']
        s = 0
        for x in l:
            #sum times not throughput
            s += dataset_sizes[x]/Data[(v,p,x)]
        Data[(v,pat,'testbed-12-full.pcap')] = dataset_sizes['testbed-12-full.pcap'] / s
        #delete the chunks
        del Data[(v,pat,'testbed-12jun_2.pcap')]
        del Data[(v,pat,'testbed-12jun_3.pcap')]
        del Data[(v,pat,'testbed-12jun_4.pcap')]
#del dataset_names[3:6] #Warning!
delete_chunks()

#turn to Mbps
for x in Data:
    Data[x] = Data[x]*8/1000000.0

for x in Data:
    print x,Data[x]
print "---------"


(x1,y1) =  get_datasets(Data,versions[0],patterns[0])
(x2,y2) =  get_datasets(Data,versions[1],patterns[0])
(x3,y3) =  get_datasets(Data,versions[2],patterns[0])
(x4,y4) =  get_datasets(Data,versions[3],patterns[0])
#plt.plot(range(len(x1)),y1,label="CPU")
#plt.plot(range(len(x2)),y2,label="GPU single")
#plt.plot(range(len(x3)),y3,label="GPU double")
#plt.plot(range(len(x4)),y4,label="CPU original")
#plt.legend()
#plt.show()

groups = [y1,y2,y3,y4]
title = 'Data sets'
labels = [0,1,2,3,4,5,6]
labels = dataset_names
legend = names
to_compare = []
stdz = [[0]*len(y1)]*4
print stdz
print groups

FIG_SIZE=(10,5)
fig , ax = plt.subplots(1,1,figsize=FIG_SIZE)
lgd = plot_bars(ax,groups,labels,title,legend,to_compare,stdz,show_legend=True)

name="/home/odroid/snort_GPU_system_logs/plots/overall_throughput.pdf"
plt.savefig(name,bbox_extra_artists=(lgd,), bbox_inches = "tight")
subprocess.Popen("pdfcrop "+name+" "+name,shell=True)
subprocess.Popen("pdfcrop")

plt.show()
