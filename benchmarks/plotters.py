from settings import *
import numpy as np

def plot_bars(ax,groups,labels,title,legend,to_compare,stdz,show_legend=False):

        N = len(labels)
        ind = np.arange(N)  # the x locations for the groups
        width = 0.165       # the width of the bars

        #br = 11

        #labels = [str(x/1000)+"K" for x in labels]

        baseline = groups[1]

        # add some text for labels, title and axes ticks
        ax.set_ylabel('Throughput (Mbps)')
        ax.set_xlabel(title)
        #ax.set_title(title)
        #ax.set_xticks(ind + width*len(labels))
        ax.set_xticks(ind + 2*width)
        #ax.set_xticklabels(datasets,rotation='45')
        ax.set_xticklabels(labels,rotation='0')
        rects_set=[]
        lgd=[]
        c=0
        for i,group in enumerate(groups):
                series = group
                stand_dev = stdz[i]
                rects = ax.bar(ind, series, 0.8*width , color=colors[c],hatch=hatches[c],yerr=stand_dev, ecolor='black',label=[legend[c]])
                v = [x.get_height() for x in rects]
                if (c in to_compare):
                        vals = np.divide(baseline,v)
                        #if switch_tp:
                        vals = np.divide(1,vals)
                        autolabel(ax,rects,vals,title,legend,c,labels)
                rects_set.append(rects)
                ind = [x+width for x in ind]
                c+=1
        if show_legend:
            #ax.legend((rects_set),legend,loc=2,ncol=2)
            #ax.legend((rects_set),legend, loc=4,ncol=2)
            #lgd = ax.legend((rects_set),legend,bbox_to_anchor=(0.,1.1,1.01,0.102),loc=2,ncol=2, mode="expand", borderaxespad=0.1,markerscale=12)
            lgd = ax.legend((rects_set),legend,bbox_to_anchor=(0.,1.2,1.0,0.102),loc=2,ncol=2, mode="expand", borderaxespad=0.1,markerscale=12)
        
        ax.plot()
        return lgd

def autolabel(ax,rects,vals,title,legend,c,labels):
    # attach some text labels
    f=0
    for rect,val in zip(rects,vals):
        height = rect.get_height()
        if (title=="Snort web traffic patterns(2K)") and (legend[c]=="DFC") and (f==0)and (len(labels)!=1):
                ax.text(rect.get_x() + rect.get_width()/2., 0.3+height,
                '%0.2f' % (val),
                        ha='center', va='bottom')
        else:
                ax.text(rect.get_x() + rect.get_width()/2., 0.1+height,
                '%0.2f' % (val),
                        ha='center', va='bottom')

        f+=1

