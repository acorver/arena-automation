
#
# 
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

import os, sys, dateutil, datetime, winshell
import pandas as pd

# Global Variables
VIDEO_SAVING_DELAY_SEC = 60
VIDEO_SAVING_DELAY_MAX = 300
VIDEO_DIRECTORY        = 'Z:/data/dragonfly/behavior video/'

videos = []

# =======================================================================================
# ...
# =======================================================================================

def processTrajectory(file, takeoffTrajectory, dataTakeoff):
    
    # Time:
    takeoffTime = dateutil.parser.parse(dataTakeoff.time)
    
    # Find closest time
    v = [(video[1], (video[0]-takeoffTime).seconds) for video in videos if (video[0]-takeoffTime).seconds>0 and video[0].date()==takeoffTime.date()]
    if len(v)==0: return
    
    m = min(v, key=lambda x: x[1])
    filelink = m[0]
    timedelta = m[1]
    
    if timedelta < VIDEO_SAVING_DELAY_MAX:
        # ...
        for subpath in ['takeoff','flysim']:
            f = './output/' + file + '/' + subpath + '/' + str(takeoffTrajectory) + '.mp4'
            if os.path.exists(f):
                with winshell.shortcut(filelink) as link:
                    link.description = "Shortcut to "+filelink
                    link.write( os.path.abspath( f.replace('.mp4','.lnk') ) )

# =======================================================================================
# ...
# =======================================================================================

def processFile(file):

    fname = file+'_Cortex.takeoffs.csv'    
    if not os.path.isfile(fname): return
    
    dataTakeoff = pd.read_csv(fname)
    dataTakeoff = dataTakeoff.rename(columns=lambda x: x.strip())
    
    # Only process takeoffs with a minimum amount of perching preceding it
    dataTakeoff = dataTakeoff[dataTakeoff.perchframes>=200]
    
    for index, row in dataTakeoff.iterrows():
        processTrajectory(file, index, row)

# =======================================================================================
# 
# =======================================================================================

def findHighspeedVideos():
    for root, dirs, files in os.walk(VIDEO_DIRECTORY):
        for file in files:
            if file.endswith('.avi'):
                absfile = root+'/'+file
                t = datetime.datetime.fromtimestamp(os.path.getctime(absfile))
                videos.append( (t, absfile) )

# =======================================================================================
# ...
# =======================================================================================

def run(async=False, settings=None):
    
    # Search videos
    findHighspeedVideos()
    
    # Find high speed videos
    files = [x for x in os.listdir('../output')]
    for file in files:
        if os.path.isdir('../output/'+file):
            processFile(file)

# =======================================================================================
# Entry point for standalone execution
# =======================================================================================

if __name__ == "__main__":
    run()
