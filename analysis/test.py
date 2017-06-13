import numpy as np

import os
os.chdir('Z:/people/Abel/arena-automation/analysis')
from shared import util

fname = 'Z:/people/Abel/arena-automation/data/2017-05-27 15-22-56/2017-05-27 15-22-56.raw.msgpack'

frames = list(util.MocapFrameIterator(fname, numFrames=100))

centroids = {}
for frame in frames:
    for camID in frame.centroids:
        if not camID in centroids:
            centroids[camID] = []
        for fc in frame.centroids[camID].centroids:
            fca = [np.array( [fc.x, fc.y] ),]
            # find closest centroid
            foundMatch = False
            for i in range(len(centroids[camID])):
                if np.mean([np.linalg.norm(c-fca[0]) for c in list(reversed(centroids[camID][i]))[0:10]]) < 20:
                    foundMatch = True
                    centroids[camID][i] += fca
                    break
            if not foundMatch:
                centroids[camID].append( fca )

pass
