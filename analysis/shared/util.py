
# --------------------------------------------------------
# Utility script with shared functions
#
# Author: Abel Corver
#         abel.corver@gmail.com
#         (Anthony Leonardo Lab, Dec. 2016)
# --------------------------------------------------------

import os, sys, psutil, time, collections, msgpack
import numpy as np
from datetime import datetime
import pandas as pd
from scipy import interpolate
import sqlite3
import tkinter as tk
from tkinter import filedialog

DATA_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), '../../data/'))

# =======================================================================================
# Data Structures and Constants
# =======================================================================================

Frame = collections.namedtuple('Frame', 'frame pos vertices trajectory time nearbyVertices')

MocapFrame = collections.namedtuple('MocapFrame', 'byteOffset frameID time yframes unidentifiedVertices centroids calibrationFile')

RawFrame = collections.namedtuple('RawFrame', 'cameraID width height centroids')
Centroid = collections.namedtuple('Centroid', 'x y q')

ExtractionSettings = collections.namedtuple('ExtractionSettings', 'files groupOutputByDay')

CORTEX_NAN = 9999999

# =======================================================================================
# Helper function for controlling process priority
#
#    Note: Because this library often runs at 100% cpu, the pc might become unusable.
#          To prevent this, all processes are automatically given below-normal process
#          priority, to keep the other programs and interfaces responsive.
# =======================================================================================

# Technically, the highest priority (realtime) is 20, but this makes the computer
# difficult to use, and hangs other processes. So because these are all non-realtime
# processing scripts, we never go higher than "HIGH" priority.
HIGHEST_PRIORITY = 2
LOWEST_PRIORITY = -20

def setProcessPriority(priority):

    niceness = 0

    isWindows = True
    try:
        sys.getwindowsversion()
    except AttributeError:
        isWindows = False

    if not isWindows:
        niceness = priority
    else:
        if priority == -20:
            niceness = psutil.IDLE_PRIORITY_CLASS
        elif priority == -1:
            niceness = psutil.BELOW_NORMAL_PRIORITY_CLASS
        elif priority == 0:
            niceness = psutil.NORMAL_PRIORITY_CLASS
        elif priority == 1:
            niceness = psutil.ABOVE_NORMAL_PRIORITY_CLASS
        elif priority == 2:
            niceness = psutil.HIGH_PRIORITY_CLASS
        elif priority == 20:
            niceness = psutil.REALTIME_PRIORITY_CLASS
        else:
            niceness = psutil.IDLE_PRIORITY_CLASS

    p = psutil.Process()
    print("Setting process priority to "+str(niceness))
    p.nice(niceness)

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
# Get recent msgpack files
# =======================================================================================

# Age of file (w.r.t. last modified) in seconds
def fileAge(f):
    if f.endswith('.msgpack'):
        fnameOrig = f.replace('.raw.msgpack','').replace('.msgpack','.msgpack.original')
        if os.path.exists(fnameOrig):
            f = fnameOrig
    ft = time.time() - os.path.getmtime(f)
    return ft

# Size of file (in MB)
def fileSize(f):
    return os.path.getsize(f) / (1024 * 1000)

def getRecentFiles(maxFileAge, minFileAge, minFileSize):
    # Gather a list of data files
    files = [os.path.join(DATA_DIR, y) + '/' + x for y in os.listdir(DATA_DIR) if
               os.path.isdir(os.path.join(DATA_DIR, y)) and 'PREVIOUS_DATA' not in y
                 for x in os.listdir(os.path.join(DATA_DIR, y)) if x.endswith('.msgpack')]

    print(str(len(files))+" files found in folder.")

    # Process newest files first
    files.sort(key=lambda x: fileAge(x))

    # Keep only files within the minimum-maximum age range
    files = [x for x in files if fileAge(x) > minFileAge and \
             fileAge(x) < maxFileAge and fileSize(x) > minFileSize]

    return files

# =======================================================================================
# Ask for post-processing input files
# =======================================================================================

