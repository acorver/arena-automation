
#
# This script extracts FlySim trials
#

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

import msgpack, multiprocessing, threading, warnings
import multiprocessing.pool
import numpy as np
import numpy_groupies as npg
from time import time
import statsmodels.api as sm
from functools import partial
import pandas as pd
from time import sleep
import datetime

from shared import util
from postprocessing import extract_perching_locations

# Global settings
DEBUG = False
IGNORE_COUNT = False

# Manually overwrite what file is processed (for debugging purposes)
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
MAX_FLYSIM_DURATION_FRAMES = 3000

# Number of trajectories saved
numSavedTraj = 0

# Number of workers to use when processing asynchronously
NUM_WORKERS_ASYNC = 12

# =======================================================================================
# Determine whether a trajectory is a FlySim trajectory, then save it
# =======================================================================================

def trajectoryBBox(traj):
    minBound = np.array([ sys.float_info.max for i in range(3)])
    maxBound = np.array([-sys.float_info.max for i in range(3)])

    for t in traj:
        minBound = np.minimum(t[2], minBound)
        maxBound = np.maximum(t[2], maxBound)

    bboxSpan = np.linalg.norm( maxBound - minBound )
    
    return minBound, maxBound, bboxSpan

def processTrajectory(trajectory, output, outputTracking, workerID, numTrajectories):
    with warnings.catch_warnings():
        warnings.simplefilter("ignore", category=RuntimeWarning)
        
        # Find the best matching markers in each frame (currently multiple markers are added per frame 
        # based on proximity
        #bestMarkers = {}
        #for t in trajectory:
        #    iframe = t[1]
        #    if not iframe in bestMarkers:
        #        bestMarkers[iframe] = []
        #    bestMarkers[iframe].append(t)

        # Currently, we just average the markers (in the future, e.g. take re-calibration into account,
        # or compute Confidence Interval in way more sophisticated than just mean)
        #trajectoryNew = []
        #for iframe in sorted(bestMarkers.keys()):
        #    meanPos = np.mean([x[2] for x in bestMarkers[iframe]], axis=0)
        #    trajectoryNew.append( [
        #        bestMarkers[iframe][0][0],
        #        bestMarkers[iframe][0][1],
        #        meanPos,
        #        bestMarkers[iframe][0][3] ])
        # Replace trajectory with corrected trajectory
        #trajectory = trajectoryNew

        # Can this be a flysim trajectory? Required minimum amount of displacement
        minBound, maxBound, bboxSpan = trajectoryBBox(trajectory)
        isDistOK = (bboxSpan > TRAJ_SAVE_MINDIST)

        # TODO: Check if points accompany Yframe at all times... if so, this is head/wing/etc. marker, not flysim
        dstFromYframe = np.array([ np.nanmin([np.linalg.norm(t[2]-y) for y in t[3]]) if len(t[3])>0 \
            else float('NaN') for t in trajectory])
        dstFromYframeMean = np.nanmean(dstFromYframe, axis=0)
        dstFromYframeSD   = np.nanstd(dstFromYframe, axis=0)
        if np.isnan(dstFromYframeMean):
            dstFromYframeMean = np.inf
        if np.isnan(dstFromYframeSD):
            dstFromYframeSD = 0

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
        
        # Find start and end point of motion (first acceleration)
        """
        def _firstMotionFrame(v):
            curpos
            return 0
        motionStartFrame = _firstMotionFrame(trajectory)
        motionEndFrame   = _firstMotionFrame(reversed(trajectory))
        """
        
        # Enforce: 
        #   o minimum trajectory length of 70 frames, 
        #   o minimum distance traveled
        #   o direction along x dimension
        is_flysim = isDistOK and isLenOK and ptStd < 5.5 and dstFromYframeMean > 80 and minz >= 200

        if isLenOK and isDistOK:
            # Save trajectory 
            trajID = numTrajectories + (workerID+1) * 100000
            fStart = min([t[1] for t in trajectory])
            fEnd   = max([t[1] for t in trajectory])
            output.put( ','.join([str(x) for x in [trajID, fStart, fEnd, is_flysim, 
                np.linalg.norm(params[0:2] - np.array([0, 0.5])), 
                R2, dstFromYframeMean, dstFromYframeSD, isDistOK, isLenOK, isDirOK, ptStd] + stds]))
            
            # Save the flysim trajectory
            for t in trajectory:
                outputTracking.put( ','.join([str(x) for x in [trajID, t[1], ] + t[2].tolist() ]))
            
            # Increment trajectory ID
            numTrajectories += 1
    
    return numTrajectories

# =======================================================================================
# Extract trajectories (async supported)
# =======================================================================================

