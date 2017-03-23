
# --------------------------------------------------------
# This script extracts head movements
#
# Author: Abel Corver, abel.corver@gmail.com
#
# Todo:
#    o compute segment lengths by unique dF ID
#    o Automatically determine if there are none, two or three head markers
#    o After segments have been detected, track segments across frames
#      Allow closest match to frames longer ago
# --------------------------------------------------------

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

import os, math
from shared import util
import pandas as pd
import numpy as np
from scipy.stats import gaussian_kde
from scipy.signal import argrelextrema
from itertools import combinations
from time import time

MAX_HEADMARKER_DIST = 25
SEGMENT_BINSIZE     = 0.25
VEC_NAN             = np.ones(3, float) *  np.nan
SEGM_MAX_ERROR      = 0.75
SEGM_MIN_FRAME_FRACTION = 0.25

DBG_MAX_PROCESSED_FRAMES = 10e6
DBG_MAX_PROCESSED_SEGM   = 10e6

# ========================================================
# Fit sphere
# ========================================================

# See: http://jekel.me/2015/Least-Squares-Sphere-Fit/
def fitHead(d):

    hm = pd.concat( [ d[['hml_x','hml_y','hml_z']], d[['hmr_x','hmr_y','hmr_z']] ], axis = 0)
    
    spX, spY, spZ = hm.ix[:,0], hm.ix[:,1], hm.ix[:,2]
    
    #   Assemble the A matrix
    spX = np.array(spX)
    spY = np.array(spY)
    spZ = np.array(spZ)
    A = np.zeros((len(spX),4))
    A[:,0] = spX*2
    A[:,1] = spY*2
    A[:,2] = spZ*2
    A[:,3] = 1
    
    #   Assemble the f matrix
    f = np.zeros((len(spX),1))
    f[:,0] = (spX*spX) + (spY*spY) + (spZ*spZ)
    
    # Remove NaN rows
    cnd = np.isnan(np.hstack((A,f))).any(axis=1)
    f = f[~cnd]
    A = A[~cnd]
    indices = [i for i in range(len(cnd)) if not cnd[i]]
        
    # Do fit
    C, residuals, rank, singval = np.linalg.lstsq(A,f)
    
    # Compute individual residuals
    res = np.dot(A, C) - f
    
    #   solve for the radius
    t = (C[0]*C[0])+(C[1]*C[1])+(C[2]*C[2])+C[3]
    radius = math.sqrt(t)
    
    cols = ['nj_x','nj_y','nj_z','head_rad','head_res_l','head_res_r']
    
    ncol = 5
    d  = np.full((len(cnd), ncol), np.nan)
    dd = np.hstack( (np.tile([C[0][0],C[1][0],C[2][0],radius], (len(res),1)), res) )
    d[indices,] = dd
    
    dn = np.full( (len(cnd)/2, len(cols)), np.nan )
    dn[:,0:5] = d[0:len(cnd)/2,:]
    dn[:,  5] = d[len(cnd)/2:,4]
    
    df = pd.DataFrame(dn, columns=cols )
        
    return df

# ========================================================
# Transform to Yframe space
# ========================================================

def transformToYframeSpace(v, vertices):
    a1  = vertices[1,:] - vertices[0,:]
    a2  = vertices[2,:] - vertices[0,:]
    #a2  = vertices[1,:] - 0.5 * (vertices[2,:] + vertices[0,:])
    a1 /= np.linalg.norm(a1)
    a2 /= np.linalg.norm(a2)
    a2  = np.cross(a1,a2)
    a3  = np.cross(a1,a2)
    m   = np.matrix([a1, a2, a3])
    vr  = v - vertices[1]
    tr  = np.asarray((m * np.matrix(vr).T).T)[0]
    return tr

# ========================================================
# Get segment lengths
# ========================================================

