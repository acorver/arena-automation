
#
# Extract continuous blocks where the animal sits in a certain location
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

import numpy as np, pandas as pd
import os, multiprocessing, gc
from time import time
import statsmodels.formula.api as smf
from scipy import stats

from shared import util
from shared import trajectories

# =======================================================================================
# Global constants
# =======================================================================================

# Set "overwrite" to True to overwrite existing files
OVERWRITE = False

# ...
DEBUG = False
SINGLEFILE = ''
IGNORE_COUNT = False

# Misc. constants
CORTEX_FPS = 200

# After how many (Cortex) frames does a trail go dead?
TRAJ_TIMEOUT = 100

# Maximum distance (in mm) between trajectory frames
TRAJ_MAXDIST = 100

# 
MAX_STATIONARY_MOVEMENT = 30

#
TRAJ_SAVE_MINLEN = 100
TRAJ_TAKEOFF_MINLEN = 50
PERCH_SAVE_MINLEN = 400

# Flysim Axis details (in XY plane)
FLYSIM_AXIS_PT1 = np.array([-400,  10])
FLYSIM_AXIS_PT2 = np.array([ 450, -15])

# Change working directory
os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)),'../../data'))

# =======================================================================================
# Process trajectory (either stable/perching or takeoff)
# =======================================================================================

