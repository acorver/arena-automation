#
# This script merges all .msgpack files from a given day into a single file
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

import msgpack
import os, multiprocessing
import datetime
import sqlite3

OVERWRITE = False

# =======================================================================================
# Process a given day
# =======================================================================================

def processFile(folderPrefix, dayParts):
    
    # Create the output folder
    os.makedirs(folderPrefix, exist_ok=True)
    
    # Join log file
    fnameOut = folderPrefix+'/'+folderPrefix+'.log'
    if not os.path.exists(fnameOut) or OVERWRITE:
        connOut = sqlite3.connect(fnameOut)
        cOut = connOut.cursor()
        cOut.execute('create table if not exists PLOG (id INTEGER PRIMARY KEY AUTOINCREMENT, timestamp DATETIME, msg TEXT)')
        for part in dayParts:
            fnameIn = part+'/'+part+'.log'
            if os.path.exists(fnameIn):
                conn = sqlite3.connect(fnameIn)
                c = conn.cursor()
                for row in c.execute('select timestamp,msg from PLOG'):
                    cOut.execute('insert into PLOG (timestamp,msg) values (?,?)', (row[0], row[1]))
                connOut.commit()
                conn.close()
        connOut.close()

    # Consolidate the MSGPACK files
    fnameOut = folderPrefix+'/'+folderPrefix+'.msgpack'
    if not os.path.exists(fnameOut) or OVERWRITE:
        with open(fnameOut,'wb') as fOut:
            for part in dayParts:
                fnameIn = part+'/'+part+'.msgpack'
                if os.path.exists(fnameIn):
                    with open(fnameIn,'rb') as fIn:
                        unpacker = msgpack.Unpacker(fIn)
                        for data in unpacker:
                            fOut.write(msgpack.packb(data))

# =======================================================================================
# 
# =======================================================================================

def run(settings, async=True):
    
    # Get a list of folders
    folders = [x for x in os.listdir('./') if os.path.isdir(x)]
    
    # Remove folders that don't match the patterns
    def _isdated(x):
        try:
            return True if datetime.datetime.strptime(x, '%Y-%m-%d %H-%M-%S') != None else False
        except:
            return False
    
    folders = [x for x in folders if _isdated(x)]
    
    # Now assign each folder to a date
    byday = {}
    for folder in folders:
        key = datetime.datetime.strftime(datetime.datetime.strptime(
            folder, '%Y-%m-%d %H-%M-%S'), '%Y-%m-%d')
        if not key in byday:
            byday[key] = []
        byday[key].append(folder)
    
    # Process each day
    if not async:
        for day in byday:
            processFile(day, byday[day])
    else:
        with multiprocessing.Pool(processes=multiprocessing.cpu_count()) as pool:
            pool.starmap(processFile, [(day, byday[day]) for day in byday])
    
    # Done!
            
if __name__ == '__main__':    
    run()