def getSegmentLengths(file):
    pairwiseDsts = []
    frameids = []
    print("Determining vertices (0)")
    for frame in readYFrames(file, nearbyVertexRange=MAX_HEADMARKER_DIST):
        # Report progress
        if (frame.frame%1e5)==0:
            print("Determining vertices ("+str(frame.frame)+")")
        # Is this frame in a perching range?
        frameids.append(frame.frame)
        for c in frame.nearbyVertices:
            # Compute all pairwise distances
            for i1 in range(len(frame.nearbyVertices)):
                for i2 in range(len(frame.nearbyVertices)):
                    if i1 > i2:
                        pairwiseDsts.append( np.linalg.norm(frame.nearbyVertices[i1] - frame.nearbyVertices[i2]) )
        if DBG_MAX_PROCESSED_SEGM != -1 and frame.frame > DBG_MAX_PROCESSED_SEGM:
            break #DEBUG!
    
    numframes = len(set(frameids))
    
    # Find mode of segment connecting the two head markers
    pw = np.array([x for x in pairwiseDsts if x < 10])
    hist = np.histogram(pw, bins=int(max(pw)/SEGMENT_BINSIZE))
    lm = np.array([0.5*(hist[1][i]+hist[1][i+2]) for i in \
        argrelextrema(hist[0], np.greater)[0] if hist[0][i]>SEGM_MIN_FRAME_FRACTION*numframes])
    
    print("Found the following segment lengths: ")
    print(','.join([str(a) for a in lm]))
    
    return { 'hml_hmr': lm[0] }

# ========================================================
# Get Yframe markers
#    o Note: This function automatically corrects 
#            mislabeled markers based on relative 
#            segment length.
# ========================================================

def getYframeMarkers(frame):
    yfs = [VEC_NAN, VEC_NAN, VEC_NAN]
    try:
        dsts  = sorted([np.linalg.norm(frame.vertices[i]-frame.vertices[j]) for i,j in combinations(range(3),2)])
        dsts2 = [[np.linalg.norm(frame.vertices[j]-frame.vertices[i]) for i in range(3)] for j in range(3)]
        c     = [set(np.where(np.in1d(dsts, dsts2[i]))[0]) for i in range(3)]
        yfl   = [x for x,y in zip(frame.vertices, c) if y==set([0,1]) ][0]
        yfc   = [x for x,y in zip(frame.vertices, c) if y==set([1,2]) ][0]
        yfr   = [x for x,y in zip(frame.vertices, c) if y==set([0,2]) ][0]
        yfv   = np.array([yfl, yfc, yfr])
        yfs   = [transformToYframeSpace(yfv[i], yfv) for i in range(3)]
    except Exception as e:
        pass
    return yfs[0], yfs[1], yfs[2]

# ========================================================
# Get head markers
# ========================================================

def getHeadMarkers(frame, segmLengths):
    m = []
    if len(frame.nearbyVertices) >= 2:
        for i1, i2 in combinations(range(len(frame.nearbyVertices)),2):
            e = float(abs(segmLengths['hml_hmr'] - \
                (np.linalg.norm(frame.nearbyVertices[i1] - frame.nearbyVertices[i2]))))
            
            if e < SEGM_MAX_ERROR:
                # Transform
                p1 = transformToYframeSpace(frame.nearbyVertices[i1], frame.vertices)
                p2 = transformToYframeSpace(frame.nearbyVertices[i2], frame.vertices)
                # Assign each to either 'left' head marker or 'right' head marker (left is more negative along X)
                hml = p1 if p1[0]<p2[0] else p2
                hmr = p2 if p1[0]<p2[0] else p1
                m.append( (e, hml, hmr) )
    
    return m

# ========================================================
# Process file
# ========================================================