def processTrajectory(trajectory, foPerches, foTakeoffs, foTracking, foDebug, takeoffID, flysim):
    
    # Enforce minimum trajectory length
    if len(trajectory) < TRAJ_SAVE_MINLEN: return takeoffID
    
    # Look for:
    #    o Point of takeoff
    #    o Upward motion following takeoff
    #  
    # In addition, label the following:
    #    o Motion towards center FlySim axis
    
    boundsMin  = [ float("inf"),  float("inf"),  float("inf")]
    boundsMax  = [-float("inf"), -float("inf"), -float("inf")]
    frameRange = ( float("inf"), -float("inf") )
    timeRange  = ( float("inf"), -float("inf") )
    numFrames, avg = (None, None)
    
    # Ranges of frame indices that were detected as "stationary"
    # We can then use the complement of these ranges to detect takeoff characteristics
    frameRanges= []

    # Find frame ranges
    for ti in range(len(trajectory)): 
        
        t = trajectory[ti]        
        
        # Update bounds                   
        boundsMin = np.nanmin( [t.pos, boundsMin], axis=0 )
        boundsMax = np.nanmax( [t.pos, boundsMax], axis=0 )
        
        # Update frame and time range
        frameRange = ( min(frameRange[0], t.frame), max(frameRange[1], t.frame) )
        timeRange  = ( min( timeRange[0], t.time ), max( timeRange[1], t.time ) )
            
        # Does any bounding box dimension exceed that allowed?
        if (len([x for x in (boundsMax-boundsMin) if x > MAX_STATIONARY_MOVEMENT]) > 0):
                        
            # If so, save the box and its length, start a new one
                        
            # Compute number of frames
            numFrames = len([x for x in trajectory if x.frame > frameRange[0] and x.frame < frameRange[1]])

            # Save data (only if minimum number of perching frames was detected)
            if (frameRange[1]-frameRange[0]) > 10:
                # Store range for additional processing
                frameRanges.append( frameRange ) 
                # Save
                savePerchInfo(foPerches, trajectory, frameRange, numFrames, \
                    timeRange, boundsMin, boundsMax)
                        
            # Reset bounding box
            boundsMin  = [ float("inf"),  float("inf"),  float("inf")]
            boundsMax  = [-float("inf"), -float("inf"), -float("inf")]
            frameRange = ( float("inf"), -float("inf") )

    # Add remaining open frame range
    frameRanges.append(frameRange)

    # Save frame ranges
    savePerchInfo(foPerches, trajectory, frameRange, numFrames, \
        timeRange, boundsMin, boundsMax)

    # Temporarily save takeoff info
    takeoffs = []

    # Detect takeoff characteristics
    for fr in frameRanges:
        ## Is this an upward trajectory? (during the first 2 sec, i.e. 400 frames) (Indicating takeoff)
        frames = [x for x in trajectory if x.frame >= (fr[1]-200) and x.frame < fr[1] + 600]

        # Minimum takeoff length required
        if (len(frames)-200) < TRAJ_TAKEOFF_MINLEN: continue
        
        # Is any position in trajectory at least 100 mm higher than starting position?
        #isUpward = ( len([x for x in frames if x.pos[2] > frames[0].pos[2]+100]) > 0 )
        delta = int(0.2 * CORTEX_FPS)
        maxUpwardSpeed = max([(frames[i+delta].pos[2]-frames[i].pos[2]) for i in range(len(frames) - delta)])
        
        ## Save framenumber when trajectory reached its peak
        framePeak = max(frames, key=lambda x:x.pos[2]).frame

        ## Is this trajectory never diverging from flysim point?
        data = { 'f': [], 'd': [] }
        minframe = min([x.frame for x in frames])
        flysimTraj = []
        for fi in range(len(frames)):
            frame = frames[fi].frame
            if frame in flysim:
                flysimPos = flysim[frame][1]
                flysimTraj.append(flysim[frame][0])
                data['f'].append( frame - minframe )
                data['d'].append( np.linalg.norm( flysimPos - frames[fi].pos ) )
                foDebug.write(','.join([str(x) for x in [fr[1],frame,frames[0].trajectory,data['f'][-1],data['d'][-1]]+flysimPos.tolist()])+'\n')

        # The flysim trajectory ID we save is the most common one (they should all be the same in the first place)
        flysimTraj = stats.mode(flysimTraj)
        if len(flysimTraj)>0 and len(flysimTraj[0])>0:
            flysimTraj = flysimTraj[0][0]
        else:
            flysimTraj = -1

        ## Range of the first 2 seconds (400 frames)
        bboxSize = np.linalg.norm(np.ptp(np.array([x.pos for x in frames]), axis=0))

        # Compute the fraction of the trajectory frames that did not diverge from flysim axis
        R2 = -1
        params = [0, 0, 0]
        if len(data['f'])>0:
            f = smf.ols(formula="d ~  f + np.power(f,2)", data=data).fit()
            R2 = f.rsquared
            params = f.params.tolist()

        ## Save
        takeoff = [fr[1], fr[1]-fr[0], frames[0].trajectory, frames[0].time, util.getTimeStr(frames[0].time), 
            bboxSize, maxUpwardSpeed,framePeak,flysimTraj] + params + [R2,]
        takeoffs.append( [takeoffID, ] + takeoff )
        takeoffID += 1
        
        foTakeoffs.write( ','.join([str(x) for x in takeoff]) + '\n' )
        foTakeoffs.flush()

    # Write tracking info
    for t in trajectory:
        # Find closest takeoff
        a = [(takeoff, (t.frame-takeoff[1])) for takeoff in \
            takeoffs if (t.frame-takeoff[1])>=0]
        takeoff  = min(a, key=lambda x:x[1])[0][0] if len(a)>0 else -1
        relFrame = min(a, key=lambda x:x[1])[0][1] if len(a)>0 else ''
        # Save
        foTracking.write( ','.join([str(x) for x in [t.frame, relFrame,
          t.trajectory, t.time, util.getTimeStr(t.time), takeoff, ] + t.pos.tolist() +
            t.vertices[0].tolist() + t.vertices[1].tolist() + t.vertices[2].tolist()]) + '\n' )
    
    return takeoffID

