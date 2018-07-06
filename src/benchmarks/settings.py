versions = ['3','0','1','2']
names = ['Snort original',\
         'Snort modified (CPU)',\
         'CLort single buffer (GPU)',\
         'CLort double buffer (GPU)',\
        ]
datasets = ['smallFlows.pcap',\
            'bigFlows.pcap',\
            'testbed-12jun_1.pcap',\
            'testbed-12jun_2.pcap',\
            'testbed-12jun_3.pcap',\
            'testbed-12jun_4.pcap',\
            'testbed-13jun_1.pcap',\
            ]
dataset_names = ['smallFlows',\
                'bigFlows',\
                'ISCX12 131',\
                'ISCX12 131-2',\
                'ISCX12 131-3',\
                'ISCX12 131-4',\
                'ISCX12 121',\
                ]
dataset_sizes = {'smallFlows.pcap': 9444731,\
                 'bigFlows.pcap': 368083648,\
                 'testbed-12jun_1.pcap': 1087244065,\
                 'testbed-12jun_2.pcap': 1180336181,\
                 'testbed-12jun_3.pcap': 1178561403,\
                 'testbed-12jun_4.pcap': 1085944370,\
                 'testbed-13jun_1.pcap': 665009856}
patterns = ['snort3-community.rules']

colors = ['lightsalmon','skyblue','steelblue','mediumseagreen','lightgreen','0.30','0.70','m']
hatches = ['/','\\\\','x','\\','//']
