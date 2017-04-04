
# --------------------------------------------------------
# Utility script with shared functions
#
# Author: Abel Corver
#         abel.corver@gmail.com
#         (Anthony Leonardo Lab, Dec. 2016)
# --------------------------------------------------------

import os, collections, msgpack
import numpy as np
from datetime import datetime
import pandas as pd
from scipy import interpolate

import tkinter as tk
from tkinter import filedialog
from tkinter import messagebox
from tkinter import simpledialog

# =======================================================================================
# Data Structures and Constants
# =======================================================================================

Frame = collections.namedtuple('Frame', 'frame pos vertices trajectory time nearbyVertices')

MocapFrame = collections.namedtuple('MocapFrame', 'frameID yframes unidentifiedVertices centroids')

RawFrame = collections.namedtuple('RawFrame', 'cameraID width height centroids')
Centroid = collections.namedtuple('Centroid', 'x y q')

ExtractionSettings = collections.namedtuple('ExtractionSettings', 'files groupOutputByDay')

CORTEX_NAN = 9999999

# =======================================================================================
# Ask for post-processing input files
# =======================================================================================

def askForExtractionSettings():
    
    files = []
    groupOutputByDay = False
    
    # Ask for filenames
    root = tk.Tk()
    root.withdraw()
    numCameras = 0
    filepaths = filedialog.askopenfilenames(title="Select the raw data files (.msgpack) to process.")
    files += root.tk.splitlist(filepaths)    
        
    # Process newest files first
    files.sort(key=lambda x: -os.path.getmtime(x))
    
    return ExtractionSettings(files, False)


# =======================================================================================
# Read Yframes and return the parsed structure (use this as iterator in for loop)
# =======================================================================================

def iterMocapFrames(file, nearbyVertexRange=None):
    
    # Parse .msgpack file format
    #    o This file format was used for data files between ~August 2016 - present...
    #    o In the future, we may switch to another data format, in which case this function 
    #      can simply be expanded, without having to change other functions.
    
    if file.endswith('.msgpack'):
        with open(file,'rb') as f:
            for x in msgpack.Unpacker(f):
                
                iframe = x[0]
                yframes = []
                unidentifiedVertices = []
                
                if not isinstance(x, int):
                    
                    # Parse ID'ed markers
                    for b in x[2]:
                        if 'Yframe' in b[0].decode():
                            vertices = np.array([[z if z!=CORTEX_NAN else 
                                float('NaN') for z in y] for y in b[1]])

                            # Continue if all vertices are NaN
                            if np.all(vertices!=vertices): continue

                            pos = np.nanmean(vertices, axis=0)
                    
                            # Get time if it exists (older files don't have a time field)
                            t = x[5] if len(x)>=6 else 0
                    
                            # Optionally get nearby vertices
                            nearbyVertices = None
                            if nearbyVertexRange != None:
                                nearbyVertices = []
                                for c in x[3]:
                                    # Get vertex
                                    v = np.array([z if z!=CORTEX_NAN else 
                                        float('NaN') for z in c])
                                    # Don't accept markers with any NaN's at this point
                                    if not np.any(v!=v):
                                        # Only proceed if this vertex is close enough
                                        if np.linalg.norm(v - pos) < nearbyVertexRange:
                                            nearbyVertices.append(v)
                                    
                            # Pass data to processing function
                            yframes.append( Frame(frame=iframe, vertices=vertices, pos=pos, 
                                trajectory=-1, time=t, nearbyVertices=nearbyVertices) )
    
                    # Parse unID'ed markers
                    for b in x[3]:
                        pos = np.array([z if z!=CORTEX_NAN else float('NaN') for z in b])
                        
                        # Continue if all vertices are NaN
                        if np.all(pos!=pos): continue
                    
                        unidentifiedVertices.append(pos)
                
                # Parse centroids, if they have been added to this data file...
                centroids = {}
                
                if len(x) >= 6:
                    for c in x[6]:
                        cs = [Centroid(y[0], y[1], y[2]) for y in c[3]]
                        rf = RawFrame(c[0], c[1], c[2], cs)
                        centroids[rf.cameraID] = rf
                
                # Done!
                yield MocapFrame(iframe, yframes, unidentifiedVertices, centroids)
    else:
        raise Exception("Motion capture data format not supported: " + file)

# =======================================================================================
# DEPRECATED: Read Yframes and return the parsed structure (use this as iterator in for loop)
# =======================================================================================