def savePerchInfo(foPerches, trajectory, frameRange, numFrames, timeRange, boundsMin, boundsMax):
    if frameRange[1]-frameRange[0] > PERCH_SAVE_MINLEN:

        # Compute average
        avg = np.nanmean(np.array([f.pos for f in trajectory if f.frame >= frameRange[0] and f.frame < frameRange[1]]), axis=0)

        foPerches.write( ','.join([str(x) for x in list(frameRange)+[numFrames,]+[trajectory[0].trajectory, ]+avg.tolist()+
            boundsMin.tolist()+boundsMax.tolist()+list(timeRange)+
            [util.getTimeStr(timeRange[0]), util.getTimeStr(timeRange[1])]]) + '\n' )
        foPerches.flush()

# =======================================================================================
# Process file
# =======================================================================================

def processFile(file):
    
    # Output file names
    fnamePerches  = file.replace('.msgpack', '.perches.csv')
    fnameTakeoffs = file.replace('.msgpack', '.takeoffs.csv')
    fnameTracking = file.replace('.msgpack', '.tracking.csv')
    fnameDebug    = file.replace('.msgpack', '.fsdbg.csv')
    
    # Don't overwrite existing file
    if not OVERWRITE and os.path.isfile( fnamePerches ): 
        print("Skipping file: "+file)
    else:
        # Debug header
        dbgHeader = "["+file.replace('_Cortex.msgpack','')[0:30]+"] "

        # Read known flysim locations
        print(dbgHeader+"Started reading flysim")
        fsTracking = util.loadFlySim(file)
        flysim = {}
        if fsTracking is not None:
            for (rowid, row) in fsTracking.iterrows():
                frame = int(row['frame'])
                if row['is_flysim'] and row['framestart'] <= frame and frame <= row['frameend']:
                    flysim[frame] = (row['flysimTraj'], np.array([row['flysim.'+c] for c in ['x','y','z']]))
            print(dbgHeader+"Finished reading flysim")
        else:
            print(dbgHeader+"Failed to read flysim... processed flysim data not found")

        # Open trajectories
        openTrajectories = []
        trajectoryID = 0

        # Start loop
        lastInfoTime = time()
        numRecords = 0
        totalNumRecords = util.countRecords(file)

        takeoffID = 0

        # ...
        with open(fnamePerches,'w') as foPerches, \
            open(fnameTakeoffs,'w') as foTakeoffs, open(fnameTracking,'w') as foTracking, \
            open(fnameDebug,'w') as foFsDbg:

            # Write headers for output files

            foTakeoffs.write('frame,perchframes,trajectory,timestamp,time,bboxsize,upwardVelocityMax,framepeak,flysimTraj,p1,p2,p3,r2\n')

            foPerches.write('framestart,frameend,numframes,trajectory,x,y,z,minx,miny,minz,' + \
                'maxx,maxy,maxz,timestampstart,timestampend,timestart,timeend\n')

            foTracking.write('frame,relframe,trajectory,timestamp,time,takeoffTraj,x,y,z,x1,y1,z1,x2,y2,z2,x3,y3,z3\n')

            for frame in util.readYFrames(file):
                # Print debug info
                numRecords += 1
                if (time() - lastInfoTime) > 5.0:
                    lastInfoTime = time()
                    print( (dbgHeader + str(numRecords) +
                        " frames [{0:.2f}%], " + str(len(openTrajectories)) +
                        " open traj").format(numRecords*100.0/totalNumRecords) )

                # Find corresponding trajectory index
                minDist = 1e6
                t = -1
                i = 0
                while i < len(openTrajectories):
                    # Check if trajectory timed out
                    if frame.frame - openTrajectories[i][-1].frame > TRAJ_TIMEOUT:
                        takeoffID = processTrajectory(openTrajectories[i],
                          foPerches, foTakeoffs, foTracking, foFsDbg, takeoffID, flysim)
                        del openTrajectories[i]
                    else:
                        # If not, evaluate whether it's the closest one
                        d = np.linalg.norm(frame.pos - openTrajectories[i][-1].pos)
                        if d < minDist:
                            minDist = d
                            t = i
                        i+=1

                # add data to existing or new trajectory
                p = None
                if t != -1 and minDist < TRAJ_MAXDIST:
                    p = util.updateYFrame(frame, trajectory=openTrajectories[t][-1].trajectory)
                    openTrajectories[t].append( p )
                else:
                    trajectoryID += 1
                    p = util.updateYFrame(frame, trajectory=trajectoryID)
                    openTrajectories.append([p,])

            # Process remaining open trajectories
            for t in openTrajectories:
                takeoffID = processTrajectory(t, foPerches, foTakeoffs, foTracking, foFsDbg, takeoffID, flysim)

    # Produce an easier to use subset of the data (actual takeoffs)
    produceConvenientSubset(file)

    # Done
    gc.collect()