# Worker function
def extractFlysim_Worker(workerIDs, tasks, output, outputTracking, debugOutput):

    # Get a unique worker ID
    workerID = workerIDs.get()

    # Working variables
    openTrajectories = []
    trajectoryID = 0 
    done = False
    numTrajectories = 0
    
    while not done:
        taskList = tasks[workerID].get()
        # Are all frames read?
        if isinstance(taskList, str) and taskList == 'NO_FRAMES_LEFT':
            # Process the remaining trajectories
            for t in openTrajectories:
                numTrajectories = processTrajectory(t, output, outputTracking, workerID, numTrajectories)
            # Signal that we're done!!
            output.put('DONE:'+str(workerID))
            done = True
        else:
            # Loop through each frame
            for frame, maxNewTrajFrame, workerID in taskList:
                # Report debug state
                debugOutput[workerID].put( (len(openTrajectories), 'streaming') )

                # Calculate Yframe mean positions
                yframes = [np.nanmean(x.vertices, axis=0) for x in frame.yframes]
            
                # o Remove "trajectories" that have too many points... (currently > 20 second = 4000 frames).
                #   These are not flysim trajectories but likely stationary points, and we will loose much 
                #   time and RAM space by keeping around such trajectories
                # o Also remove trajectories that are too old (haven't been updated recently)
                i = 0
                while i < len(openTrajectories):
                    
                    if len( openTrajectories[i] ) > MAX_FLYSIM_DURATION_FRAMES or \
                        (frame.frameID - openTrajectories[i][-1][1]) > TRAJ_TIMEOUT:
                        if len( openTrajectories[i] ) <= MAX_FLYSIM_DURATION_FRAMES:
                            debugOutput[workerID].put(('processing',))
                            numTrajectories = processTrajectory(openTrajectories[i], \
                                output, outputTracking, workerID, numTrajectories)
                        del openTrajectories[i]
                        i = 0
                    else:
                        i += 1
                
                # Are we done with processing?
                if len(openTrajectories) == 0 and frame.frameID > maxNewTrajFrame:
                    output.put('DONE:'+str(workerID))
                    done = True
            
                # Loop through unIDed points
                for pos in frame.unidentifiedVertices:
                    # Find corresponding trajectory index 
                    minDist = 1e6
                    t = -1
                    for i in range(len(openTrajectories)):
                        d = np.linalg.norm(pos - openTrajectories[i][-1][2])
                        if d < minDist:
                            minDist = d
                            t = i
                    
                    # Recent enough... add data to existing or new trajectory 
                    if t != -1 and minDist < TRAJ_MAXDIST: # and openTrajectories[t][-1][1] != iframe:
                        openTrajectories[t].append( (openTrajectories[t][-1][0], frame.frameID, pos, yframes) )
                        # If this trajectory has been stationary for too long, split it
                        # This happens when the FlySim bead doesn't go completely out of view... What are multiple 
                        # trajectories will then be seen as one
                        #     - Compute volume spanned by last 3 seconds (600 frames) of points
                        if len(openTrajectories[t]) > 600:
                            minBound, maxBound, bboxSpan = trajectoryBBox(openTrajectories[t][(-200):(-1)])
                            if bboxSpan < 10:
                                debugOutput[workerID].put(('processing',))
                                numTrajectories = processTrajectory(openTrajectories[t], output, \
                                    outputTracking, workerID, numTrajectories)
                                del openTrajectories[t]
                    else:
                        # Create new trajectory (only if allowed)
                        if frame.frameID <= maxNewTrajFrame:
                            trajectoryID += 1
                            openTrajectories.append([(trajectoryID, frame.frameID, pos, yframes),])
            # Signal that this worker finished processing this chunk of frames
            tasks[workerID].task_done()

