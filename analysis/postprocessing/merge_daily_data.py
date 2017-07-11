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
import os, multiprocessing, shutil
import datetime, time
import sqlite3

DEBUG     = True
OVERWRITE = False
DEBUG_SELECTDIR = '17-07-04'

# =======================================================================================
# Process a given day
# =======================================================================================

def processFile(folderPrefix, dayParts):

    # For debug purposes, allow only certain dirs to be processed
    if DEBUG_SELECTDIR not in folderPrefix:
        return

    # Create the output folder
    os.makedirs(folderPrefix, exist_ok=True)

    fnameOutLog = folderPrefix + '/' + folderPrefix + '.log'
    fnameOut    = folderPrefix + '/' + folderPrefix + '.msgpack'

    if not OVERWRITE and os.path.exists(fnameOutLog) and os.path.exists(fnameOut):
        pass
    else:
        # If there is only one recording during this day, simply copy the data files to the new directory
        if len(dayParts) == 1:

            print("Merging daily files (single part, so simple copy): "+folderPrefix)

            part = dayParts[0]
            if not os.path.exists(fnameOut) or OVERWRITE:
                shutil.copyfile(part+'/'+part+'.msgpack', fnameOut)
            if not os.path.exists(fnameOutLog) or OVERWRITE:
                shutil.copyfile(part+'/'+part+'.log'    , fnameOutLog)
        else:
            print("Merging daily files: "+folderPrefix)

            # Join log file
            if not os.path.exists(fnameOutLog) or OVERWRITE:
                connOut = sqlite3.connect(fnameOutLog)
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
    folders.sort(key=lambda x: -os.path.getmtime(x))
    
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

        # Only add folders that are not actively being edited
        #    (i.e. that have been inactive for at least 5 min)
        secondsClosed = time.mktime(time.localtime()) - os.path.getmtime(folder)
        if secondsClosed < 10:
            # Don't process other folders on that day either...
            # The data for that day will be processed in one go, once it is done
            # If this behavior is not preferred, this line can be changed...
            byday[key] = None
            # Print status
            print("Skipping folder, active within last 10 seconds: " + folder)
        elif byday[key] is not None:
            byday[key].append(folder)
    
    # Process each day
    if not async:
        for day in byday:
            if byday[day] is not None:
                processFile(day, byday[day])
    else:
        with multiprocessing.Pool(processes=multiprocessing.cpu_count()) as pool:
            pool.starmap(processFile, [(day, byday[day]) for day in byday if byday[day] is not None])
    
    # Done!
            
if __name__ == '__main__':    
    run(None, async=(not DEBUG))