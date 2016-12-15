
import msgpack
import numpy as np
import math, os, multiprocessing
from datetime import datetime

#
# TODO: Don't save positions that are all NaN
#

# Set working directory
os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)),'../data'))

# Set "overwrite" to True to overwrite existing files
OVERWRITE = False

SINGLE_FILE = "2016-11-11 12-20-41_Cortex.msgpack"

# Misc. constants
CORTEX_NAN  = 9999999

def processFile(file):
    print("Started file: "+file)
    outfile = file + '.csv'
    try:
        if OVERWRITE or not os.path.isfile( outfile ):
            i = 0
            with open(outfile, 'w') as fo:
                with open(file,'rb') as f:
                    filename = file
                    if '/' in filename:
                        filename = filename[filename.rfind('/')+1:]
                    for x in msgpack.Unpacker(f):
                        if not isinstance(x, int):
                            # Get time
                            dt = datetime.fromtimestamp(x[5]//1000).strftime('%Y-%m-%d %H:%M:%S')
                            # Process ID'ed bodies
                            for b in x[2]:
                                # Extract marker points
                                pos = np.nanmean([[z if z!=CORTEX_NAN else float('NaN') for z in y] 
                                    for y in b[1]], axis=0)
                                fo.write(','.join([dt, filename, str(b[0]), str(x[0])] + [str(y) for y in pos]) + '\n')
                        i+=1
                        if ((i%1000000)==0): 
                            print("[" + file + "] Processed "+str(i)+" frames")
        else:
            print("Skipping file: "+file)
    except Exception as e:
        print(str(e))
        os.remove(outfile)

def run(async=False):
    if SINGLE_FILE == "":
        files = [x for x in os.listdir('./') if x.endswith('.msgpack')]
        with multiprocessing.Pool(processes=12) as pool:
            (pool.map_async if async else pool.map)(processFile, files)
            return pool
    else:
        processFile(SINGLE_FILE)

if __name__ == '__main__':    
    run()