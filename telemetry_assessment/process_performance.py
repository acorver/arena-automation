
import os, multiprocessing
from datetime import datetime
import pandas as pd
import numpy as np
import plot_performance

os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'data/'))

DEBUG = False

# Global settings
fnames = [
    ('new_transceiver_rigid_backpack.arduino.position.log',
     'new_transceiver_rigid_backpack.telemetry.log'), 
    ('old_transceiver_rigid_backpack.arduino.position.log',
     'old_transceiver_rigid_backpack.telemetry.log')
]

def processFile(fname):
    # Create position ranges
    pos = pd.DataFrame([], columns=['t1','t2','x','z1','z2'])
    with open(fname[0], 'r') as f: 
        lpos = [y.split(',') for y in [x for x in f.read().split('\n') if x!='' and 'pos' in x]]
        lpos = [[datetime.strptime(x[0],'%Y-%m-%d %H:%M:%S.%f'),]+x[1:] for x in lpos]
        lpos = [[x[0],x[1]] + [int(y) for y in x[2:5]] for x in lpos]
        t1, t2 = None, None
        for p in lpos:
            if t1 == None:
                t1 = p
            elif np.all([abs(p[i]-t1[i]) < 5 for i in range(2,5)]):
                t2 = p
            else:
                # Save 
                if t2 != None:
                    pos.loc[pos.shape[0]] = [t1[0], t2[0],] + t1[2:]
                    print([pos.shape[0],]+p[2:5])
                # pos
                t1, t2 = p, None

    # Read telemetry data
    tel = []
    with open(fname[1], 'r') as f:
        tel = [y.split(',') for y in [x for x in f.read().split('\n') if x!='']]
    tel = [[datetime.strptime(x[0],'%Y-%m-%d %H:%M:%S.%f'),]+x[1:] for x in tel]
    tel = [[y if not isinstance(y,str) else y.replace('Infinity','inf') for y in x] for x in tel]    
    
    # Go through each telemetry measurement, find a position point that happened within .5 seconds, if so, add to table
    data = pd.DataFrame([], columns=['time','time_pos','time_pos_end','x','z1','z2','logErrorRate','PowerV','MissingFrames','FalseFrames'])

    # Go through each telemetry measurement, find a position point that happened within .5 seconds, if so, add to table
    for it, t in zip(range(len(tel)),tel):
    
        # Get time
        time = t[0]
    
        # Find position at this measurement
        a = pos.ix[(pos.t1 < time) & (pos.t2 > time), ['t1','t2','x','z1','z2']]
    
        if a.shape[0] > 0:
            data.loc[data.shape[0]] = [time,] + a.values.tolist()[0] + t[1:5]
    
        if (it%1000)==0: 
            print(it) 
            
    # Are there positions that were explored that are not within the range of error?
    ps = [str(x[0])+str(x[1]) for x in zip(pos.x.tolist(),pos.z1.tolist())]
    for ip, p in pos.iterrows():
        if not (str(p.x)+str(p.z1)) in ps:
            data.loc[data.shape[0]] = [None,] + p[['t1','t2','x','z1','z2']].values.tolist()[0] + [float('NaN') for i in range(4)]
    
    data.to_csv(fname[1].replace('.log','.csv'))

if __name__ == "__main__":
    if DEBUG:
        for fname in fnames:
            processFile(fname)
    else:
        with multiprocessing.Pool(4) as pool:
            pool.map(processFile, fnames)
    
    # Now plot!
    plot_performance.run()