#
# Note: To parallelize the discovery of FlySim trajectories, the processFile(...) function 
#       splits the motion capture file into N contiguous blocks of frames. Each worker 
#       process will process these frames in parallel. However, the frames are still linearly 
#       read in, in a multiplexed way, but are then passed to the working processes.
#
def processFile(fname, async=True):
    
    # Number of CPUs to use (due to IO reading limits, only ? workers appear to be necessary 
    # for maximum performance)
    NUM_WORKERS = NUM_WORKERS_ASYNC if not DEBUG else 1

    # Output file names
    foName         = fname.replace('.raw.msgpack','.msgpack').replace('.msgpack','.flysim.csv')
    foNameTracking = fname.replace('.raw.msgpack','.msgpack').replace('.msgpack','.flysim.tracking.csv')
    
    # Determine total number of frames (this also forces the creation of the data file index... important!)
    totalNumFrames, minFrameIdx, maxFrameIdx = util.countRecords(fname, includeFrameRange=True)
    print("Total number of records: "+str(totalNumFrames))
    
    # An array of queues of frames to process, one queue for each worker process
    tasks = [multiprocessing.JoinableQueue() for i in range(NUM_WORKERS)]

    # An array for debug output
    debugOutput = [multiprocessing.JoinableQueue() for i in range(NUM_WORKERS)]
    debugStates = {}

    # An output queue for .flysim.csv data
    output = multiprocessing.Queue()

    # A queue of IDs... each worker will request a unique ID from this at startup, and use this
    # to select the appropriate tasks
    workerIDs = multiprocessing.JoinableQueue()
    for id in range(NUM_WORKERS):
        workerIDs.put(id)
    
    # An output queue for .flysim.tracking.csv data
    outputTracking = multiprocessing.Queue()

    # Create worker threads
    pool = None
    if async:
        if DEBUG:
            pool = multiprocessing.pool.ThreadPool(NUM_WORKERS, extractFlysim_Worker, (workerIDs, tasks, output, outputTracking, debugOutput))
        else:
            pool = multiprocessing.Pool(NUM_WORKERS, extractFlysim_Worker, (workerIDs, tasks, output, outputTracking, debugOutput))

    # Find frame chunks
    frameIDs = list(util.iterMocapFrameIDs(fname, includeRowIDs=False))
    currentStartFramesOfTasks = [frameIDs[int(float(i) *
      len(frameIDs) / float(NUM_WORKERS))] for i in range(NUM_WORKERS)]
    # No need to keep all frameIDs in memory anymore (this was unnecessary in the first place, but convenient)
    del frameIDs

    # The maximum frame index at which the worker process is allowed to start a new trajectory (but it's
    # still allowed to continue an old trajectory)
    maxNewTrajFrameOfTasks = [(currentStartFramesOfTasks[i+1]-1) if
      (i+1) < NUM_WORKERS else maxFrameIdx for i in range(NUM_WORKERS)]
    
    # Worker status
    workerDone = [False for i in range(NUM_WORKERS)]
    
    # Counter to keep track of how many frames we've processed
    numFramesProcessed = 0
    
    # Start queueing frames
    lastProgressReportTime = datetime.datetime.now()
    with open(foName,'w') as fo, open(foNameTracking,'w') as foTracking:
        
        # Write header
        fo.write('flysimTraj, framestart, frameend, is_flysim, score_dir, score_r2, distanceFromYframe, '+
                 'distanceFromYframeSD, dist_ok, len_ok, dir_ok, std, stdX, stdY, stdZ\n')
        
        # Write header
        foTracking.write('trajectory,frame,x,y,z\n')
        
        # Process all frames
        while True:
            # Add frames to each worker queue
            for workerID in range(len(tasks)):
                # If there are fewer than 3 chunks of frames left for a given worker process, add more frames
                readAllFrames = False
                if not workerDone[workerID]:
                    if tasks[workerID].qsize() < 3:
                        # Create an iterator over MoCap frames
                        mocap = util.MocapFrameIterator(fname, \
                            startFrame=currentStartFramesOfTasks[workerID])
                        # We send frames to be processed in batches, which should speed up processing
                        frames = []
                        try:
                            for i in range(2000):
                                frames.append( (mocap.__next__(), maxNewTrajFrameOfTasks[workerID], workerID) )
                            # Close iterator
                            mocap.stopIteration(raiseStopIteration=False)
                        except StopIteration:
                            readAllFrames = True
                        
                        # Keep counter of number of processed frames
                        numFramesProcessed += len(frames)
                        # Submit frames for processing
                        tasks[workerID].put( frames )
                        # Now remember to continue with the following frame
                        currentStartFramesOfTasks[workerID] = frames[-1][0].frameID + 1
                
                # Now see if this worker should shut down
                if readAllFrames:
                    # By pushing "NO_FRAMES_LEFT", we are signaling to the worker processes that all frames have been read
                    tasks[workerID].put( 'NO_FRAMES_LEFT' )
                    print("No frames left for worker "+str(workerID))
                
            # Update on progress
            if (datetime.datetime.now() - lastProgressReportTime).total_seconds() > 5:

                for qdi in range(len(debugOutput)):
                    while not debugOutput[qdi].empty():
                        debugStates[qdi] = debugOutput[qdi].get()
                dbgStr = ','.join([str(debugStates[x][0]) for x in debugStates])

                lastProgressReportTime = datetime.datetime.now()
                s = "/" + str(totalNumFrames)+" ("+'{0:.{1}f}'.format(
                        100*numFramesProcessed/totalNumFrames, 3)+'%)'
                print("Processed "+str(numFramesProcessed)+s+" frames: "+dbgStr)
                
            # Limit how often this runs
            sleep(0.5)
                
            # Write output
            while not output.empty():
                s = output.get()
                if 'DONE:' in s:
                    wid = int(s.replace('DONE:',''))
                    print("Worker done: "+str(wid))
                    workerDone[wid] = True
                else:
                    fo.write(s+'\n')
                    fo.flush()
                
            # Write tracking info
            while not outputTracking.empty():
                s = outputTracking.get()
                foTracking.write(s+'\n')
                foTracking.flush()
                
            # Break when all worker processes have exited
            allWorkersDone = True if len([x for x in workerDone if not x])==0 else False
            if allWorkersDone: 
                break
    # Done!
    print("Done extracting FlySim trials!")

