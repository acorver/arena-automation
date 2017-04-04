#
# This script discovers the exact timing of the hardware trigger, making it easy 
# to automatically align Photron, MAC and telemetry data.
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
# Imports and global constants for this script
# =======================================================================================

DEBUG_FILE = 'Z:/people/Abel/arena-automation/data/2017-03-28 09-41-53/2017-03-28 09-41-53.raw.msgpack'

from shared import util

# =======================================================================================
# Process file
# =======================================================================================

def processFile(dataFile):
    for frame in util.iterMocapFrames(dataFile):
        pass

# =======================================================================================
# Entry point
# =======================================================================================

def run(async=False, settings = None):    
    
    # ...
    if settings == None:
        if DEBUG_FILE == '' :
            settings = util.askForExtractionSettings()
        else:
            settings = util.ExtractionSettings([DEBUG_FILE,], False)

    # Process all log files:
    for file in settings.files:
        if DEBUG_FILE == '':
            try:
                processFile(file)
            except Exception as e:
                print(e)
        else:
            processFile(file)

if __name__ == "__main__":
    run()