def askForExtractionSettings(allRecentFilesIfNoneSelected=False):

    # Ask for filenames
    root = tk.Tk()
    root.withdraw()
    filepaths = filedialog.askdirectory(title="Select the directory to process.")
    dirs = [filepaths,] #list(root.tk.splitlist(filepaths))
    
    # Get all the msgpack files in the directory (we don't choose .raw files, even where available.
    # The assumption is the relevant script will switch to the .raw.msgpack file where necessary.)
    files = []
    for dir in dirs:
        dataFiles = [x for x in os.listdir(dir) if x.endswith('.msgpack')]
        if len(dataFiles) > 1:
            dataFiles = [x for x in os.listdir(dir) if \
                x.endswith('.msgpack') and not x.endswith('.raw.msgpack')]
            if len(dataFiles) != 1:
                # There should only be one or two msgpack files in a directory, at most one 
                # .msgpack file, and at most one .raw.msgpack. If these requirements are not 
                # met, skip this directory
                # TODO: Give a more helpful warning message instead...
                continue
        files += [os.path.join(dir, x) for x in dataFiles]

    # Process newest files first
    if len(files) > 0:
        files.sort(key=lambda x: -fileAge(x))
    elif allRecentFilesIfNoneSelected:
        files = getRecentFiles(3600 * 24 * 365, 60, 2)
        for file in files:
            print("  file: "+file)
        if len(files) == 0:
            print("No recent files found...")

    return ExtractionSettings(files, False)

# =======================================================================================
# Correct frame indices
#
# Note: Cortex can be stopped and restarted. Each time it is restarted, the frame index resumes
#       from 0. This causes duplicate frame issues, etc. To prevent this issue, each .msgpack
#       file is pre-processed to ensure frameID's are unique. Because we will only ever
#       record ~10 hours, and it is not currently conceivable we would record more than
#       24 hours, the maximum conceivable frameID within a single consecutive block is
#       24 * 3600 * 200 = 17,280,000 ~= 2^24 (assuming 200 fps motion capture). We therefore
#       use the higher power of two bits to encode each "restart" of the counter. In this way
#       the original frameID can easily be recovered from any frameID by selecting only
#       the 24 least significant bits.
#
#       Correcting frame indices is a risky business when done wrong. If this function were
#       to crash, we could lose the original datafile. Therefore, at first the corrected file
#       is written to a new ".msgpack.tmp" file. Consequently, after this file is successfully
#       created, the original file is either renamed to ".msgpack.old" (when debugging)
#       otherwise or deleted.
#
#       This function is run as part of the "buildMocapIndex" function, as that function requires
#       a unique frame ID for lookup purposes. When a .msgpack.index file already exists,
#       this function will refuse to execute.
#
#       Note that running this function will create an incompatibility in frame indices between
#       daily files and
#
# =======================================================================================

