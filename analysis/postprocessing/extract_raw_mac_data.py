#
# This script extracts raw MAC camera data (in .VC files)
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

import numpy as np
import struct
from shared import util
import msgpack, json
import sqlite3
import subprocess
import io
import datetime, time
from multiprocessing import Pool
from shared import util

# Directory to search for Cortex data
CORTEX_DIR = 'Z:/data/dragonfly/motion capture/MoCap raw'

# File to debug
DEBUG_FILE = 'Z:/people/Abel/arena-automation/data/2017-03-28 09-41-53/2017-03-28 09-41-53.msgpack'

# 
CONVERTER_EXE = os.path.abspath(os.path.join( 
    os.path.dirname(os.path.abspath(__file__)),'../bin/cortex-raw-utils.exe'))

# =======================================================================================
# Convert entire cortex directory to SQLite and/or csv file
# =======================================================================================

def openVCDB(outputDir):
    dbFile = os.path.join(outputDir, 'vc.sqlite')
    existed = os.path.isfile(dbFile)
    # We allow a high timeout (2 minutes), because multiple streams will be locking the database and 
    # writing in turn...
    conn = sqlite3.connect(dbFile, timeout=120)
    c = conn.cursor()
    return conn, c, existed

def extractVCFile(capFileIdx, capFile, outputDir):
    # Find raw camera data filename
    rawCameraDataFile = ''
    with open(capFile,'r') as f:
        tmp = f.read()
        i1 = tmp.find('RawCameraData=')+len('RawCameraData=')
        n = tmp[i1:tmp.find('\n', i1)]
        if n != '':
            rawCameraDataFile = os.path.join(os.path.dirname(capFile), n)
        
    # Find start frame
    startFrame = -1
    startTime = None
    timecodeFile = capFile.replace('.cap','.tc')
    
    # Some files are missing .tc files (it appears to be safe to skip these files)
    if not os.path.exists(timecodeFile):
        return

    with open(timecodeFile, 'r') as f:
        tmp = f.read().split('\n')
        date = datetime.datetime.utcfromtimestamp(os.path.getmtime(
            timecodeFile)).strftime('%Y-%m-%d ')
        startTime = datetime.datetime.strptime(date + tmp[0], '%Y-%m-%d %H:%M:%S:00')
        startFrame = int(tmp[3])

    # Skip invalid files
    if rawCameraDataFile == '' or not os.path.exists(rawCameraDataFile):
        return
        
    # Try all cameras
    sqliteCache = []
    for camID in range(1,1024):
        # Construct the actual filename for each camera
        vcFile = rawCameraDataFile.replace('.vc1','.vc'+str(camID))
            
        # Stop this loop if we processed all cameras
        if not os.path.isfile(vcFile):
            break
            
        # Use external C application to convert raw VC files to .json format
        rawCameraDataFileConverted = vcFile + '.json'
        try:
            subprocess.check_output([CONVERTER_EXE, vcFile, rawCameraDataFileConverted])
        except:
            print("Error converting file: " + vcFile)
        
        # Read this json data
        data = None
        with open(rawCameraDataFileConverted,'r') as f:
            data = json.load(f)
            
        # delete the temporary .json file
        os.remove(rawCameraDataFileConverted)

        # Loop through each frame
        for x in data:
            # Estimate time that this frame arrived... This is very approximate (could be e.g. many seconds off), but should be    
            # precise enough to disambiguate identical frameIDs that correspond to different re-starts.
            estimatedTime = startTime + datetime.timedelta( 
                seconds = x["frame"] / x["fps"] )
            estimatedTime = time.mktime(estimatedTime.timetuple())
                
            # Add to database (TODO: Is the "-1" in frame computation correct?)
            sqliteCache.append( [startFrame + x["frame"] - 1, estimatedTime, camID, json.dumps(x)] )
                
            
        # Status update
        print("Converted capture file "+str(capFileIdx)+" cam file "+str(camID))

    # Commit all data to DB
    conn, c, dbExisted = openVCDB(outputDir)
    c.executemany('insert into vc (frameID, timestampPOSIX, cameraID, centroids) values (?,?,?,?)', sqliteCache)
    conn.commit()
    conn.close()

