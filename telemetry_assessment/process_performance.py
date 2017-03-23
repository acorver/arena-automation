
import os, multiprocessing
from datetime import datetime
import pandas as pd
import numpy as np
import plot_performance
import tkinter as tk
from tkinter import filedialog

os.chdir(os.path.dirname(os.path.abspath(__file__)))

DEBUG = True

def getFilenames():
    # Global settings
    root = tk.Tk()
    root.withdraw()

    def askfile(t):
        fname = filedialog.askopenfilename(title=t)
        if fname == "" or fname == None:
            print("The filenames is invalid. Restart the program to try again.")
            print("Filename: "+fname)
            root.destroy()
        else:
            return fname

    filename1 = askfile("Select the position  file for transceiver device 1 (e.g. New Device)")
    filename2 = askfile("Select the telemetry file for transceiver device 1 (e.g. New Device)")
    filename3 = askfile("Select the position  file for transceiver device 2 (e.g. Old Device)")
    filename4 = askfile("Select the telemetry file for transceiver device 2 (e.g. Old Device)")

    # Common names are:
    #  new_transceiver_rigid_backpack.arduino.position.log
    #  new_transceiver_rigid_backpack.telemetry.log
    #  old_transceiver_rigid_backpack.arduino.position.log
    #  old_transceiver_rigid_backpack.telemetry.log

    fnames = [(filename1, filename2), (filename3, filename4)]

    return fnames

def processFile(fname):

    outfile = fname[1].replace('.log','') + '.csv'

    if os.path.exists(outfile):
        print("Skipping file because it already exists: " + outfile)
        return
    
    # Error-robust datetime parser: robust to files with invalid lines (e.g. cutoff after program exit)
    def _strptime(x,f = '%Y-%m-%d %H:%M:%S.%f'):
        try:
            return datetime.strptime(x,f)
        except:
            return ''

    # Create position ranges
    pos = pd.DataFrame([], columns=['t1','t2','x','z1','z2'])
    with open(fname[0], 'r') as f: 
        lpos = [y.split(',') for y in [x for x in f.read().split('\n') if x!='' and 'pos' in x]]
        lpos = [[_strptime(x[0]),]+x[1:] for x in lpos if _strptime(x[0])!='']
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

    print("Found "+str(pos.shape[0])+" position ranges.")

    # Read telemetry data
    tel = []
    with open(fname[1], 'r') as f:
        tel = [y.split(',') for y in [x for x in f.read().split('\n') if x!='']]
    tel = [[_strptime(x[0]),]+x[1:] for x in tel if _strptime(x[0])!='']
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
            print(str(it) + ", #valid measurements="+str(data.shape[0])) 
            
    # Are there positions that were explored that are not within the range of error?
    ps = [str(x[0])+str(x[1]) for x in zip(pos.x.tolist(),pos.z1.tolist())]
    for ip, p in pos.iterrows():
        if not (str(p.x)+str(p.z1)) in ps:
            data.loc[data.shape[0]] = [None,] + p[['t1','t2','x','z1','z2']].values.tolist()[0] + [float('NaN') for i in range(4)]
    
    data.to_csv(outfile)

if __name__ == "__main__":

    fnames = getFilenames()

    for fname in fnames:
        # Change the working directory (i.e. output directory) to the directory of the first file 
        # (presumably all four files are in the same directory)
        outdir = os.path.dirname(fname[0])
        os.chdir(outdir)

        processFile(fname)
    
    # Now plot!
    plot_performance.run(fnames)