# =======================================================================================
# Produce an easier to use subset of the data (actual takeoffs)
# =======================================================================================

def produceConvenientSubset(fname):

    data = trajectories.getTakeoffData(fname)

    if data is None:
        return

    # Add flysim data, if it exists
    flysim = trajectories.getFlysimData(fname)
    if flysim is not None:
        flysim = flysim.rename(index=str, columns={'trajectory': 'flysim_trajectory',
                                'x': 'x_fs', 'y': 'y_fs', 'z': 'z_fs'})
        data = data.join(flysim, on='frame', how='left', rsuffix='_')

    # Convert data into long format
    ndata = None
    for s in ['','1','2','3','_fs']:
        if ('x'+s) in data:
            d = data[['frame','trajectory','timestamp','time','x'+s,'y'+s,'z'+s]]
            d.columns = ['frame','trajectory','timestamp','time','x','y','z']
            if s == '':
                d.ix[:,'type'] = 'yf_center'
            elif s == '1':
                d.ix[:,'type'] = 'yf_bl'
            elif s == '2':
                d.ix[:,'type'] = 'yf_fc'
            elif s == '3':
                d.ix[:,'type'] = 'yf_br'
            elif s == '_fs':
                d.ix[:,'type'] = 'flysim'
            ndata = d if ndata is None else ndata.append(d)
    data = ndata

    # Add all other markers to this dataset, so we can verify whether a flysim event was missed,
    # etc.
    frames = data.frame.unique()
    d = []
    for frame in util.MocapFrameIterator(fname):
        if frame.frameID in frames:
            for m in util.getAllMarkersInFrame(frame):
                d.append([frame.frameID,] + m)
    d = pd.DataFrame(np.array(d), columns=['frame','x','y','z'])
    d.frame = d.frame.astype(np.int64)
    ndata = pd.merge(data, d, on='frame', how='right', suffixes=['','_'])
    ndata = ndata[['frame','trajectory','timestamp','time','x_','y_','z_']]
    ndata.columns = ['frame', 'trajectory', 'timestamp', 'time', 'x', 'y', 'z']
    ndata.ix[:,'type'] = 'all'
    data = data.append(ndata)

    # Save
    fnameOut = fname.replace('.raw.msgpack','').replace('.msgpack','') + '.mocap.all.csv'
    data.to_csv(fnameOut)
    fnameOut = fname.replace('.raw.msgpack','').replace('.msgpack','') + '.mocap.csv'
    data[data.type!='all'].to_csv(fnameOut)

# =======================================================================================
# Entry point
# =======================================================================================

def run(async=False, settings=None):

    util.setProcessPriority(util.HIGHEST_PRIORITY)

    if SINGLEFILE != "":
        processFile(SINGLEFILE)
    else:
        # Get files
        if settings == None:
            settings = util.askForExtractionSettings(allRecentFilesIfNoneSelected=True)

        if not DEBUG:
            if len(settings.files) == 1:
                processFile(settings.files[0])
            else:
                util.setProcessPriority(util.LOWEST_PRIORITY)
                with multiprocessing.Pool(processes=multiprocessing.cpu_count()) as pool:
                    (pool.map_async if async else pool.map)(processFile, settings.files)
                    return pool
        else:
            for file in settings.files:
                processFile(file)

    return None

if __name__ == '__main__':    
    run()


