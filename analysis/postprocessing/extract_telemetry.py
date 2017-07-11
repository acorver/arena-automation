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

import pandas as pd, numpy as np, scipy.io
import re, datetime, ntpath

TELEMETRY_DIR = 'Z:/data/dragonfly/telemetry/'

MAX_TRIGGER_MISMATCH_MOCAP_TELEMETRY = 1.0 # In seconds

CORTEX_FPS = 200

# Switch for debugging purposes: force reconsruction of ephys, even if a saved reconstruction
# already exists for that file.
RE_RECONSTRUCT = True

# =======================================================================================
# Process telemetry file and align with mocap
# =======================================================================================

def processTelemetry(data, fname, fnameOut):

    fnameLedTriggersFinal = fname.replace('.raw.msgpack', '').replace('.msgpack', '') + '.led_triggers'

    # Read triggers
    triggers = pd.read_csv(fnameLedTriggersFinal)

    # Align mocap triggers and frame data!
    #   - For each telemetry file, find the trigger that happened most closely

    data.loc[:,'frameID'] = np.NaN

    for fileEphys in data.fileEphys.unique():

        triggerEphys = data[(data.fileEphys == fileEphys) & (data.time == 0)]

        if len(triggerEphys) > 0:
            minIdx = np.argmin(np.abs(triggers.timestamp - int(triggerEphys.systemClock.iloc[0])))

            triggerMocap = triggers.iloc[minIdx]

            timeMismatch = int(abs(triggerMocap.timestamp - triggerEphys.systemClock))

            if timeMismatch > MAX_TRIGGER_MISMATCH_MOCAP_TELEMETRY:
                print("No match found for telemetry trigger at time " + str(int(triggerEphys.systemClock)) +
                      "(Closest mocap trigger time mismatch = " + '{:.4f}'.format(timeMismatch) + " s)")
            else:
                print("Merging ephys and mocap files at trigger time: " + str(int(triggerEphys.systemClock)))
                data.loc[data.fileEphys == fileEphys, 'frameID'] = triggerMocap.frameID + \
                                                                   (data.time[
                                                                        data.fileEphys == fileEphys] - triggerMocap.time) * (
                                                                   CORTEX_FPS / 1000.0)

    # TODO: Search for conflicting matches... i.e. double-used, or non-used mocap triggers

    # Done!
    data.to_csv(fnameOut)

    # Write to Matlab-compatible file
    # a_dict = {col_name: data[col_name].values for col_name in data.columns.values}
    # a_dict[data.index.name] = data.index.values
    # scipy.io.savemat(fnameOut.replace('.csv','.mat'), {'struct': a_dict})

# =======================================================================================
# Process telemetry directory
# =======================================================================================

def processFile(fname):

    fnameOut = fname.replace('.raw.msgpack', '').replace('.msgpack', '') + '.telemetry.csv'

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
                      in os.listdir(telemetryCapDir) if x.endswith('.meta') and not x.endswith('all.meta')]

    # Process each .meta file, and merge the data into a single table
    dataMerged = []
    i = 0
    for metaFile in metaFiles:
        print("Processing telemetry: " + metaFile)
        d = telemetry.reconstructEphys(metaFile, saveToFile=True, ignoreSavedFile=RE_RECONSTRUCT)
        d.loc[:,'fileEphys'] = ntpath.basename(metaFile)
        if d is not None:

            # Update merged dataset
            if dataMerged is None:
                dataMerged = d
            else:
                dataMerged = dataMerged.append(d)
            
            # Create output filename
            fn = ''
            try:
                fn = re.search('([0-9]+)\.meta', metaFile).groups(0)[0]
            except:
                fn = str(i).zfill(5)
                i += 1
            fn = fnameOut.replace('.telemetry.csv', '_' + fn + '.telemetry.csv')

            # Process/align
            processTelemetry(d, fname, fn)

    if dataMerged is not None:
        processTelemetry(dataMerged, fname, fnameOut)

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
