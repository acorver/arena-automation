
# =======================================================================================
# Change working directory so this script can be run independently as well as as a module
# =======================================================================================

import os, sys
if __name__ == "__main__": 
    p = os.path.dirname(os.path.abspath(__file__))
    os.chdir(os.path.join(p,'../../data'))
    sys.path.insert(0, os.path.join(p,'../'))

# =======================================================================================
# Imports for this script
# =======================================================================================

import msgpack
import numpy as np
import math, os, multiprocessing
from datetime import datetime
from shared import util

#
# TODO: Don't save positions that are all NaN
#

# Set "overwrite" to True to overwrite existing files
OVERWRITE = False

SINGLE_FILE = "" #"2016-11-11 12-20-41_Cortex.msgpack"

# Misc. constants
CORTEX_NAN  = 9999999

def processFile(file):
    print("Started file: "+file)
    outfile = file.replace('.msgpack','.xyz.csv')
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
        settings = util.askForExtractionSettings()    

        if len(settings.files) > 1:
            with multiprocessing.Pool(processes=12) as pool:
                (pool.map_async if async else pool.map)(processFile, settings.files)
                return pool
        else:
            processFile(settings.files[0])
    else:
        processFile(SINGLE_FILE)

if __name__ == '__main__':    
    run()