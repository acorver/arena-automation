#
# This script recomputes XYZ marker locations, as well as confidence intervals and recalibrations
#
#     Cortex appears to store 11 DLT coefficients, which can be easily transformed into U,V (i.e. camera X,Y) 
#     coordinates, as described here: http://www.kwon3d.com/theory/dlt/dlt.html
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

pass

# =======================================================================================
# Process a given day
# =======================================================================================

# TODO: Add function to util to iterate over all markers in frame (regardless of whether they 
# belong to a markerset or not)

def processFile(fname):
    
    cameras = list(range(1,1+18))
    
    for frame in util.MocapFrameIterator(fname):
        markers = util.getAllMarkersInFrame(frame)
        markersProjTo2D = {}
        for marker in markers:
            # Project the marker to XYZ coordinates for each camera
            for camID in cameras:
                markersProjTo2D[camID] = None # TODO
            

# =======================================================================================
# Run script
# =======================================================================================

def run(async=True):
    pass
            
if __name__ == '__main__':    
    run()
