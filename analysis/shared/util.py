
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
import pickle
import sqlite3

import tkinter as tk
from tkinter import filedialog
from tkinter import messagebox
from tkinter import simpledialog

# =======================================================================================
# Data Structures and Constants
# =======================================================================================

Frame = collections.namedtuple('Frame', 'frame pos vertices trajectory time nearbyVertices')

MocapFrame = collections.namedtuple('MocapFrame', 'byteOffset frameID time yframes unidentifiedVertices centroids')

RawFrame = collections.namedtuple('RawFrame', 'cameraID width height centroids')
Centroid = collections.namedtuple('Centroid', 'x y q')

ExtractionSettings = collections.namedtuple('ExtractionSettings', 'files groupOutputByDay')

CORTEX_NAN = 9999999

# =======================================================================================
# Helper class
# =======================================================================================

# Source: https://github.com/msgpack/msgpack-python/issues/76
class StreamCounter(object):
    __slots__ = ('nbytes',)
    def __init__(self):
        self.nbytes = 0
    def __call__(self, s):
        self.nbytes += len(s)

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
# Build an index for a mocap file
# =======================================================================================

def buildMocapIndex(file, verbose=False):
    ofile = file.replace('.msgpack','.msgpack.index')
    i = 0
    if not file.endswith('.msgpack'):
        raise Exception("Mocap data has to be in .msgpack format.")
    elif os.path.exists(ofile):
        raise Exception("Mocap index already exists.")
    else:
        conn = sqlite3.connect(ofile)
        c = conn.cursor()
        c.execute('''CREATE TABLE idx
                (frameID integer PRIMARY KEY, offset integer)''')
        c.execute('CREATE INDEX i1 ON idx (frameID)')
        conn.commit()
        
        buf = []
        curOffset = 0
        for frame in iterMocapFrames(file):
            buf.append( (frame.frameID, curOffset) )            
            curOffset = frame.byteOffset
            
            i += 1
            if (i % 100000) == 0:
                c.executemany('insert into idx (frameID, offset) values (?,?)', buf)
                conn.commit()
                buf = []
                if verbose:
                    print("Processed "+str(i))
        
        c.executemany('insert into idx (frameID, offset) values (?,?)', buf)
        conn.commit()
        conn.close()

# =======================================================================================
# Iterator for Yframes and return the parsed structure... 
# =======================================================================================

# This function currently parses the .msgpack file format
#    o This file format was used for data files between ~August 2016 - present...
#    o In the future, we may switch to another data format, in which case this function 
#      can simply be expanded, without having to change other functions.

class MocapFrameIterator:
    def __init__(self, file, nearbyVertexRange=None, startFrame=None, endFrame=None, numFrames=None):
        self.counter = StreamCounter()
        self.numFramesYielded = 0
        self.startFrame = startFrame
        self.endFrame = endFrame
        self.numFrames = numFrames
        self.nearbyVertexRange = nearbyVertexRange

        if file.endswith('.msgpack'):

            # Open the data file
            self.f = open(file,'rb')

            # If a start frame is requested, look it up in the index
            if startFrame != None:
                fileIdx = file.replace('.msgpack','.msgpack.index')
                if not os.path.exists(fileIdx):
                    # Auto-build necessary index if it doesn't exist
                    buildMocapIndex(file)
                
                # Seek to right point in file
                conn = sqlite3.connect(fileIdx)
                c = conn.cursor()
                s = [x for x in c.execute(
                    'select frameID, offset from idx where frameID <= ? order by frameID DESC limit 2', 
                        (int(startFrame),))]
                # Note: This code above has been found not to work when type(startFrame)==np.int64...
                #       We're therefore forcing 'int' type here!
                conn.close()
                if len(s) == 0:
                    raise Exception("Start frame specified, but no frameID <= requested frame found... \n" + 
                        "Tried query: 'select frameID, offset from idx where frameID <= "+str(startFrame)+" order by frameID desc limit 1'")
                else:
                    self.f.seek(s[0][1])
            
            self.unpacker = msgpack.Unpacker(self.f)
        else:
            raise Exception("Mocap Frame Iterator currently only parses .msgpack files.")
    
    def __iter__(self):
        return self

    def stopIteration(self):
        self.f.close()
        raise StopIteration
    
    def __next__(self):
        try:
            x = self.unpacker.unpack(write_bytes=self.counter)
                    
            iframe = x[0]
            yframes = []
            unidentifiedVertices = []
            
            # Get time if it exists (older files don't have a time field)
            timestamp = x[5] if len(x)>=6 else 0        
                           
            byteOffset = self.counter.nbytes
                
            if self.endFrame != None and iframe >= self.endFrame:
                self.stopIteration()

            if not isinstance(x, int):
                    
                # Parse ID'ed markers
                for b in x[2]:
                    if 'Yframe' in b[0].decode():
                        vertices = np.array([[z if z!=CORTEX_NAN else 
                            float('NaN') for z in y] for y in b[1]])

                        # Continue if all vertices are NaN
                        if np.all(vertices!=vertices): continue

                        pos = np.nanmean(vertices, axis=0)
                    
                        # Optionally get nearby vertices
                        nearbyVertices = None
                        if self.nearbyVertexRange != None:
                            nearbyVertices = []
                            for c in x[3]:
                                # Get vertex
                                v = np.array([z if z!=CORTEX_NAN else 
                                    float('NaN') for z in c])
                                # Don't accept markers with any NaN's at this point
                                if not np.any(v!=v):
                                    # Only proceed if this vertex is close enough
                                    if np.linalg.norm(v - pos) < self.nearbyVertexRange:
                                        nearbyVertices.append(v)
                                    
                        # Pass data to processing function
                        yframes.append( Frame(frame=iframe, vertices=vertices, pos=pos, 
                            trajectory=-1, time=timestamp, nearbyVertices=nearbyVertices) )
    
                # Parse unID'ed markers
                for b in x[3]:
                    pos = np.array([z if z!=CORTEX_NAN else float('NaN') for z in b])
                        
                    # Continue if all vertices are NaN
                    if np.all(pos!=pos): continue
                    
                    unidentifiedVertices.append(pos)
                
            # Parse centroids, if they have been added to this data file...
            centroids = {}
                
            if len(x) >= 7:
                for c in x[6]:
                    cs = [Centroid(y[0], y[1], y[2]) for y in c[3]]
                    rf = RawFrame(c[0], c[1], c[2], cs)
                    centroids[rf.cameraID] = rf
                
            # Done!
            if self.numFrames == None or self.numFramesYielded < self.numFrames:
                self.numFramesYielded += 1
                return MocapFrame(byteOffset, iframe, timestamp, yframes, unidentifiedVertices, centroids)
            else:
                self.stopIteration()
        except msgpack.OutOfData:
            self.stopIteration()

# =======================================================================================
# [DEPRECATED] Read Yframes and return the parsed structure (use this as iterator in for loop) 
# =======================================================================================

def iterMocapFrames(file, nearbyVertexRange=None, startFrame=None, endFrame=None, numFrames=None):
    
    mocap = MocapFrameIterator(
        file = file, 
        nearbyVertexRange = nearbyVertexRange, 
        startFrame = startFrame, 
        endFrame = endFrame, 
        numFrames = numFrames)
    
    for frame in mocap:
        yield frame

# =======================================================================================
# [DEPRECATED] Read Yframes and return the parsed structure (use this as iterator in for loop)
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
    except:
        return None