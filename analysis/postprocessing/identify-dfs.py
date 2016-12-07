
#
# Step 1. Extract trajectories 
# Step 2. Project the unidentified markers in the space of the Yframe 
#         for each trajectory
# Step 3. Find spheres with consistent spacing to ID.
#

# Look at 10/26-17 data

# ================================================
# Imports
# ================================================

import msgpack
import numpy as np
import math, os, multiprocessing
from time import time

# Helper function
def flatten(z): return [x for y in z for x in y]

# ================================================
# Global settings
# ================================================

os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)),'test'))

# Debugging?
DEBUG = False
IGNORE_COUNT = False
PROCESS_SAMPLE_LIMIT = -1

# Set "overwrite" to True to overwrite existing files
OVERWRITE = False

# Misc. constants
CORTEX_NAN = 9999999

# After how many (Cortex) frames does a trail go dead?
TRAJ_TIMEOUT = 100

# Maximum distance (in mm) between trajectory frames
TRAJ_MAXDIST = 10

# Maximum distance from unID'ed vertex to Yframe center
MAX_UNID_DIST = 100

# ================================================
# Process file
# ================================================

def transformToYframeSpace(v, vertices):
    a1  = vertices[2,:] - vertices[0,:]
    a2  = vertices[1,:] - 0.5 * (vertices[2,:] + vertices[0,:])
    a1 /= np.linalg.norm(a1)
    a2 /= np.linalg.norm(a2)
    a3  = np.cross(a1,a2)
    m   = np.matrix([a1, a2, a3])
    return m * np.matrix(v).T                      

