import sys
import matplotlib.pyplot as plt
from getters import get_versions,get_datasets,get_patterns
from settings import *
from plotters import plot_bars

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
title = 'asdf'
labels = [0,1,2,3,4,5,6]
labels = dataset_names
legend = names
to_compare = []
stdz = [[0]*len(y1)]*4
print stdz
print groups

fig , ax = plt.subplots(1,1)
lgd = plot_bars(ax,groups,labels,title,legend,to_compare,stdz,show_legend=True)

plt.show()
