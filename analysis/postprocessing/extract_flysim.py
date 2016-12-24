
#
# This script extracts FlySim trials
#

# Imports
import os, sys, msgpack, multiprocessing, warnings
import numpy as np
import numpy_groupies as npg
from time import time
import statsmodels.api as sm
from functools import partial
import pandas as pd

import postprocessing.extract_perching_locations

# Global settings
DEBUG = False
IGNORE_COUNT = False

# 
#SINGLEFILE = '2016-11-07 12-51-23_Cortex.msgpack' # SINGLEFILE = None
#SINGLEFILE = '2016-11-11 12-20-41_Cortex.msgpack'
#SINGLEFILE = '2016-11-14 14-09-32_Cortex.msgpack'
#SINGLEFILE = '2016-12-10 12-18-45.msgpack'
SINGLEFILE = ''

# Set "overwrite" to True to overwrite existing files
OVERWRITE = False

# Misc. constants
CORTEX_NAN = 9999999

# After how many (Cortex) frames does a trail go dead?
TRAJ_TIMEOUT = 100

# Maximum distance (in mm) between trajectory frames
TRAJ_MAXDIST = 75

# Minimum displacement a trajectory should have to be saved
TRAJ_SAVE_MINDIST = 500
TRAJ_SAVE_MINLEN  = 200

# Change data directory
os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)),'../data'))

# Number of trajectories saved
numSavedTraj = 0

# =======================================================================================
# Determine whether a trajectory is a FlySim trajectory, then save it
# =======================================================================================

def processTrajectory(trajectory, fo, foTracking):
    with warnings.catch_warnings():
        warnings.simplefilter("ignore", category=RuntimeWarning)

        # Can this be a flysim trajectory? Required minimum amount of displacement
        minBound = np.array([ sys.float_info.max for i in range(3)])
        maxBound = np.array([-sys.float_info.max for i in range(3)])

        for t in trajectory:
            minBound = np.minimum(t[2], minBound)
            maxBound = np.maximum(t[2], maxBound)

        isDistOK = (np.linalg.norm( maxBound - minBound ) > TRAJ_SAVE_MINDIST)

        # TODO: Check if points accompany Yframe at all times... if so, this is head/wing/etc. marker, not flysim
        dstFromYframe = np.array([ np.nanmin([np.linalg.norm(t[2]-y) for y in t[3]]) if len(t[3])>0 \
            else float('NaN') for t in trajectory])
        dstFromYframeMean = np.nanmean(dstFromYframe, axis=0)
        dstFromYframeSD   = np.nanstd(dstFromYframe, axis=0)
    
        # Check direction of takeoff
        data = np.array([x[2] for x in trajectory])
        x    = [a[0] for a in data]
        y    = [a[1] for a in data]
        z    = [a[2] for a in data]
        A    = np.vstack([x, y, np.ones(len(x))]).T

        f = sm.OLS(z, A).fit() 
        R2 = f.rsquared
        params = f.params

        isDirOK = ((R2 > 0.5) and np.linalg.norm(params[0:2] - np.array([0, 0.5])) < 2.0)
    
        # Check minimum length
        isLenOK = (len(trajectory) > TRAJ_SAVE_MINLEN)
        
        # Check minimum Z (Flysim currently never goes below ~200)
        minz = np.nanmin(np.array(z))
        
        # Check variance of points in each frame
        def means(c, x): 
            x['r'+c] = x[c] - np.repeat(x[c].mean(skipna=True), len(x.index))
            return x
        
        df = pd.DataFrame( {'f': [t[1] for t in trajectory], 'x': x, 'y': y, 'z': z} )
        df = df.groupby('f').apply(partial(means, 'x'))
        df = df.groupby('f').apply(partial(means, 'y'))
        df = df.groupby('f').apply(partial(means, 'z'))
        
        stds = [df['r'+c].std (skipna=True) for c in ['x','y','z']]
        ptStd = np.nansum(stds)
        
        # Enforce: 
        #   o minimum trajectory length of 70 frames, 
        #   o minimum distance traveled
        #   o direction along x dimension
        
        is_flysim = isDistOK and isLenOK and ptStd < 5.5 and dstFromYframeMean > 80 and minz >= 200

        if isLenOK and isDistOK:
            # Save trajectory 
            trajID = t[0]
            fStart = min([t[1] for t in trajectory])
            fEnd   = max([t[1] for t in trajectory])
            fo.write( ','.join([str(x) for x in [trajID, fStart, fEnd, is_flysim, 
                np.linalg.norm(params[0:2] - np.array([0, 0.5])), 
                R2, dstFromYframeMean, dstFromYframeSD, isDistOK, isLenOK, isDirOK, ptStd] + stds]) + '\n' )
            fo.flush()
            
            # Save the flysim trajectory
            for t in trajectory:
                foTracking.write( ','.join([str(x) for x in [t[0], t[1], ] + t[2].tolist() ]) + '\n' )

