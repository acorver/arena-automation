
#
# This script will watch for new data files, and automatically start processing all relevant data
#

# Imports
import os, sched, time, update_all
from shared import util

# Time to wait between updates (in seconds)
WATCH_INTERVAL = 60 * 5

# Only auto-process files that are a maximum of N days old:
MAX_AUTO_UPDATE_AGE = 3600 * 24 * 3

# Only auto-process files that have been unchanged for this many minutes:
MIN_AUTO_UPDATE_AGE = 3600 * 0.5

# Enforce a minimal file-size, in order to not clutter the data space with 
# either erroneously created raw data files, or very quickly aborted sequences
# (in megabytes)
MIN_RAW_DATA_SIZE = 100

# schedule helper function
s = sched.scheduler(time.time, time.sleep)
def scheduleNext(s):
    s.enter(WATCH_INTERVAL, 1, checkFiles, (s,))

# TODO: Run this app in the tray!

# Age of file (w.r.t. last modified) in seconds
def fileAge(f):
    ft = time.time() - os.path.getmtime(f)
    return ft

# Size of file (in MB)
def fileSize(f):
    return os.path.getsize(f) / (1024 * 1000)

# 
def checkFiles(s):
    
    # Gather a list of data files
    files = [x for x in os.listdir('./') if x.endswith('.msgpack')]
    
    # Process newest files first
    files.sort(key=lambda x: -os.path.getmtime(x))
    
    # Keep only files within the minimum-maximum age range
    files = [x for x in files if fileAge(x) > MIN_AUTO_UPDATE_AGE and \
        fileAge(x) < MAX_AUTO_UPDATE_AGE and fileSize(x) > MIN_RAW_DATA_SIZE]
    
    # Process?
    if len(files) > 0:
        # Create a settings object
        settings = util.ExtractionSettings(files, False)
        
        # Re-import any python libraries, in case this script is run over long durations, 
        # and any post-processing code changes
        # ... TODO ...
        
        # Update all postprocessing data for these files!
        # Note: We make sure this auto-updater is immune to any accidental 
        #       errors introduced in other scripts, so it can keep running
        try:
            update_all.updateAll(settings)
        except Exception as e:
            print(e)
            print("Ignoring error... Continuing auto-updater...")
    
    # Re-schedule file check
    scheduleNext(s)

#
if __name__ == "__main__":
    
    print("Starting auto-updater...")

    # On startup, do an immediate check
    # Note: This also schedules the next check
    checkFiles(s)
    
    # Now run the scheduler
    s.run()

    print("Stopping auto-updater...")    