def extractVC(directoryPath, outputDir):
    
    # If the DB was just created, initialize it
    conn, c, dbExisted = openVCDB(outputDir)
    if not dbExisted:
        c.execute('''CREATE TABLE vc
                     (frameID integer, timestampPOSIX integer, cameraID integer, centroids TEXT)''')
        c.execute('CREATE INDEX i1 ON vc (frameID)')
        c.execute('CREATE INDEX i2 ON vc (timestampPOSIX)')
        c.execute('CREATE INDEX i3 ON vc (cameraID)')
        conn.commit()
    conn.close()

    # Get a list of all capture files
    cortexCapFiles = [os.path.join(directoryPath,x) for x in \
        os.listdir(directoryPath) if x.endswith('.cap')]
    
    # Now process each capture file
    params = [[i,f,outputDir] for i,f in zip(range(len(cortexCapFiles)), cortexCapFiles, )]
    with Pool() as pool:    
        pool.starmap(extractVCFile, params)
    
    # Done!
    return

# =======================================================================================
# Process log
# =======================================================================================

def processFile(dataFile):
    
    # Get time the current file was modified...
    ctime = datetime.datetime.fromtimestamp(os.path.getmtime( 
        dataFile)).strftime('%Y-%m-%d')
    
    # Get the relevant cortex data directories
    cortexCapDirs = [os.path.join(CORTEX_DIR,y) for y in os.listdir(CORTEX_DIR) if \
            os.path.isdir(os.path.join(CORTEX_DIR,y)) and \
                datetime.datetime.fromtimestamp(os.path.getctime(os.path.join(
                    CORTEX_DIR,y))).strftime('%Y-%m-%d') == ctime]

    # Produce converted files
    if not os.path.exists(os.path.join(os.path.dirname(dataFile), 'vc.sqlite')):
        for cortexCapDir in cortexCapDirs:
            extractVC(cortexCapDir, os.path.dirname(dataFile))
    
    # Open output database
    conn, c, _ = openVCDB(os.path.dirname(dataFile))
    
    # Determine total number of frames to process
    totalToProcess = util.countRecords(dataFile)
    
    # Now loop through the frames received in the file, and find their corresponding raw camera files
    counter = 0
    with open(dataFile,'rb') as fIn:
        with open(dataFile.replace('.msgpack','.raw.msgpack'),'wb') as fOut:
            for frame in msgpack.Unpacker(fIn):
                # Get frame ID
                frameID = frame[0]
                # Get time (in seconds, hence divide by 1000)
                frameTime = frame[5] / 1000
                # Query the raw data
                results = [x for x in c.execute( 
                    'select frameID, cameraID, timestampPOSIX, centroids from vc where '+
                    'frameID=? ORDER BY timestampPOSIX-? DESC', 
                        [frameID, frameTime])]
                
                rawData = [] 
                if len(results) > 0:
                    # Only keep the data with the closest matching frame
                    # Note that the resultset is already ordered by the SQL query
                    nresults = {}
                    for result in results:
                        camID = result[1]
                        if not camID in nresults:
                            nresults[camID] = result
                        else:
                            pass # Good point to put breakpoint to debug 
                                 # how frameID conflicts are resolved
                    # Process each result
                    for camID, result in nresults.items():
                        js = json.loads(result[3])
                        rawData.append([
                            camID,
                            js["width"],
                            js["height"],
                            [[c["x"], c["y"], c["q"]] for c in js["centroids"]]
                        ])
                    
                # Now, *finally* assign the centroids to the frame (we copy the original dataset, in 
                # order to get a complete reference.)
                newframe = frame + [rawData,]
                msgpack.dump(newframe, fOut)
                
                # Output debug info
                if (counter%10000) == 0:
                    print("Gathered raw frame data for "+str(counter)+"/"+str(totalToProcess)+" frames ")
                counter += 1
    # Done!
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
