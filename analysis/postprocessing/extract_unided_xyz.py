
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
    outfile = file.replace('.msgpack','.unids.csv')
    try:
        if OVERWRITE or not os.path.isfile( outfile ):
            with open(outfile, 'w') as fo:
                for frame in util.iterMocapFrames(file):
                    for vi,v in zip(range(len(frame.unidentifiedVertices)),frame.unidentifiedVertices):
                        fo.write(','.join([str(x) for x in [frame.frameID,vi]+v.tolist()])+'\n')
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
