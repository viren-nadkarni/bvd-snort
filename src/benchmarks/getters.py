from settings import *

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