def processFile(file, saveDst, saveDstError):

    # List of trajectories to keep matching against
    # Note: if the data gap becomes too large, the open trajectories
    # are transferred to the 'trajectories' array
    openTrajectories = []

    # Output file names
    foName  = file.replace('.msgpack',             ('' if saveDst == None else '.'+str(saveDst)+'mm') + '.csv')
    fodName = file.replace('.msgpack', '.dists'  + ('' if saveDst == None else '.'+str(saveDst)+'mm') + '.csv') 
    foyName = file.replace('.msgpack', '.yframe' + ('' if saveDst == None else '.'+str(saveDst)+'mm') + '.csv') 

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
    with open(foName,'w') as fo, open(file,'rb') as f, open(fodName,'w') as fod, open(foyName,'w') as foy:
        for x in msgpack.Unpacker(f):
            # Only process small amount of data for debugging
            if PROCESS_SAMPLE_LIMIT != -1 and numRecords > PROCESS_SAMPLE_LIMIT: break

            # Print debug info
            numRecords += 1
            if (time() - lastInfoTime) > 5.0:
                lastInfoTime = time()
                print( ("["+file[-20:-1].replace('.msgpack','')+","+str(saveDst)+"] "+str(numRecords)+
                    " frames [{0:.2f}%], "+str(len(openTrajectories))+
                    " open traj").format(numRecords*100.0/totalNumRecords))
            # ...
            if not isinstance(x, int):
                for b in x[2]:
                    if 'Yframe' in b[0].decode():
                        iframe = x[0]
                        vertices = np.array([[z if z!=CORTEX_NAN else 
                            float('NaN') for z in y] for y in b[1]])

                        # Continue if all vertices are NaN
                        if np.all(vertices!=vertices): continue

                        pos = np.nanmean(vertices, axis=0)
                        # Find corresponding trajectory index 
                        minDist = 1e6
                        t = -1
                        for i in range(len(openTrajectories)):
                            d = np.linalg.norm(pos - openTrajectories[i][-1][2])
                            if d < minDist:
                                minDist = d
                                t = i
                        # Recent enough trajectory?
                        if t != -1 and iframe - openTrajectories[t][-1][1] > TRAJ_TIMEOUT:
                            # Timed out, remove this trajectory from the open list
                            del openTrajectories[t]
                        else:   
                            # Recent enough...
                            # find nearby unID'ed vertices
                            nearbyVertices = []
                            nearbyVerticesOriginal = []
                            for c in x[3]:
                                # Get vertex
                                v = np.array([z if z!=CORTEX_NAN else 
                                    float('NaN') for z in c]) - pos
                                # Don't accept markers with any NaN's at this point
                                if not np.any(v!=v):
                                    # Transform coordinates into Yframe space
                                    vt = transformToYframeSpace(v, vertices)
                                    # Only proceed if this vertex is close enough
                                    if np.linalg.norm(vt) < MAX_UNID_DIST:
                                        nearbyVertices.append(vt)
                                        nearbyVerticesOriginal.append(v)
                            # add data to existing or new trajectory
                            p = None
                            if t != -1 and minDist < TRAJ_MAXDIST:
                                p = (openTrajectories[t][-1][0], iframe, pos, vertices, nearbyVertices, nearbyVerticesOriginal)
                                openTrajectories[t].append( p )
                            else:
                                trajectoryID += 1
                                p = (trajectoryID, iframe, pos, vertices, nearbyVertices, nearbyVerticesOriginal)
                                openTrajectories.append([p,])
                            # Save only particular marker points?
                            pts = None
                            ptsOriginal = None
                            unids = p[4]
                            if saveDstError != None and saveDst != None:
                                # Find points with right distance apart
                                closestPair = (-1,-1)
                                closestPairError = 1e6
                                for i1 in range(len(unids)):
                                    for i2 in range(len(unids)):
                                        if i1 != i2:
                                            e = abs(saveDst-np.linalg.norm(
                                                unids[i1] - unids[i2]))
                                            if e < saveDstError:
                                                closestPairError = e
                                                closestPair = (i1,i2)
                                if closestPair[0] > 0 and closestPair[1] > 0:
                                    pts = [unids[closestPair[0]], unids[closestPair[1]]]
                                    ptsOriginal = [p[5][closestPair[0]], p[5][closestPair[1]]]
                                else:
                                    pts = []
                                    ptsOriginal = []
                            else:
                                pts = unids
                                ptsOriginal = p[5]
                            
                            # Save unIDed markers                 
                            for v in pts:
                                if not np.all(v!=v): 
                                    fo.write( 
                                        str(p[0]) + ',' + str(p[1]) + ',' +
                                        ','.join([str(x) for x in p[2].tolist()]) + ',' + 
                                        ','.join([str(x) for x in v.T.tolist()[0]]) + '\n')
                            # Save pairwise distances
                            for i1 in range(len(pts)):
                                for i2 in range(len(pts)):
                                    if i1 > i2:
                                        fod.write( str(p[0]) + ',' + str(p[1]) + ',' +
                                            ','.join([str(x) for x in p[2].tolist()]) + ',' + 
                                            ','.join([str(x) for x in flatten(pts[i1].tolist())]) + ',' + 
                                            ','.join([str(x) for x in flatten(pts[i2].tolist())]) + ',' + 
                                            ','.join([str(x) for x in ptsOriginal[i1].tolist()]) + ',' + 
                                            ','.join([str(x) for x in ptsOriginal[i2].tolist()]) + ',' + 
                                            str(np.linalg.norm(pts[i1] - pts[i2])) + ',' + 
                                            str(np.linalg.norm(ptsOriginal[i1] - ptsOriginal[i2])) + '\n' )
                            # Save yframe data
                            if not np.any(vertices!=vertices):
                                foy.write( 
                                    str(p[0]) + ',' + str(p[1]) + ',' +
                                    ','.join([str(x) for x in p[2].tolist()]) + ',' + 
                                    ','.join([str(x) for x in flatten(vertices.tolist())]) + ',' +
                                    ','.join([str(x) for x in flatten(transformToYframeSpace(vertices-pos, vertices).tolist())]) + '\n')

    # Done!
    pass

# ================================================
# Entry point
# ================================================

if __name__ == '__main__':
    
    files = flatten([[(x,y,0.3 * y if y!=None else None) for y in [None, 5, 10]] 
        for x in os.listdir('./') if x.endswith('.msgpack')])

    if DEBUG:
        for file in [x for x in files if x[0]=='2016-10-27 15-05-35_Cortex.msgpack']:
            processFile(file[0], file[1], file[2])
    else:
        with multiprocessing.Pool(processes=24) as pool:
            pool.starmap(processFile, files)
