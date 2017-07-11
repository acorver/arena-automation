
#
# This script facilitates accessing trajectory information...
# It simplifies the process of correctly combining the output of the analysis files to get
# particular trajectories of interest.
#

import os, pandas as pd

# A cache for trajectories
TRAJECTORY_CACHE = {}

# Extract from .takeoffs.csv the actual trajectories of interest
def getTakeoffs(fname, minPerchFrames=300, minspan=150):

    # Determine data filename
    if fname.endswith('.msgpack'):
        fname = fname.replace('.raw.msgpack','').replace('.msgpack','') + '.takeoffs.csv'
    # Read data
    d = pd.read_csv(fname)
    d = d.rename(columns=lambda x: x.strip())
    # We're interested in takeoffs starting from a perching position
    d = d[d.perchframes >= minPerchFrames]
    # We're interested in takeoffs that span at least a certain distance
    #    Note: The takeoff detection script is conservative and will report many small "takeoffs",
    #          i.e. many small movements. However, we're only interested in significant trajectories
    d = d[d.bboxsize >= minspan]
    # Done
    return d

# Get a particular takeoff
def getTakeoff(fnameOrDataframe, trajectoryID, startFrame, maxNumFrames = 600):

    global TRAJECTORY_CACHE

    # Get dataframe
    data = None
    if isinstance(fnameOrDataframe, str):
        if fnameOrDataframe in TRAJECTORY_CACHE:
            data = TRAJECTORY_CACHE[fnameOrDataframe]
        else:
            # Determine data filename
            fname = fnameOrDataframe.replace('.raw.msgpack','').\
                replace('.msgpack','') + '.tracking.csv'
            data = pd.read_csv(fname)
            TRAJECTORY_CACHE[fnameOrDataframe] = data
    else:
        data = fnameOrDataframe

    # Subset dataset to return only this trajectory
    d = data.copy()
    d = d[d.trajectory == trajectoryID]
    d = d[(d.frame >= startFrame) & (d.frame <= (startFrame+maxNumFrames))]

    # Done!
    return d

def getTakeoffData(fname):
    takeoffs = getTakeoffs(fname)

    data = None
    for itraj, traj in takeoffs.iterrows():
        d = getTakeoff(fname, traj.trajectory, traj.frame)
        if data is None:
            data = d
        else:
            data = data.append(d)

    return data

def getFlysims(fname):
    fnameCsv = fname.replace('.raw.msgpack','').\
                   replace('.msgpack', '') + '.flysim.csv'
    if not os.path.exists(fnameCsv):
        return None
    else:
        flysim = pd.read_csv(fnameCsv)
        flysim = flysim.rename(columns=lambda x: x.strip())
        flysim = flysim.loc[flysim.is_flysim,:]
        return flysim

def getFlysimData(fname):
    fnameCsv = fname.replace('.raw.msgpack','').\
                   replace('.msgpack','')+'.flysim.tracking.csv'
    try:
        if not os.path.exists(fnameCsv):
            return None
        else:
            flysims = getFlysims(fname)
            if flysims is None:
                return None
            else:
                flysimIDs = flysims.flysimTraj.unique()

                flysim = pd.read_csv(fnameCsv)
                flysim = flysim.rename(columns=lambda x: x.strip())
                flysim = flysim[flysim.trajectory.isin(flysimIDs)]

                return flysim
    except:
        return None
