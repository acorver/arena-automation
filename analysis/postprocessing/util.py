
# --------------------------------------------------------
# Utility script with shared functions
#
# Author: Abel Corver, abel.corver@gmail.com
#         (Anthony Leonardo Lab, Dec. 2016)
# --------------------------------------------------------

import collections, msgpack
import numpy as np
from datetime import datetime

Frame = collections.namedtuple('Frame', 'frame pos vertices trajectory time nearbyVertices')

CORTEX_NAN = 9999999

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

def updateYFrame(frame, trajectory=None):
    return Frame(
        frame=frame.frame, 
        pos=frame.pos, 
        vertices=frame.vertices, 
        trajectory=frame.trajectory if trajectory==None else trajectory, 
        nearbyVertices=frame.nearbyVertices,
        time=frame.time)

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

def getTimeStr(time):
    return datetime.fromtimestamp(time//1000).strftime('%Y-%m-%d %H:%M:%S')
