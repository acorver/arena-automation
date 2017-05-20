#
#
#

# =======================================================================================
# Change working directory so this script can be run independently as well as as a module
# =======================================================================================

import os, sys
if __name__ == "__main__":
    p = os.path.dirname(os.path.abspath(__file__))
    sys.path.insert(0, os.path.join(p,'../'))
    os.chdir(p)

# =======================================================================================
# Imports and global constants for this script
# =======================================================================================

import threading
import multiprocessing
from time import sleep
from time import time
from queue import Queue
import pandas as pd, numpy as np
from shared import util
import matplotlib.pyplot as plt
import scipy.misc
import seaborn as sns
from ggplot import *
import concurrent.futures
import datetime
import traceback

# =======================================================================================
# Find number of available cameras
# =======================================================================================

def getMaxCamID(fname):
    maxCamID = 0
    i = 0
    for frame in util.iterMocapFrames(fname):
        if len(frame.centroids.keys()) > 0:
            maxCamID = max(maxCamID, max(frame.centroids.keys()))
        i += 1
        if i > 5e3:
            break
    return maxCamID

# =======================================================================================
# Find 
# =======================================================================================

# ...

# =======================================================================================
# Find triggerbox markers in a given frame
# =======================================================================================

def findTriggerBoxInFrame(frame, camID, spacing=None, debug=True):
                
    # Result variables
    centroids = []
    if camID in frame.centroids:
        for c in frame.centroids[camID].centroids:
            npt = np.array([c.x, c.y])
            if len(centroids)==0 or min([np.linalg.norm(npt-c2) for c2 in centroids]) >= 0:
                centroids.append( npt )
    centroids = np.array(centroids)

    # Find co-linear, equi-distantly spaced triplet of centroids
    syncbox = []
    for i1,c1 in zip(range(len(centroids)),centroids):
        for i2,c2 in zip(range(len(centroids)),centroids):
            for i3,c3 in zip(range(len(centroids)),centroids):
                if i2 > i1 and i3 > i2:
                    a = c1 - c2
                    b = c2 - c3 
                    c = c1 - c3 
                    colinearity = abs(np.dot( a, b ) * np.dot( b, c ) / 
                      (np.linalg.norm(a)*np.linalg.norm(b)*np.linalg.norm(b)*np.linalg.norm(c)))
                    d1 = np.linalg.norm(b-a)
                    d2 = np.linalg.norm(c-b)
                    d3 = np.linalg.norm(c-a)
                    maxdist = np.max([d1,d2,d3])
                    if colinearity > 0.9 and maxdist < 200:
                        na = np.linalg.norm(a)
                        nb = np.linalg.norm(b)
                        syncbox.append({'c1': c1, 'c2': c2, 'c3': c3, 'frame': frame.frameID, 
                                        'spacing': 0.5 * (na + nb), 'timestamp': frame.time, 
                                        'camID': camID, 'equidist': max(na/nb,nb/na)})
    
    if len(syncbox) > 0:
        # Did we specify a known spacing? If so, sort by spacing, using a reasonable equidist threshold
        if spacing != None:
            syncbox = [x for x in syncbox if x['equidist'] < 1.3]
            syncbox = sorted(syncbox, key=lambda x: abs(spacing-x['spacing']))
            if len(syncbox) > 0:
                
                # Save original syncbox ranking as debug info
                if debug:
                    syncbox[0]['ranking'] = syncbox
                
                # Choose top one
                syncbox = syncbox[0]
            
                # Now check if triggers occurred
                additionalPts = []
                for i,c in zip(range(len(centroids)),centroids):
                    try:
                        syncboxPts = [syncbox[m] for m in ['c1','c2','c3']]
                    except Exception as e:
                        traceback.print_exc()
                    mindist = np.min([np.linalg.norm(c-c2) for c2 in syncboxPts+additionalPts])
                    if mindist > 5:
                        isColinear = np.min([np.cross(c2 / np.linalg.norm(c2), 
                                          c / np.linalg.norm(c)) for c2 in syncboxPts]) < 4
                        isNearby = mindist < 0.7 * spacing
                        if isNearby and isColinear:
                            additionalPts.append(c)
                syncbox['additionalPts'] = additionalPts
            else:
                syncbox = None
        else:
            syncbox = sorted(syncbox, key=lambda x: -x['equidist'])
            syncbox = syncbox[0]
            syncbox['additionalPts'] = []
    else:
        syncbox = None        
    
    # Add all centroids (useful for debugging)
    if syncbox != None and debug:
        syncbox['allcentroids'] = centroids
    
    # Triggered?
    triggered = False
    if syncbox != None:
        if len(syncbox['additionalPts']) >= 3:
            triggered = True

    # Done!
    return syncbox, triggered

