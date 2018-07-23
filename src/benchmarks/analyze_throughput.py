import sys
import matplotlib.pyplot as plt
from getters import get_versions,get_datasets,get_patterns
from settings import *
from plotters import plot_bars
import subprocess
import numpy as np

def delete_ISCX_chunks(D,Z):

    #aggregate ISCX12 Full
    for v in versions:
        for p in patterns:
            l = [ 'testbed-12jun_1.pcap', 'testbed-12jun_2.pcap', 'testbed-12jun_3.pcap', 'testbed-12jun_4.pcap']
            s = 0
            z = 0
            for x in l:
                #sum times not throughput
                s += dataset_sizes[x]/Data[(v,p,x)]
                z += Dev[(v,p,x)]
            D[(v,pat,'testbed-12-full.pcap')] = dataset_sizes['testbed-12-full.pcap'] / s
            Z[(v,pat,'testbed-12-full.pcap')] = z/4.0
            #delete the chunks
            del D[(v,pat,'testbed-12jun_2.pcap')]
            del D[(v,pat,'testbed-12jun_3.pcap')]
            del D[(v,pat,'testbed-12jun_4.pcap')]
            
            del Z[(v,pat,'testbed-12jun_2.pcap')]
            del Z[(v,pat,'testbed-12jun_3.pcap')]
            del Z[(v,pat,'testbed-12jun_4.pcap')]
    #del dataset_names[3:6] #Warning!
    delete_chunks()

log_file = sys.argv[1]
Data = {}
Dev = {}
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

#turn to Mbps
for x in Data:
    Data[x] = [y*8/1000000.0 for y in Data[x]]

#get stand dev
for x in Data:
    Dev[x] = np.std(Data[x])

#normalize
for x in Data:
    Data[x] = float(sum(Data[x]))/len(Data[x])

delete_ISCX_chunks(Data,Dev)


for x in Data:
    print x,Data[x]
print "---------"



(x1,y1) =  get_datasets(Data,versions[0],patterns[0])
(x2,y2) =  get_datasets(Data,versions[1],patterns[0])
(x3,y3) =  get_datasets(Data,versions[2],patterns[0])
(x4,y4) =  get_datasets(Data,versions[3],patterns[0])


(s1,z1) =  get_datasets(Dev,versions[0],patterns[0])
(s2,z2) =  get_datasets(Dev,versions[1],patterns[0])
(s3,z3) =  get_datasets(Dev,versions[2],patterns[0])
(s4,z4) =  get_datasets(Dev,versions[3],patterns[0])

#(x1,y1) =  get_patterns(Data,versions[0],datasets[2])
#(x2,y2) =  get_patterns(Data,versions[1],datasets[2])
#(x3,y3) =  get_patterns(Data,versions[2],datasets[2])
#(x4,y4) =  get_patterns(Data,versions[3],datasets[2])

groups = [y1,y2,y3,y4]
title = 'Data sets'
labels = dataset_names
labels = [\
		'smallFlows',\
                'bigFlows',\
                'ISCX12 131',\
                'ISCX12 121',\
                'ISCX12 121-full',\
	]
#title = 'Number of pattenrs'
#labels = ['Default (829)', 'Intermediate (2000)', 'Full (3370)']
legend = names
to_compare = []
#stdz = [[0]*len(y1)]*4
stdz = [z1,z2,z3,z4] 
print stdz
print groups

FIG_SIZE=(10,3)
fig , ax = plt.subplots(1,1,figsize=FIG_SIZE)
lgd = plot_bars(ax,groups,labels,title,legend,to_compare,stdz,show_legend=True)
ax.grid()
name="/home/odroid/snort_GPU_system_logs/plots/overall_throughput.pdf"
#name="/home/odroid/snort_GPU_system_logs/plots/patterns_ISCX_131.pdf"
plt.savefig(name,bbox_extra_artists=(lgd,), bbox_inches = "tight")
subprocess.Popen("pdfcrop "+name+" "+name,shell=True)
subprocess.Popen("pdfcrop")

plt.show()

print "Double buff vs CPU modified"
for i in range(len(y2)):
    print float(y4[i])/y2[i]

print "Double buff vs single buff"
for i in range(len(y3)):
    print float(y4[i])/y3[i]
