versions = ['3','0','1','2']
names = ['Snort original',\
         'Snort modified (CPU)',\
         'CLort single buffer (GPU)',\
         'CLort double buffer (GPU)',\
        ]
datasets = [\
            'smallFlows.pcap',\
            'bigFlows.pcap',\
            'testbed-13jun_1.pcap',\
            'testbed-12jun_1.pcap',\
            'testbed-12jun_2.pcap',\
            'testbed-12jun_3.pcap',\
            'testbed-12jun_4.pcap',\
            'testbed-12-full.pcap',\
            ]
dataset_names = [\
                'smallFlows',\
                'bigFlows',\
                'ISCX12 121',\
                'ISCX12 131',\
                'ISCX12 131-2',\
                'ISCX12 131-3',\
                'ISCX12 131-4',\
                'ISCX12 131-full',\
                ]
dataset_sizes = {\
                 'smallFlows.pcap': 9444731,\
                 'bigFlows.pcap': 368083648,\
                 'testbed-13jun_1.pcap': 665009856,\
                 'testbed-12jun_1.pcap': 1087244065,\
                 'testbed-12jun_2.pcap': 1180336181,\
                 'testbed-12jun_3.pcap': 1178561403,\
                 'testbed-12jun_4.pcap': 1085944370,\
                 'testbed-12-full.pcap': 4532086019,\
                 }
patterns = [\
            #'snort3-community.rules',\
            'snort3-community_default_all_content.rules',\
            'snort3-community_medium_all_content.rules',\
            'snort3-community_full_all_content.rules',\
            ]

colors = ['lightsalmon','skyblue','steelblue','mediumseagreen','lightgreen','0.30','0.70','m']
hatches = ['/','\\\\','x','\\','//']

def delete_chunks():
    del dataset_names[dataset_names.index('ISCX12 131-2')]
    del dataset_names[dataset_names.index('ISCX12 131-3')]
    del dataset_names[dataset_names.index('ISCX12 131-4')]

    del datasets[datasets.index('testbed-12jun_2.pcap')]
    del datasets[datasets.index('testbed-12jun_3.pcap')]
    del datasets[datasets.index('testbed-12jun_4.pcap')]

    del dataset_sizes['testbed-12jun_2.pcap']
    del dataset_sizes['testbed-12jun_3.pcap']
    del dataset_sizes['testbed-12jun_4.pcap']
