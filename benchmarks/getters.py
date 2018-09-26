from settings import *

def get_versions(D,pat,data):
    l=[]
    i=[]
    for x in versions:
        if D.has_key((x,pat,data)):
            i.append(x)
            l.append(D[(x,pat,data)])
    return (i,l)

def get_patterns(D,ver,data):
    l=[]
    i=[]
    for x in patterns:
        if D.has_key((ver,x,data)):
            i.append(x)
            l.append(D[(ver,x,data)])
    return (i,l)

def get_datasets(D,ver,pat):
    l=[]
    i=[]
    for x in datasets:
        if D.has_key((ver,pat,x)):
            i.append(x)
            l.append(D[(ver,pat,x)])
    return (i,l)