def processFile(file):
    
    # Load perching data
    perches = pd.read_csv( file.replace('.msgpack','.perches.csv') )
    perches['numframes'] = pd.to_numeric(perches.numframes, errors='coerce')
    perches = perches[perches.numframes>800] # At least 4 seconds of perching
    
    # Output file
    ofName = file.replace('.msgpack', '.headmvmt.csv')
    
    # Compute distances
    segmLengths = getSegmentLengths(file)
    
    # Estimate of rows (rows are dynamically added and later removed)
    totalRows = 1000000
        
    # Init data table
    cols = [ \
        'frame','trajectory', \
        'ayfl_x','ayfl_y','ayfl_z','ayfc_x','ayfc_y','ayfc_z','ayfr_x','ayfr_y','ayfr_z', \
        'yfl_x','yfl_y','yfl_z','yfc_x','yfc_y','yfc_z','yfr_x','yfr_y','yfr_z', \
        'hml_x','hml_y','hml_z','hmr_x','hmr_y','hmr_z','hm_e_l','hm_e_h']
    d = pd.DataFrame(np.full((totalRows*5, len(cols)), np.nan), columns = cols)

    # Debug header
    dbgHeader = "["+file.replace('_Cortex.msgpack','')[0:30]+"] "
    
    # Now mark all segments with this length
    lastInfoTime = time()
    irow = 0
    for frame in util.readYFrames(file, nearbyVertexRange=MAX_HEADMARKER_DIST):
        # Print progress info
        if (time() - lastInfoTime) > 5.0:
            lastInfoTime = time()
            print( dbgHeader + "Processed "+str(irow)+"/"+str(totalRows)+\
                " frames (frame="+str(frame.frame)+")" )
        
        # Get Markers
        yfl, yfc, yfr = getYframeMarkers(frame)
        hms = getHeadMarkers(frame, segmLengths)
        
        # Save to dataset
        for hm in hms:
            e, hml, hmr = hm
            
            if irow >= d.shape[0]: 
                d2 = pd.DataFrame(np.full((totalRows, len(cols)), np.nan), columns = cols)
                d = pd.concat( (d, d2), axis=0 )
            
            d.iloc[irow] = [ frame.frame, frame.trajectory,] + \
                (frame.vertices[0]-frame.pos).tolist() + \
                (frame.vertices[1]-frame.pos).tolist() + \
                (frame.vertices[2]-frame.pos).tolist() + \
                yfl.tolist()+yfc.tolist()+yfr.tolist() + \
                hml.tolist()+hmr.tolist()+[e,float('NaN')]
            irow += 1
        
        if irow > DBG_MAX_PROCESSED_FRAMES:
            break

    d = d[pd.notnull(d.frame)]
    
    # Determine relative similarity to surrounding points
    #    Weighted minimum error to other frames in surrounding
    print("Determining segment anomalies.")
    cc = ['frame','hml_x','hml_y','hml_z','hmr_x','hmr_y','hmr_z']
    lastInfoTime = time()
    dnpt = d[cc].as_matrix()
    dnp  = np.full( (dnpt.shape[0], dnpt.shape[1]+1), np.nan)
    dnp[:,:-1] = dnpt
    wsl = 200
    wsr = 200
    for irow in range(wsl,dnp.shape[0]-wsr):
        if (time() - lastInfoTime) > 2.0:
            lastInfoTime = time()
            print( " Processed "+str(irow)+"/"+str(d.shape[0])+" rows for anomalies" )
        f = dnp[(irow-wsl):(irow+wsr),0].astype('int')
        e = np.linalg.norm(dnp[(irow-wsl):(irow+wsr),1:7] - dnp[irow,1:7], axis=1)
        dnp[irow,7] = np.nanmean(e) # sum(npg.aggregate(f,e, func='min')) # NOTE: It's more correct to do aggregate/min
                                                                          # first, but even just summing should highlight
                                                                          # segments that are rare (i.e. prob. error)
                                                                          # Also aggregation is _extremely_ slow for large data
    
    d['hm_e_h'] = dnp[:,7]
    
    # Record if this is segment is valid
    d['valid'] = (d['hm_e_h'] == d.groupby('frame')['hm_e_h'].transform('min'))
    
    # Determine head rotation
    a =  np.array([0.5 * (d['hml_x']+d['hmr_x']) - d['yfc_x'], \
                   0.5 * (d['hml_y']+d['hmr_y']) - d['yfc_y'], \
                   0.5 * (d['hml_z']+d['hmr_z']) - d['yfc_z']]).T
    a = [x/np.linalg.norm(x) for x in a]
    
    d['h_azm']  = [math.acos( np.dot( np.array([0, 1, 0]), np.array([x[0], x[1],   0 ])) ) for x in a]
    d['h_alt']  = [math.acos( np.dot( np.array([0, 0, 1]), np.array([  0 , x[1], x[2]])) ) for x in a]
    d['h_roll'] = [math.acos( np.dot( np.array([0, 0, 1]), np.array([x[0],   0 , x[2]])) ) for x in a]
    
    # Save intermediate results!
    d.to_csv( ofName )

# ========================================================
# Entry point
# ========================================================

def run(async=False):
    file = '2016-11-14 14-09-32_Cortex.msgpack'
    processFile(file)

if __name__ == "__main__":
    run()