# =======================================================================================
# Find triggerbox points in all frames
# =======================================================================================

def findTriggerBoxInFrames(fname, camID, startFrame=None, numFrames=None):
    i = -1
    for frame in util.iterMocapFrames(fname, startFrame=startFrame, numFrames=numFrames):
        
        tb, triggered = findTriggerBoxInFrame(frame, camID, spacing=38)
        
        if tb != None:
            tbc = []
            for m in ['c1','c2','c3']:
                tbc.append([tb['camID'],tb['frame'],tb[m][0],tb[m][1],'syncbox'])
            for c in tb['additionalPts']:
                tbc.append([tb['camID'],tb['frame'],c[0],c[1],'additionalpt'])
            for c in tb['allcentroids']:
                notYetIncluded = True
                for pt in [tb[m] for m in ['c1','c2','c3']] + tb['additionalPts']:
                    if np.linalg.norm(pt - c) < 1:
                        notYetIncluded = False
                        break
                if notYetIncluded:
                    tbc.append([tb['camID'],tb['frame'],c[0],c[1],'all'])
        
            yield tbc
            i += 1

        if (i%1000)==0:
            print("Processed "+str(i)+" frames")

# =======================================================================================
# Find triggers in parallel
# =======================================================================================

def findTriggerAsync(frame, camID):
    try:
        syncbox, triggered = findTriggerBoxInFrame(frame, camID, spacing=38)
        if triggered:
            timestamp_str = str(datetime.datetime.fromtimestamp(syncbox['timestamp']/1000))
            return syncbox['camID'], syncbox['frame'], syncbox['timestamp'], timestamp_str
        else:
            return None
    except Exception as e:
        return e
       
# Worker function    
def findTriggers_Worker(tasks, output, async=True):
    while True:
        taskList = tasks.get()
        for frame, camID in taskList:
            if not frame is None:
                r = findTriggerAsync(frame, camID)
                if isinstance(r, Exception):
                    print(r)
                elif r != None:
                    # When a trigger was detected write the frame number and camera ID to file
                    output.put(','.join([str(y) for y in r])) 
        tasks.task_done()
        # If async is false, we break the for loop, so control goes back to findTriggers while loop
        if not async:
            break

def findTriggers(fname, camID, async=True, startFrame=None):
        
    # Number of CPUs to use (due to IO reading limits, only 8 workers appear to be necessary 
    # for maximum performance)
    NUM_CPUS = 8
    
    # Create an iterator over MoCap frames
    mocap = util.MocapFrameIterator(fname, startFrame=startFrame)
    
    # Get total number of frames
    totalNumFrames = util.countRecords(fname)

    tasks = multiprocessing.JoinableQueue()
    output = multiprocessing.Queue()

    # Create worker threads
    pool = None
    if async:
        pool = multiprocessing.Pool(NUM_CPUS, findTriggers_Worker, (tasks, output))
    
    # Start queueing frames
    numFramesProcessed = 0
    with open(fname.replace('.raw.msgpack','').replace('.msgpack','')+'.led_triggers','w') as fOut:
        while True:
            try:
                numFramesProcessed += 1000
                # We send frames to be processed in batches, which should speed up processing
                tasks.put( [(mocap.__next__(), camID) for i in range(1000)] )
                if tasks.qsize() > 100:
                    sleep(1)
                if (numFramesProcessed%1000)==0:
                    s = "/" + str(totalNumFrames)+" ("+'{0:.{1}f}'.format(
                            100*numFramesProcessed/totalNumFrames, 3)+'%)'
                    print("Processed "+str(numFramesProcessed)+s+" frames")
                # If not async, process the newly queued item now
                if not async:
                    findTriggers_Worker(tasks, output, async=False)
                # Write output
                while not output.empty():
                    s = output.get()
                    print("Wrote to file: "+s)
                    fOut.write(s+'\n')
                    fOut.flush()
            except StopIteration as esi:
                break
    
    print("Done searching for triggers.")

# =======================================================================================
# Test code, to run this as a standalone script
# =======================================================================================

if __name__ == "__main__":
    
    startFrame = None #1588914 - 10
    camID = 17
    fname = "../../data/2017-04-20 14-08-04/2017-04-20 14-08-04.raw.msgpack"
    async = True # False
    
    findTriggers(fname, camID, async=async, startFrame=startFrame)

        