def correctFrameIndices(file):

    # Check if frame indices have already been corrected
    if os.path.exists(file.replace('.msgpack','.msgpack.index')):
        raise Exception("Index file already exists, so frame IDs have already been corrected.")

    # Set output paths
    fnameOutOriginal = file.replace('.msgpack','') + '.msgpack.original'
    fnameOutTmp      = file.replace('.msgpack','') + '.msgpack.tmp'

    # Check if the temporary output file doesn't already exist
    # If it exists, don't run this function
    if os.path.exists(fnameOutOriginal):
        raise Exception(".msgpack.original file already exists...")

    # Print status
    print("Correcting frame indices...")

    # This variable keeps track of the number of "restarts"
    numRestarts = 0

    # This variable keeps track of last frameID (used to detect "restarts")
    lastFrameID = None

    # Transfer data and correct frame index
    with open(fnameOutTmp, 'wb') as fOut:
        with open(file, 'rb') as fIn:
            unpacker = msgpack.Unpacker(fIn)
            for data in unpacker:
                frameID = data[0]

                # Restart?
                if lastFrameID != None and frameID <= lastFrameID:
                    numRestarts += 1
                    print("Restart detected in frame index... adjusting frame "
                          "indices (#restarts="+str(numRestarts)+".")
                lastFrameID = frameID

                data[0] = frameID + (numRestarts << 24)
                fOut.write(msgpack.packb(data))

    # Currently, we rename the current file to .original for security purposes,
    # in case anything is wrong with the "fix index" algorithm. However,
    # once we fully trust this function, the .msgpack.original file can be deleted,
    # as the original frameID's can be recovered from the newly assigned frameID's
    # We currently, however, do not back up the .raw.msgpack.original file, as this is
    # itself a copy of existing data.

    if file.endswith('.raw.msgpack'):
        os.remove(file)
    else:
        os.rename(file, fnameOutOriginal)
    # Assign the .tmp file the original input filename... The index adjustment/fix is hereby
    # hidden from any other functions (indirectly) calling this function.
    os.rename(fnameOutTmp, file)

    # Print status
    print("Done correcting frame indices.")

    # Done!
    pass

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
        # Correct frame indices to remove duplicates, if necessary
        if not file.endswith('.raw.msgpack'):
            correctFrameIndices(file)

        # Start creating index
        conn = sqlite3.connect(ofile)
        c = conn.cursor()
        c.execute('''CREATE TABLE idx
                (frameID integer PRIMARY KEY, timestamp DATETIME, offset integer)''')
        c.execute('CREATE INDEX i1 ON idx (frameID)')
        c.execute('CREATE INDEX i2 ON idx (timestamp)')
        conn.commit()
        
        buf = []
        curOffset = 0
        for frame in iterMocapFrames(file):
            buf.append( (frame.frameID, frame.time, curOffset) )
            curOffset = frame.byteOffset
            
            i += 1
            if (i % 100000) == 0:
                try:
                    c.executemany('insert into idx (frameID, timestamp, offset) values (?,?,?)', buf)
                except:
                    # If failed, find the conflicting frameID
                    for b in buf:
                        try:
                            c.execute('insert into idx (frameID, timestamp, offset) values (?,?,?)', b)
                        except Exception as e:
                            print("Error inserting frame, frameID="+str(b[0])+".")
                            print("Likely duplicate frame index.")
                            raise e # Make sure program is interrupted...
                conn.commit()
                buf = []
                if verbose:
                    print("Processed "+str(i))
        
        c.executemany('insert into idx (frameID, timestamp, offset) values (?,?,?)', buf)
        conn.commit()
        conn.close()

# =======================================================================================
# Get all markers in a given range of frames
# =======================================================================================

def iterAllMarkers(fname, startFrame, numFrames):
    for frame in MocapFrameIterator(fname, startFrame=startFrame, numFrames=numFrames):
        yield getAllMarkersInFrame(frame)

# =======================================================================================
# Return all 3d markers in a MocapFrame, regardless of whether they're recognized or not
# =======================================================================================

def getAllMarkersInFrame(frame):
    m = []
    try:
        m += np.array(frame.unidentifiedVertices).tolist()
        for yf in frame.yframes:
            m += yf.vertices.tolist()
        return m
    except Exception as e:
        print(frame)
        raise e

# =======================================================================================
# Iterate over available frameIDs
# =======================================================================================

def iterMocapFrameIDs(fname, includeRowIDs=True):

    # Get the index file, and ensure it exists
    fileIdx = fname.replace('.msgpack', '.msgpack.index')
    if not os.path.exists(fileIdx):
        # Auto-build necessary index if it doesn't exist
        buildMocapIndex(fname)

    # Connect to the index file, and loop over its indices
    conn = sqlite3.connect(fileIdx)
    c = conn.cursor()
    i = 0
    for x in c.execute('select frameID from idx',()):
        if includeRowIDs:
            yield i, x[0]
        else:
            yield x[0]
        i += 1
    conn.close()

# =======================================================================================
# Iterator for Yframes and return the parsed structure... 
# =======================================================================================

# This function currently parses the .msgpack file format
#    o This file format was used for data files between ~August 2016 - present...
#    o In the future, we may switch to another data format, in which case this function 
#      can simply be expanded, without having to change other functions.
#

