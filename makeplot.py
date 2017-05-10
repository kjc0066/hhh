#!/usr/local/bin/ipython -pylab
f = open('data.txt.summary', 'rU').read().strip().split('\n')
g = [[tuple(s.split('=')) for s in r.split()] for r in f]
import tabular as tb
x = tb.tabarray(kvpairs = g)
x.saveSV('anything.tsv')
y = tb.tabarray(SVfile='anything.tsv')

#directory to save images in
directory='./'
#extension/file type (eps/png/pdf/ps/svg)
extension='.pdf'

#Make the graphs for all parameters
for params in ([10,5], [100,50], [1000,500], [1000,50], [1000,200]):
	epsi=params[0]
	phii=params[1]
	#Algorithm comparison graphs
	for value in ('time', 'epsilon'):
		for algtype in ('1', '1_33', '2'):
			pylab.figure()
			algnames = ['./hhh', './uhhh', './ancestry', './full']
			markers = ['+','x','*','d']
			for i in range(0,4):
				ind = (y['algorithm']==algnames[i]+algtype) & (y['counters']==epsi) & (y['threshold']==y['nitems']/phii)
				ind2 = y['nitems'][ind].argsort()
				marker=markers[i]
				if (value=='epsilon'):
					pylab.plot(y['nitems'][ind][ind2]/1000000, y[value][ind][ind2]*epsi, marker=marker)
				else:
					pylab.plot(y['nitems'][ind][ind2]/1000000, y[value][ind][ind2], marker=marker)
			pylab.xlabel('$N$ (millions)', fontsize=17)
			pylab.xticks(fontsize=17)
			pylab.yticks(fontsize=17)
			if (value=='time'):
				pylab.ylabel('CPU Time (secs)', fontsize=17)
			if (value=='epsilon'):
				pylab.ylabel('Relative Error', fontsize=17)
				pylab.semilogx()
			if (algtype=='1'):
				pylab.title('Byte-granularity in one dimension with $\\varepsilon='+str(1.0/epsi)+'$ and $\phi='+str(1.0/phii)+'$', fontsize=18)
			if (algtype=='1_33'):
				pylab.title('Bit-granularity in one dimension with $\\varepsilon='+str(1.0/epsi)+'$ and $\phi='+str(1.0/phii)+'$', fontsize=18)
			if (algtype=='2'):
				pylab.title('Byte-granularity in two dimensions with $\\varepsilon='+str(1.0/epsi)+'$ and $\phi='+str(1.0/phii)+'$', fontsize=18)
			pylab.legend(['hhh', 'unitary', 'partial','full'])
			pylab.savefig(directory+value+'_'+str(epsi)+'-'+str(phii)+'_'+algtype+extension)
			pylab.close()
	#Bar charts
	for value in ('memory', 'outputsize'):
		for algtype in ('1', '1_33', '2'):
			pylab.figure()
			if (value=='memory'):
				iran = range(0,4)
				val = range(0,4)
				errb = range(0,4)
				algnames=['./hhh', './uhhh', './ancestry', './full']
				pos = [0.5,1.5,2.5,3.5]    # the bar centers on the y axis
				labes = ('hhh', 'unitary', 'partial', 'full')
				divisor=2000.0
			else:
				iran = range(0,3)
				val = range(0,3)
				errb = range(0,3)
				algnames=['./hhh','./ancestry', './full']
				pos = [0.5,1.5,2.5]    # the bar centers on the y axis
				labes = ('hhh', 'partial', 'full')
				divisor=2.0
			for i in iran:
				ind = (y['algorithm']==algnames[i]+algtype) & (y['counters']==epsi) & (y['threshold']==y['nitems']/phii)
				val[i] = (max(y[value][ind])+min(y[value][ind]))/divisor
				errb[i]= (max(y[value][ind])-min(y[value][ind]))/divisor
			pylab.bar(pos,val, yerr=errb, ecolor='r', align='center')
			pylab.xticks(pos, labes, fontsize=17)
			pylab.yticks(fontsize=17)
			if (value=='memory'):
				pylab.ylabel('Memory (KBytes)', fontsize=17)
			if (value=='outputsize'):
				pylab.ylabel('Output Size', fontsize=17)
			if (algtype=='1'):
				pylab.title('Byte-granularity in one dimension with $\\varepsilon='+str(1.0/epsi)+'$ and $\phi='+str(1.0/phii)+'$', fontsize=18)
			if (algtype=='1_33'):
				pylab.title('Bit-granularity in one dimension with $\\varepsilon='+str(1.0/epsi)+'$ and $\phi='+str(1.0/phii)+'$', fontsize=18)
			if (algtype=='2'):
				pylab.title('Byte-granularity in two dimensions with $\\varepsilon='+str(1.0/epsi)+'$ and $\phi='+str(1.0/phii)+'$', fontsize=18)
			pylab.savefig(directory+value+'_'+str(epsi)+'-'+str(phii)+'_'+algtype+extension)
			pylab.close()	
	#Parallel graphs
	for value in ('time', 'walltime', 'scaled'):
		for algtype in ('1', '1_33', '2'):
			pylab.figure()
			markers = ['+','*','d','x','o','1']
			for threads in range(1,5):
				ind = (y['algorithm']=='./omp'+algtype) & (y['counters']==epsi) & (y['threshold']==y['nitems']/phii) & (y['threads']==threads)
				ind2 = y['nitems'][ind].argsort()
				if (value=='scaled'):
					pylab.plot(y['nitems'][ind][ind2]/1000000, threads*y['walltime'][ind][ind2], marker=markers[i-1])
				else:
					pylab.plot(y['nitems'][ind][ind2]/1000000, y[value][ind][ind2], marker=markers[i-1])

			pylab.xlabel('$N$ (millions)', fontsize=17)
			pylab.xticks(fontsize=17)
			pylab.yticks(fontsize=17)
			if (value=='time'):
				pylab.ylabel('Total CPU Time (secs)', fontsize=17)
			if (value=='walltime'):
				pylab.ylabel('Wall Time (secs)', fontsize=17)
			if (value=='scaled'):
				pylab.ylabel('Wall Time (secs) $\\times$ Threads', fontsize=17)
			if (algtype=='2'):
				pylab.title('Parallel byte-granularity in two dimensions with $\\varepsilon='+str(1.0/epsi)+'$ and $\phi='+str(1.0/phii)+'$', fontsize=18)
			if (algtype=='1_33'):
				pylab.title('Parallel bit-granularity in one dimension with $\\varepsilon='+str(1.0/epsi)+'$ and $\phi='+str(1.0/phii)+'$', fontsize=18)
			if (algtype=='1'):
				pylab.title('Parallel byte-granularity in one dimension with $\\varepsilon='+str(1.0/epsi)+'$ and $\phi='+str(1.0/phii)+'$', fontsize=18)
			pylab.legend(['1 thread','2 threads','3 threads','4 threads'])
			pylab.savefig(directory+'parallel'+value+'_'+str(epsi)+'-'+str(phii)+'_'+algtype+extension)
			pylab.close()
