#
# This script extracts telemetry data from .bin/.bug3/.meta files, and converts it to
# .csv data
#

# =======================================================================================
# Change working directory so this script can be run independently as well as as a module
# =======================================================================================

import os, sys

if __name__ == "__main__":
    p = os.path.dirname(os.path.abspath(__file__))
    sys.path.insert(0, os.path.join(p, '../'))
    os.chdir(p)

# =======================================================================================
# Imports and global constants for this script
# =======================================================================================

from shared import util
from shared import telemetry

import pandas as pd
import re, datetime

TELEMETRY_DIR = 'Z:/data/dragonfly/telemetry/'

# =======================================================================================
# Process telemetry directory
# =======================================================================================

def processFile(fname):

    fnameLedTriggersFinal = fname.replace('.raw.msgpack', '').replace('.msgpack', '') + '.led_triggers'

    # Get mocap data alignment
    triggers = pd.read_csv(fnameLedTriggersFinal)

    # Get time the current file was modified...
    ctime = re.search('[0-9]{4}-[0-9]{2}-[0-9]{2}', fname).group(0)

    # Get the relevant cortex data directories
    telemetryCapDirs = [os.path.join(TELEMETRY_DIR,y) for y in os.listdir(TELEMETRY_DIR) if \
            os.path.isdir(os.path.join(TELEMETRY_DIR,y)) and \
                datetime.datetime.fromtimestamp(os.path.getctime(os.path.join(
                    TELEMETRY_DIR,y))).strftime('%Y-%m-%d') == ctime]


    # Loop through all .meta files in directories
    metaFiles = []
    for telemetryCapDir in telemetryCapDirs:
        metaFiles += [os.path.join(telemetryCapDir, x) for x
                      in os.listdir(telemetryCapDir) if x.endswith('.meta')]

    # Process each .meta file, and merge the data into a single table
    data = None
    for metaFile in metaFiles:
        print("Processing telemetry: " + metaFile)
        d = telemetry.reconstructEphys(metaFile, saveToFile=True)
        if d is not None:
            if data is None:
                data = d
            else:
                data = data.append(d)

    # Align mocap triggers and frame data!
    pass

    # Save to disk!
    pass

    # Done!
    pass

# =======================================================================================
# Entry point
# =======================================================================================

def run(async=False, settings=None):

    # ...
    if settings == None:
        settings = util.askForExtractionSettings()

    # Process all log files:
    for file in settings.files:
        processFile(file)

if __name__ == "__main__":
    run()