# =======================================================================================
# Extract trajectories
# =======================================================================================

def processFile(file):
    
    openTrajectories = []

    # Output file names
    foName         = file.replace('.msgpack','.flysim.csv')
    foNameTracking = file.replace('.msgpack','.flysim.tracking.csv')

    # Don't overwrite existing file
    if not OVERWRITE and os.path.isfile( foName ): 
        print("Skipping file: "+file)
        return

    # Determine total number of records
    print("Determining number of records")
    totalNumRecords = 0
    if DEBUG or IGNORE_COUNT:
        totalNumRecords = 1e6
    else:
        with open(file,'rb') as f:
            for x in msgpack.Unpacker(f):
                totalNumRecords += 1
        print("Total number of records: "+str(totalNumRecords))
    
    # Start loop
    lastInfoTime = time()
    numRecords = 0
    trajectoryID = 0

    # Open output file
    with open(foName,'w') as fo, open(file,'rb') as f, open(foNameTracking,'w') as foTracking:
        # Write header
        fo.write('trajectory, framestart, frameend, is_flysim, score_dir, score_r2, distanceFromYframe, '+
                 'distanceFromYframeSD, dist_ok, len_ok, dir_ok, std, stdX, stdY, stdZ\n')

        foTracking.write('trajectory,frame,x,y,z\n')

        # Get new data row
        for x in msgpack.Unpacker(f):
            # Print debug info
            numRecords += 1

            if (time() - lastInfoTime) > 5.0:
                lastInfoTime = time()
                print( ("["+file[-20:-1].replace('.msgpack','')+"] "+str(numRecords)+
                    " frames [{0:.2f}%], "+str(len(openTrajectories))+
                    " open traj").format(numRecords*100.0/totalNumRecords))
            # ...
            if not isinstance(x, int):
                # Get Yframes
                yframes = []
                for b in x[2]:
                    if 'Yframe' in b[0].decode():
                        iframe = x[0]
                        vertices = np.array([[z if z!=CORTEX_NAN else 
                            float('NaN') for z in y] for y in b[1]])
                        if np.all(vertices!=vertices): continue
                        yframes.append( np.nanmean(vertices, axis=0) )

                # Loop through unIDed points
                for c in x[3]:
                    iframe = x[0]
                    pos = np.array([z if z!=CORTEX_NAN else 
                        float('NaN') for z in c])
                    
                    # Fast-forward to relevant part
                    if DEBUG and iframe < 370000: continue

                    # Continue if all vertices are NaN
                    if np.all(pos!=pos): continue
                    
                    # Find corresponding trajectory index 
                    minDist = 1e6
                    t = -1
                    for i in range(len(openTrajectories)):
                        d = np.linalg.norm(pos - openTrajectories[i][-1][2])
                        if d < minDist:
                            minDist = d
                            t = i
                    
                    # Recent enough trajectory?
                    if t != -1 and (iframe - openTrajectories[t][-1][1]) > TRAJ_TIMEOUT:
                        # Timed out... Now:
                        #   o Determine whether this is a flySim trajectory
                        #   o Remove this trajectory from the open list
                        processTrajectory(openTrajectories[t], fo, foTracking)
                        del openTrajectories[t]
                    else:   
                        # Recent enough... add data to existing or new trajectory 
                        if t != -1 and minDist < TRAJ_MAXDIST: # and openTrajectories[t][-1][1] != iframe:
                            openTrajectories[t].append( (openTrajectories[t][-1][0], iframe, pos, yframes) )
                        else:
                            trajectoryID += 1
                            openTrajectories.append([(trajectoryID, iframe, pos, yframes),])
        
        # Process the remaining trajectories
        for t in openTrajectories:
            processTrajectory(t, fo, foTracking)
    
# =======================================================================================
# Main loop
# =======================================================================================

def run(async=False):
        
    # Get files
    files = [x for x in os.listdir('./') if x.endswith('.msgpack')]

    # Process newest files first
    files.sort(key=lambda x: -os.path.getmtime(x))

    if SINGLEFILE != '':
        processFile(SINGLEFILE)
    else:
        with multiprocessing.Pool(processes=16) as pool:
            (pool.map_async if async else pool.map)(processFile, files)
            return pool

    return None

if __name__ == "__main__": 
    run()