def readYFrames(file, nearbyVertexRange=None):
    with open(file,'rb') as f:
        for x in msgpack.Unpacker(f):
            if not isinstance(x, int):
                for b in x[2]:
                    if 'Yframe' in b[0].decode():
                        iframe = x[0]
                        vertices = np.array([[z if z!=CORTEX_NAN else 
                            float('NaN') for z in y] for y in b[1]])

                        # Continue if all vertices are NaN
                        if np.all(vertices!=vertices): continue

                        pos = np.nanmean(vertices, axis=0)
                    
                        # Get time if it exists (older files don't have a time field)
                        t = x[5] if len(x)>=6 else 0
                    
                        # Optionally get nearby vertices
                        nearbyVertices = None
                        if nearbyVertexRange != None:
                            nearbyVertices = []
                            for c in x[3]:
                                # Get vertex
                                v = np.array([z if z!=CORTEX_NAN else 
                                    float('NaN') for z in c])
                                # Don't accept markers with any NaN's at this point
                                if not np.any(v!=v):
                                    # Only proceed if this vertex is close enough
                                    if np.linalg.norm(v - pos) < nearbyVertexRange:
                                        nearbyVertices.append(v)
                                    
                        # Pass data to processing function
                        yield Frame(frame=iframe, vertices=vertices, pos=pos, trajectory=-1, time=t, nearbyVertices=nearbyVertices)

# =======================================================================================
# Selectively update Yframe data structure
# =======================================================================================

def updateYFrame(frame, trajectory=None):
    return Frame(
        frame=frame.frame, 
        pos=frame.pos, 
        vertices=frame.vertices, 
        trajectory=frame.trajectory if trajectory==None else trajectory, 
        nearbyVertices=frame.nearbyVertices,
        time=frame.time)

# =======================================================================================
# Count number of records
# =======================================================================================

def countRecords(file):
    # Determine total number of records
    print("Determining number of records")
    totalNumRecords = 0
    
    with open(file,'rb') as f:
        for x in msgpack.Unpacker(f):
            if not isinstance(x, int):
                for b in x[2]:
                    if 'Yframe' in b[0].decode():
                        totalNumRecords += 1
    print("Total number of records: "+str(totalNumRecords))
    
    return totalNumRecords

# =======================================================================================
# Convert a timestamp to formatted string
# =======================================================================================

def getTimeStr(time):
    return datetime.fromtimestamp(time//1000).strftime('%Y-%m-%d %H:%M:%S')

# =======================================================================================
# Load FlySim tracking
# =======================================================================================

def loadFlySim(file):
    
    try:
        fs         = pd.read_csv(file.replace('.msgpack','.flysim.csv'), header=None, skiprows=1)
        fs.columns = ['flysimTraj', 'framestart', 'frameend', 'is_flysim', 'score_dir', 'score_r2', \
            'distanceFromYframe', 'distanceFromYframeSD', 'dist_ok', 'len_ok', 'dir_ok', \
            'std', 'stdX', 'stdY', 'stdZ']
        fs['frame'] = fs['framestart']
    
        fsTracking = pd.read_csv(file.replace('.msgpack','.flysim.tracking.csv'))
        fsTracking.columns = ['trajectory','frame','flysim.x','flysim.y','flysim.z']
    
        fsTracking = pd.merge(fsTracking, fs, on=['frame'], how='left')
        fsTracking['framestart'] = fsTracking['framestart'].ffill()
        fsTracking['frameend']   = fsTracking['frameend'].ffill()
        fsTracking['is_flysim']  = fsTracking['is_flysim'].ffill()
        fsTracking['flysimTraj'] = fsTracking['flysimTraj'].ffill()
        
        def f(x):
            x['minz'] = x['flysim.z'].min()
            x['zspan'] = x['flysim.z'].max() - x['flysim.z'].min()
            x['is_flysim'] = (x.is_flysim) & (x.zspan<50) & (x.minz>100)
            
            d = x.groupby(['frame',]).mean().reset_index()
            # Note: Dropping duplicates required for scipy.interpolate.splprep
            d.drop_duplicates(inplace=True, subset=['flysim.x','flysim.y','flysim.z'])
            d = d.as_matrix(['frame','flysim.x','flysim.y','flysim.z'])
            d = d[d[:,0].argsort()]
            tck, u = interpolate.splprep([d[:,1], d[:,2], d[:,3]], s=200)
            unew = np.arange(0, 1.0, 1.0 / x.shape[0])
            out = interpolate.splev(unew, tck)
            x['fs.appr.x'] = out[0][0:x.shape[0]]
            x['fs.appr.y'] = out[1][0:x.shape[0]]
            x['fs.appr.z'] = out[2][0:x.shape[0]]
            x['_e'] = np.linalg.norm(np.array([
                x['flysim.x']-x['fs.appr.x'],
                x['flysim.y']-x['fs.appr.y'],
                x['flysim.z']-x['fs.appr.z']]), axis=0)
            return x

        fsTracking = fsTracking.groupby(['trajectory',]).apply(f)
        idx = (fsTracking.groupby(['trajectory','frame'])['_e'].transform(min) == fsTracking._e)
        fsTracking['istraj'] = np.repeat(False, fsTracking.shape[0])
        fsTracking['istraj'][idx] = np.repeat(True, sum(idx))
        
        return fsTracking
    except pd.io.common.EmptyDataError as e:
        return None