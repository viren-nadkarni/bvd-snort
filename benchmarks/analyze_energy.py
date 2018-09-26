import sys
import matplotlib.pyplot as plt
from getters import get_versions,get_datasets,get_patterns
from settings import *
from plotters import plot_bars
import subprocess
import numpy as np

#colors = ['lightsalmon','skyblue','steelblue','mediumseagreen','lightgreen','0.30','0.70','m']
colors = ['coral','skyblue','steelblue','mediumseagreen','forestgreen','0.30','0.70','m']
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
            
            Data[name]["a15"] = a15[100:]
            Data[name]["a7"]  = a7[100:]
            Data[name]["gpu"]  = gpu[100:]
            Data[name]["mem"]= mem[100:]

legend = ['Snort original', 'Snort modified (CPU)','CLort single buffer (GPU)','CLort double buffer (GPU)']
FIG_SIZE=(8,4)

time = range(0,len(Data[name]["a15"]),10)

#CPU -big (15)
fig , ax = plt.subplots(1,1,figsize=FIG_SIZE)
for i,name in enumerate(versions):
    time = np.linspace(0,len(Data[name]["a15"])/100.0,len(Data[name]["a15"]))
    ax.plot(time,Data[name]["a15"],color=colors[i])
lgd = ax.legend(legend,bbox_to_anchor=(0.,1.15,1.0,0.102),loc=2,ncol=2, mode="expand", borderaxespad=0.1,markerscale=12)
ax.set_xlabel("Time (s)")
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
    time = np.linspace(0,len(Data[name]["a7"])/100.0,len(Data[name]["a7"]))
    ax.plot(time,Data[name]["a7"],color=colors[i])
lgd = ax.legend(legend,bbox_to_anchor=(0.,1.15,1.0,0.102),loc=2,ncol=2, mode="expand", borderaxespad=0.1,markerscale=12)
ax.set_xlabel("Time (s)")
ax.set_ylabel("Power Consumption (Watt)")

name="/home/odroid/snort_GPU_system_logs/plots/energy_CPU_little.pdf"
plt.savefig(name,bbox_extra_artists=(lgd,), bbox_inches = "tight")
subprocess.Popen("pdfcrop "+name+" "+name,shell=True)
subprocess.Popen("pdfcrop")
plt.show()



#GPU
fig , ax = plt.subplots(1,1,figsize=FIG_SIZE)
for i,name in enumerate(versions):
    time = np.linspace(0,len(Data[name]["gpu"])/100.0,len(Data[name]["gpu"]))
    ax.plot(time,Data[name]["gpu"],color=colors[i])
lgd = ax.legend(legend,bbox_to_anchor=(0.,1.15,1.0,0.102),loc=2,ncol=2, mode="expand", borderaxespad=0.1,markerscale=12)
ax.set_xlabel("Time (s)")
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
    time = np.linspace(0,len(Data[name]["mem"])/100.0,len(Data[name]["mem"]))
    ax.plot(time,Data[name]["mem"],color=colors[i])
lgd = ax.legend(legend,bbox_to_anchor=(0.,1.15,1.0,0.102),loc=2,ncol=2, mode="expand", borderaxespad=0.1,markerscale=12)
ax.set_xlabel("Time (s)")
ax.set_ylabel("Power Consumption (Watt)")
ax.set_ylim([0.02,0.08])

name="/home/odroid/snort_GPU_system_logs/plots/energy_Memmory.pdf"
plt.savefig(name,bbox_extra_artists=(lgd,), bbox_inches = "tight")
subprocess.Popen("pdfcrop "+name+" "+name,shell=True)
subprocess.Popen("pdfcrop")
plt.show()

for name in versions:
    Data[name]["total"] = []
    for i in range(len(Data[name]["gpu"])):
        Data[name]["total"].append( Data[name]["gpu"][i]+Data[name]['a15'][i]+Data[name]['a7'][i]+Data[name]['mem'][i] )



#Total
fig , ax = plt.subplots(1,1,figsize=FIG_SIZE)
for i,name in enumerate(versions):
    time = np.linspace(0,len(Data[name]["total"])/100.0,len(Data[name]["total"]))
    ax.plot(time,Data[name]["total"],color=colors[i])
lgd = ax.legend(legend,bbox_to_anchor=(0.,1.15,1.0,0.102),loc=2,ncol=2, mode="expand", borderaxespad=0.1,markerscale=12)
ax.set_xlabel("Time (s)")
ax.set_ylabel("Power Consumption (Watt)")
ax.set_ylim([0.5,3.5])

name="/home/odroid/snort_GPU_system_logs/plots/energy_Total.pdf"
plt.savefig(name,bbox_extra_artists=(lgd,), bbox_inches = "tight")
subprocess.Popen("pdfcrop "+name+" "+name,shell=True)
subprocess.Popen("pdfcrop")
plt.show()

print "AVGS power (Watt)"
for name in versions:
    s = 0
    min_t = 100000
    max_t = 0
    for i in range(len(Data[name]['total'])):
        if Data[name]['total'][i]>2.0:
            s += Data[name]['total'][i]
            if (i>max_t):
                max_t = i
            if (i<min_t):
                min_t = i
        #print sum(Data[name]['total'])/float(len(Data[name]['total']))
    print float(s)/(max_t - min_t)

print "Energy (Joule)"
for name in versions:
    s = 0
    min_t = 100000
    max_t = 0
    for i in range(len(Data[name]['total'])):
        if Data[name]['total'][i]>2.0:
            s += Data[name]['total'][i]
            if (i>max_t):
                max_t = i
            if (i<min_t):
                min_t = i
        #print sum(Data[name]['total'])/float(len(Data[name]['total']))
    print float(s)/100.0 #because max_t, min_t is sec/100