#Those ugly extra nonuniform graphs
epsi=1000
phii=200
#Algorithm comparison graphs
for value in ('time', 'epsilon'):
	for algtype in ('1', '1_33', '2'):
		pylab.figure()
		if (value=='epsilon'):
			multiplier=epsi
		else:
			multiplier=1
		algnames = ['./hhh', './ancestry', './full']
		markers = ['+','*','d','x','o','1']
		for i in range(0,3):
			ind = (y['algorithm']==algnames[i]+algtype) & (y['counters']==epsi) & (y['threshold']==y['nitems']/phii)
			ind2 = y['nitems'][ind].argsort()
			pylab.plot(y['nitems'][ind][ind2]/1000000, y[value][ind][ind2]*multiplier, marker=markers[i])
		for epsnu in (750, 500, 250):
			ind = (y['algorithm']=='./hhh'+algtype) & (y['counters']==epsnu) & (y['threshold']==y['nitems']/phii)
			ind2 = y['nitems'][ind].argsort()
			pylab.plot(y['nitems'][ind][ind2]/1000000, y[value][ind][ind2]*multiplier, marker=markers[6-epsnu/250])
		pylab.xlabel('$N$ (millions)', fontsize=17)
		pylab.xticks(fontsize=17)
		pylab.yticks(fontsize=17)
		if (value=='time'):
			pylab.ylabel('CPU Time (secs)', fontsize=17)
		if (value=='epsilon'):
			pylab.ylabel('Relative Error', fontsize=17)
			pylab.semilogx()
		if (algtype=='1'):
			pylab.title('Byte-granularity in one dimension with $\\varepsilon='+str(1.0/epsi)+'$ and $\phi='+str(1.0/phii)+'$', fontsize=18)
		if (algtype=='1_33'):
			pylab.title('Bit-granularity in one dimension with $\\varepsilon='+str(1.0/epsi)+'$ and $\phi='+str(1.0/phii)+'$', fontsize=18)
		if (algtype=='2'):
			pylab.title('Byte-granularity in two dimensions with $\\varepsilon='+str(1.0/epsi)+'$ and $\phi='+str(1.0/phii)+'$', fontsize=18)
		pylab.legend(['hhh','partial','full', 'hhh.0013', 'hhh.002', 'hhh.004'])
		pylab.savefig(directory+'nu'+value+'_'+str(epsi)+'-'+str(phii)+'_'+algtype+extension)
		pylab.close()
