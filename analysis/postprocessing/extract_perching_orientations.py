
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
import math, os, multiprocessing, warnings

# Debug switch
DEBUG = False

# Set "overwrite" to True to overwrite existing files
OVERWRITE = False

# Reference vectors
vZenith = np.array( [0 , 0 , 1] )
vX      = np.array( [1 , 0 , 0] )

# Misc. constants
CORTEX_NAN  = 9999999

def findPerchRangeIdx(frame, perchRanges, lastPerchRange):
    if lastPerchRange != -1 and \
        perchRanges[lastPerchRange][0] <= frame and \
        perchRanges[lastPerchRange][1] >= frame:
        return lastPerchRange
    else:
        for i in range(len(perchRanges)):
            if len(perchRanges[i]) != 2: continue
            if perchRanges[i][0] <= frame and \
                perchRanges[i][1] >= frame:
                return i
    return -1

def processFile(file):

    outfile   = file.replace('.msgpack','.angles.csv')
    perchfile = file.replace('.msgpack','.perches.csv')
    i = 0

    # Suppress errors relating to "mean of empty slice" due to frames with only NaNs
    warnings.simplefilter("ignore", category=RuntimeWarning)

    if OVERWRITE or not os.path.isfile(outfile):
        # Load perching location data
        perchRanges = []
        if os.path.isfile(perchfile):
            with open(perchfile, 'r') as fp:
                perchRanges = [ [int(y) for y in x.split(',')[10:12]] for x in fp.read().split('\n')]
        lastPerchRange = -1
        # Start processing
        with open(outfile , 'w') as fo:
            with open(file,'rb') as f:
                filename = file
                if '/' in filename:
                    filename = filename[filename.rfind('/')+1:]
                for x in msgpack.Unpacker(f):
                    try:
                        if not isinstance(x, int):
                            for b in x[2]:
                                if "Yframe" in str(b[0]):
                                    # Get frame index
                                    frameIdx = x[0]
                                    # Find the perch block this frame is in
                                    perchRangeIdx = findPerchRangeIdx(frameIdx, perchRanges, lastPerchRange)
                                    if perchRangeIdx != -1: lastPerchRange = perchRangeIdx
                                    # Markers
                                    markers = [[z if z!=CORTEX_NAN else float('NaN') for z in y] for y in b[1]]                                
                                    # Position
                                    pos = np.nanmean(markers, axis=0)
                                    # Skip if position is NaN
                                    if len([x for x in pos if math.isnan(x)]) > 0: continue
                                    # Gather 3D Yframe points
                                    pts = [np.array([z if z!=CORTEX_NAN else float('NaN') for z in y]) 
                                        for y in b[1]]
                                    # Compute the YFrame primary axis
                                    axis1   = pts[1]  - (.5 * pts[0] + .5 * pts[2])
                                    axis1_h = (axis1[0], axis1[1], 0)
                                    # Compute the YFrame secondary axis
                                    axis2 = pts[0] -  pts[2]
                                    # Normalize axes
                                    axis1   /= np.linalg.norm(axis1)
                                    axis1_h /= np.linalg.norm(axis1_h)
                                    axis2   /= np.linalg.norm(axis2)
                                    # Compute angles
                                    angleAltitude = math.acos(np.dot( vZenith, axis1   ))
                                    angleAzimuth  = math.acos(np.dot( vX     , axis1_h ))
                                    # Save
                                    fo.write(','.join([filename, str(b[0].decode('utf8')), str(frameIdx), str(perchRangeIdx)] + [str(y) for y in list(pos) + 
                                        list(axis1) + list(axis2) + [angleAltitude, angleAzimuth] + perchRanges[perchRangeIdx] + list(markers[0]) + list(markers[1]) + list(markers[2]) ]) + '\n')
                    except Exception as e:
                        print("error...")
                    
                    i+=1
                    if ((i%1000000)==0): 
                        print("Processed "+str(i)+" frames.")

def run(async=False):
    files = [x for x in os.listdir('./') if x.endswith('.msgpack')]

    if DEBUG:
        processFile(files[0])
    else:
        if len(settings.files) == 1:
            processFile(settings.files[0])
        else:
            with multiprocessing.Pool(processes=16) as pool:
                (pool.map_async if async else pool.map)(processFile, files)
                return pool
    
if __name__ == '__main__':    
    run()
