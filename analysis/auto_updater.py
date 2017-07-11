
#
# This script will watch for new data files, and automatically start processing all relevant data
#

# Imports
import os, sched, time, update_all
from shared import util

# Time to wait between updates (in seconds)
WATCH_INTERVAL = 60 * 5

# Only auto-process files that are a maximum of N days old:
MAX_AUTO_UPDATE_AGE = 3600 * 24 * 14

# Only auto-process files that have been unchanged for this many minutes:
MIN_AUTO_UPDATE_AGE = 3600 * 0.5

# Enforce a minimal file-size, in order to not clutter the data space with 
# either erroneously created raw data files, or very quickly aborted sequences
# (in megabytes)
MIN_RAW_DATA_SIZE = 10

# schedule helper function
s = sched.scheduler(time.time, time.sleep)
def scheduleNext(s):
    s.enter(WATCH_INTERVAL, 1, checkFiles, (s,))

# TODO: Run this app in the tray!

# 
def checkFiles(s):
    
    # Gather a list of data files
    files = util.getRecentFiles(MAX_AUTO_UPDATE_AGE, MIN_AUTO_UPDATE_AGE, MIN_RAW_DATA_SIZE)

    # Process?
    if len(files) > 0:
        for file in files:
            # Create a settings object
            settings = util.ExtractionSettings([file,], False)

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