#Bar charts
for value in ('memory', 'outputsize'):
	for algtype in ('1', '1_33', '2'):
		pylab.figure()
		val = range(0,6)
		errb = range(0,6)
		algnames=['./hhh', './ancestry', './full', './hhh', './hhh', './hhh']
		epsnu=[1000,1000,1000,750,500,250]
		if (value=='memory'):
			divisor=2000.0
		else:
			divisor=2.0
		for i in range(0,6):
			ind = (y['algorithm']==algnames[i]+algtype) & (y['counters']==epsnu[i]) & (y['threshold']==y['nitems']/phii)
			val[i] = (max(y[value][ind])+min(y[value][ind]))/divisor
			errb[i]= (max(y[value][ind])-min(y[value][ind]))/divisor
		pos = [0.5,1.5,2.5,3.5,4.5,5.5]    # the bar centers on the y axis
		pylab.bar(pos,val, yerr=errb, ecolor='r', align='center')
		pylab.xticks(pos, ('hhh', 'partial', 'full', 'hhh.0013', 'hhh.002', 'hhh.004'))
		if (value=='memory'):
			pylab.ylabel('Memory (KBytes)', fontsize=17)
		if (value=='outputsize'):
			pylab.ylabel('Output Size', fontsize=17)
		if (algtype=='1'):
			pylab.title('Byte-granularity in one dimension with $\\varepsilon='+str(1.0/epsi)+'$ and $\phi='+str(1.0/phii)+'$', fontsize=18)
		if (algtype=='1_33'):
			pylab.title('Bit-granularity in one dimension with $\\varepsilon='+str(1.0/epsi)+'$ and $\phi='+str(1.0/phii)+'$', fontsize=18)
		if (algtype=='2'):
			pylab.title('Byte-granularity in two dimensions with $\\varepsilon='+str(1.0/epsi)+'$ and $\phi='+str(1.0/phii)+'$', fontsize=18)
		pylab.savefig(directory+'nu'+value+'_'+str(epsi)+'-'+str(phii)+'_'+algtype+extension)
		pylab.close()
#Now the graphs with eps on the horizontal axis
for value in ('time', 'memory'):
	for algtype in ('1', '1_33', '2'):
		pylab.figure()
		if (value=='memory'):
			divisor=1000
		else:
			divisor=1
		algnames = ['./hhh', './uhhh', './ancestry', './full']
		markers = ['+','x','*','d']
		for i in range(0,4):
			ind = (y['algorithm']==algnames[i]+algtype) & (y['nitems']==30000000) & (y['counters']<=100000)
			ind2 = y['counters'][ind].argsort()
			pylab.plot(y['counters'][ind][ind2], y[value][ind][ind2]/divisor, marker=markers[i])
		pylab.xticks(fontsize=17)
		pylab.yticks(fontsize=17)
		pylab.xlabel('$1/\\varepsilon$', fontsize=17)
		pylab.semilogx()
		if (value=='time'):
			pylab.ylabel('CPU Time (secs)', fontsize=17)
		if (value=='memory'):
			pylab.ylabel('Memory (KBytes)', fontsize=17)
			pylab.loglog()
		if (algtype=='1'):
			pylab.title('Byte-granularity in one dimension with $N=30$ million', fontsize=18)
		if (algtype=='1_33'):
			pylab.title('Bit-granularity in one dimension with $N=30$ million', fontsize=18)
		if (algtype=='2'):
			pylab.title('Byte-granularity in two dimensions with $N=30$ million', fontsize=18)
		pylab.legend(['hhh','unitary', 'partial','full'])
		pylab.savefig(directory+'veps'+value+'_'+algtype+extension)
		pylab.close()
quit()



