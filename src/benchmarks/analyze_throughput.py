import sys
import matplotlib.pyplot as plt

log_file = sys.argv[1]
versions = ['0','1','2']
datasets = ['smallFlows.pcap',\
            'bigFlows.pcap',\
            'testbed-12jun_1.pcap',\
            'testbed-12jun_2.pcap',\
            'testbed-12jun_3.pcap',\
            'testbed-12jun_4.pcap',\
            'testbed-13jun_1.pcap',\
            ]
dataset_sizes = {'smallFlows.pcap': 9444731,\
                 'bigFlows.pcap': 368083648,\
                 'testbed-12jun_1.pcap': 1087244065,\
                 'testbed-12jun_2.pcap': 1180336181,\
                 'testbed-12jun_3.pcap': 1178561403,\
                 'testbed-12jun_4.pcap': 1085944370,\
                 'testbed-13jun_1.pcap': 665009856}
patterns = ['snort3-community.rules']

Data = {}

def get_versions(D,pat,data):
    l=[]
    i=[]
    for x in versions:
        i.append(x)
        l.append(D[(x,pat,data)])
    return (i,l)
        
def get_patterns(D,ver,data):
    l=[]
    i=[]
    for x in patterns:
        i.append(x)
        l.append(D[(ver,x,data)])
    return (i,l)

def get_datasets(D,ver,pat):
    l=[]
    i=[]
    for x in datasets:
        i.append(x)
        l.append(D[(ver,pat,x)])
    return (i,l)


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

print Data
print "---------"

(x1,y1) = get_versions(Data,patterns[0],datasets[0])
print (x1,y1)
(x2,y2) = get_versions(Data,patterns[0],datasets[1])
print (x2,y2)

plt.plot(x1,y1)
plt.plot(x2,y2)
plt.show()

(x1,y1) =  get_datasets(Data,versions[0],patterns[0])
(x2,y2) =  get_datasets(Data,versions[1],patterns[0])
(x3,y3) =  get_datasets(Data,versions[2],patterns[0])
plt.plot(range(len(x1)),y1,label="CPU")
plt.plot(range(len(x2)),y2,label="GPU single")
plt.plot(range(len(x3)),y3,label="GPU double")
plt.legend()
plt.show()