# =======================================================================================
# [DEPRECATED] Determine whether a trajectory is a FlySim trajectory, then save it
# =======================================================================================

def processTrajectorySync(trajectory, fo, foTracking):
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
# [DEPRECATED] Extract trajectories synchronously
# =======================================================================================

def processFileSync(file):
    
    openTrajectories = []

    # Output file names
    foName         = file.replace('.raw.msgpack','.msgpack').replace('.msgpack','.flysim.csv')
    foNameTracking = file.replace('.raw.msgpack','.msgpack').replace('.msgpack','.flysim.tracking.csv')

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
        totalNumRecords = util.countRecords(file)
        print("Total number of records: "+str(totalNumRecords))
    
    # Start loop
    lastInfoTime = time()
    numRecords = 0
    trajectoryID = 0

    # Open output file
    with open(foName,'w') as fo, open(file,'rb') as f, open(foNameTracking,'w') as foTracking:
        # Write header
        fo.write('flysimTraj, framestart, frameend, is_flysim, score_dir, score_r2, distanceFromYframe, '+
                 'distanceFromYframeSD, dist_ok, len_ok, dir_ok, std, stdX, stdY, stdZ\n')

        foTracking.write('trajectory,frame,x,y,z\n')

        # Get new data row
        for frame in util.iterMocapFrames(file):
            
            # Print debug info
            numRecords += 1

            if (time() - lastInfoTime) > 5.0:
                lastInfoTime = time()
                print( ("["+file[-20:-1].replace('.msgpack','')+"] "+str(numRecords)+
                    " frames [{0:.2f}%], "+str(len(openTrajectories))+
                    " open traj").format(numRecords*100.0/totalNumRecords))
            
            # Calculate Yframe mean positions
            yframes = [np.nanmean(x.vertices, axis=0) for x in frame.yframes]
            
            # o Remove "trajectories" that have too many points... (currently > 20 second = 4000 frames).
            #   These are not flysim trajectories but likely stationary points, and we will loose much 
            #   time and RAM space by keeping around such trajectories
            # o Also remove trajectories that are too old (haven't been updated recently)
            i = 0
            while i < len(openTrajectories):
                if len( openTrajectories[i] ) > 4000 or \
                    (frame.frameID - openTrajectories[t][-1][1]) > TRAJ_TIMEOUT:
                    if len( openTrajectories[i] ) <= 4000:
                        processTrajectorySync(openTrajectories[i], fo, foTracking)
                    del openTrajectories[i]
                    i = 0
                else:
                    i += 1
            
            # Loop through unIDed points
            for pos in frame.unidentifiedVertices:
                # Find corresponding trajectory index 
                minDist = 1e6
                t = -1
                for i in range(len(openTrajectories)):
                    d = np.linalg.norm(pos - openTrajectories[i][-1][2])
                    if d < minDist:
                        minDist = d
                        t = i
                    
                # Recent enough... add data to existing or new trajectory 
                if t != -1 and minDist < TRAJ_MAXDIST: # and openTrajectories[t][-1][1] != iframe:
                    openTrajectories[t].append( (openTrajectories[t][-1][0], frame.frameID, pos, yframes) )
                else:
                    trajectoryID += 1
                    openTrajectories.append([(trajectoryID, frame.frameID, pos, yframes),])
        
        # Process the remaining trajectories
        for t in openTrajectories:
            processTrajectorySync(t, fo, foTracking)
    
# =======================================================================================
# Main loop
# =======================================================================================

def run(async=False, settings = None):
    
    if SINGLEFILE != '':
        processFile(SINGLEFILE)
    else:
        # Get files
        if settings == None:
            settings = util.askForExtractionSettings()
        
        if not DEBUG:
            if len(settings.files) == 1:
                processFile(settings.files[0])
            else:
                with multiprocessing.Pool(processes=16) as pool:
                    (pool.map_async if async else pool.map)(processFile, settings.files)
                    return pool
        else:
            for file in settings.files:
                processFile(file)

    return None

if __name__ == "__main__": 
    run()