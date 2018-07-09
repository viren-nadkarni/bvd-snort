import sys
import matplotlib.pyplot as plt
from getters import get_versions,get_datasets,get_patterns
from settings import *
from plotters import plot_bars
import subprocess

colors = ['lightsalmon','skyblue','steelblue','mediumseagreen','lightgreen','0.30','0.70','m']
hatches = ['/','\\\\','x','\\','//']

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
FIG_SIZE=(10,5)

#CPU -big (15)
fig , ax = plt.subplots(1,1,figsize=FIG_SIZE)
for i,name in enumerate(versions):
    ax.plot(Data[name]["a15"],color=colors[i])
lgd = ax.legend(legend,bbox_to_anchor=(0.,1.1,1.0,0.102),loc=2,ncol=2, mode="expand", borderaxespad=0.1,markerscale=12)
ax.set_xlabel("Time (ms)")
ax.set_ylabel("Power Consumption (Watt)")
ax.set_ylim([0.5,3.5])

name="/home/odroid/snort_GPU_system_logs/plots/energy_CPU.pdf"
plt.savefig(name,bbox_extra_artists=(lgd,), bbox_inches = "tight")
subprocess.Popen("pdfcrop "+name+" "+name,shell=True)
subprocess.Popen("pdfcrop")
plt.show()

#CPU-LITTLE (a7)
fig , ax = plt.subplots(1,1,figsize=FIG_SIZE)
for i,name in enumerate(versions):
    ax.plot(Data[name]["a7"],color=colors[i])
lgd = ax.legend(legend,bbox_to_anchor=(0.,1.1,1.0,0.102),loc=2,ncol=2, mode="expand", borderaxespad=0.1,markerscale=12)
ax.set_xlabel("Time (ms)")
ax.set_ylabel("Power Consumption (Watt)")

name="/home/odroid/snort_GPU_system_logs/plots/energy_CPU_little.pdf"
plt.savefig(name,bbox_extra_artists=(lgd,), bbox_inches = "tight")
subprocess.Popen("pdfcrop "+name+" "+name,shell=True)
subprocess.Popen("pdfcrop")
plt.show()



#GPU
fig , ax = plt.subplots(1,1,figsize=FIG_SIZE)
for i,name in enumerate(versions):
    ax.plot(Data[name]["gpu"],color=colors[i])
lgd = ax.legend(legend,bbox_to_anchor=(0.,1.1,1.0,0.102),loc=2,ncol=2, mode="expand", borderaxespad=0.1,markerscale=12)
ax.set_xlabel("Time (ms)")
ax.set_ylabel("Power Consumption (Watt)")
ax.set_ylim([0.1,0.6])

name="/home/odroid/snort_GPU_system_logs/plots/energy_GPU.pdf"
plt.savefig(name,bbox_extra_artists=(lgd,), bbox_inches = "tight")
subprocess.Popen("pdfcrop "+name+" "+name,shell=True)
subprocess.Popen("pdfcrop")
plt.show()

#Memmory
fig , ax = plt.subplots(1,1,figsize=FIG_SIZE)
for i,name in enumerate(versions):
    ax.plot(Data[name]["mem"],color=colors[i])
lgd = ax.legend(legend,bbox_to_anchor=(0.,1.1,1.0,0.102),loc=2,ncol=2, mode="expand", borderaxespad=0.1,markerscale=12)
ax.set_xlabel("Time (ms)")
ax.set_ylabel("Power Consumption (Watt)")
ax.set_ylim([0.02,0.08])

name="/home/odroid/snort_GPU_system_logs/plots/energy_Memmory.pdf"
plt.savefig(name,bbox_extra_artists=(lgd,), bbox_inches = "tight")
subprocess.Popen("pdfcrop "+name+" "+name,shell=True)
subprocess.Popen("pdfcrop")
plt.show()