class MocapFrameIterator:
    def __init__(self, file, nearbyVertexRange=None, startFrame=None, endFrame=None, numFrames=None, startTime=None):
        self.counter = StreamCounter()
        self.numFramesYielded = 0
        self.startFrame = startFrame
        self.endFrame = endFrame
        self.numFrames = numFrames
        self.nearbyVertexRange = nearbyVertexRange

        if not os.path.exists(file):
            self.unpacker = None
        elif file.endswith('.msgpack'):

            # Open the data file
            self.f = open(file,'rb')

            # If a start frame is requested, look it up in the index
            if startFrame is not None or startTime is not None:
                fileIdx = file.replace('.msgpack','.msgpack.index')
                if not os.path.exists(fileIdx):
                    # Auto-build necessary index if it doesn't exist
                    buildMocapIndex(file)
                
                # Seek to right point in file
                conn = sqlite3.connect(fileIdx)
                c = conn.cursor()

                if startFrame is not None:
                    s = [x for x in c.execute(
                        'select frameID, offset from idx where frameID <= ? order by frameID DESC limit 2',
                            (int(startFrame),))]
                    if len(s) == 0:
                        raise Exception("Start frame specified, but no frameID <= requested frame found... \n" +
                                        "Tried query: 'select frameID, offset from idx where frameID <= " + str(
                            startFrame) + " order by frameID desc limit 1'")
                    else:
                        self.f.seek(s[0][1])

                elif startTime is not None:
                    s = [x for x in c.execute(
                        'select timestamp, frameID, offset from idx where timestamp < '+str(int(startTime))+
                        ' and timestamp != 0 order by frameID DESC limit 20',())]
                    if len(s) == 0:
                        raise Exception("Start time specified, but no timestamp <= requested start time... \n" +
                                        "Tried query: 'select timestamp, frameID, offset from idx where timestamp <= " + str(
                            startTime) + " order by frameID desc limit 1'")
                    else:
                        self.f.seek(s[0][2])

                # Note: This code above has been found not to work when type(startFrame)==np.int64...
                #       We're therefore forcing 'int' type here!
                conn.close()

            self.unpacker = msgpack.Unpacker(self.f)
        else:
            raise Exception("Mocap Frame Iterator currently only parses .msgpack files.")
    
    def __iter__(self):
        return self

    def stopIteration(self, raiseStopIteration=True):
        self.f.close()
        if raiseStopIteration:
            raise StopIteration
    
    def __next__(self):
        if self.unpacker is None:
            self.stopIteration()

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
            
            # Get calfile, if it exists
            calfile = ''
            if len(x) >= 8:
                calfile = x[7]
            
            # Done!
            if self.numFrames == None or self.numFramesYielded < self.numFrames:
                self.numFramesYielded += 1
                return MocapFrame(byteOffset, iframe, timestamp, yframes, unidentifiedVertices, centroids, calfile)
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

    for frame in MocapFrameIterator(file, nearbyVertexRange=nearbyVertexRange):
        for yframe in frame.yframes:
            yield yframe

    """ DEPRECATED:
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
    """

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

def countRecords(file, includeFrameRange=False, createIndexIfNotExists=True):
    # Determine total number of records
    print("Determining number of records")
    totalNumRecords = 0
    minFrameIdx =  99999999
    maxFrameIdx = -99999999
    
    fnameIdx = file.replace('.msgpack','.msgpack.index')
    
    # Create an index if it doesn't exist
    if createIndexIfNotExists and not os.path.exists(fnameIdx):
        buildMocapIndex(file)
        
    # Is an index available already?
    if os.path.exists(fnameIdx):
        print("Using pre-computed index: "+fnameIdx)
        conn = sqlite3.connect(fnameIdx)
        c = conn.cursor()
        totalNumRecords = [x for x in c.execute('select count(*) from idx')][0][0]
        minFrameIdx = [x for x in c.execute('select min(frameid) from idx')][0][0]
        maxFrameIdx = [x for x in c.execute('select max(frameid) from idx')][0][0]
        conn.close()
    else:
        for frame in MocapFrameIterator(file):
            totalNumRecords += 1
            minFrameIdx = min(minFrameIdx, frame.frame)
            maxFrameIdx = max(maxFrameIdx, frame.frame)
        print("Total number of records: "+str(totalNumRecords))
        
        if totalNumRecords == 0:
            minFrameIdx = maxFrameIdx = None
            
    # Done!
    return totalNumRecords if not includeFrameRange else (totalNumRecords, minFrameIdx, maxFrameIdx)

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
        fsTracking.loc[idx, 'istraj'] = True

        return fsTracking
    except pd.io.common.EmptyDataError as e:
        return None
    except Exception as e2:
        return None