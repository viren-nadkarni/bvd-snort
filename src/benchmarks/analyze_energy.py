import sys
import matplotlib.pyplot as plt
from getters import get_versions,get_datasets,get_patterns
from settings import *
from plotters import plot_bars
import subprocess



versions  = ['3_energy','0_energy','1_energy','2_energy']

Data = {}

for name in versions: 
    with open('/home/odroid/snort_GPU_system_logs/'+name,'rb') as log:
        c=0
        a15 = []
        a7 = []
        gpu = []
        mem = []
        Data[name]={}
        for row in log:
            c+=1
            if c==1:
                continue
            x = row.split(',')
            if len(x)!=4:
                continue
            a15.append(float(row.split(',')[0]))
            a7.append(float(row.split(',')[1]))
            gpu.append(float(row.split(',')[2]))
            mem.append(float(row.split(',')[3]))
            
            Data[name]["a15"] = a15
            Data[name]["a7"]  = a7 
            Data[name]["gpu"]  = gpu
            Data[name]["mem"]= mem

legend = ['Snort original', 'Snort modified (CPU)','CLort single buffer (GPU)','CLort double buffer (GPU)']
for name in versions:
    plt.plot(Data[name]["a15"])
plt.legend(legend)
plt.title('a15')
plt.show()

for name in versions:
    plt.plot(Data[name]["a7"])
plt.legend(legend)
plt.title('a7')
plt.show()

for name in versions:
    plt.plot(Data[name]["gpu"])
plt.legend(legend)
plt.title('gpu')
plt.show()

for name in versions:
    plt.plot(Data[name]["mem"])
plt.legend(legend)
plt.title('memory')
plt.show()

