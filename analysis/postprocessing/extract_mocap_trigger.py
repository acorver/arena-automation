#
#
#

DEBUG = True
DEBUG_STARTFRAME = 0
DEBUG_CAMS = list(range(18))

# =======================================================================================
# Change working directory so this script can be run independently as well as as a module
# =======================================================================================

import os, sys
if __name__ == "__main__":
    p = os.path.dirname(os.path.abspath(__file__))
    sys.path.insert(0, os.path.join(p,'../'))
    os.chdir(p)

# =======================================================================================
# Imports and global constants for this script
# =======================================================================================

import multiprocessing
from time import sleep
import pandas as pd, numpy as np
from ggplot import *
import matplotlib.pyplot as plt
import scipy.misc
from shared import util
import re, datetime
import traceback
import seaborn as sns

# =======================================================================================
# Find number of available cameras
# =======================================================================================

def getMaxCamID(fname):
    maxCamID = 0
    i = 0
    for frame in util.iterMocapFrames(fname):
        if len(frame.centroids.keys()) > 0:
            maxCamID = max(maxCamID, max(frame.centroids.keys()))
        i += 1
        if i > 5e3:
            break
    return maxCamID

# =======================================================================================
# Find triggerbox markers in a given frame
# =======================================================================================

def findTriggerBoxInFrame(frame, camID, spacing=None, debug=True):
                
    # Result variables
    centroids = []
    if camID in frame.centroids:
        for c in frame.centroids[camID].centroids:
            npt = np.array([c.x, c.y])
            if len(centroids)==0 or min([np.linalg.norm(npt-c2) for c2 in centroids]) >= 0:
                centroids.append( npt )
    centroids = np.array(centroids)

    # Find co-linear, equi-distantly spaced triplet of centroids
    syncbox = []
    for i1,c1 in zip(range(len(centroids)),centroids):
        for i2,c2 in zip(range(len(centroids)),centroids):
            for i3,c3 in zip(range(len(centroids)),centroids):
                if i2 > i1 and i3 > i2:
                    a = c1 - c2
                    b = c2 - c3 
                    c = c1 - c3 
                    colinearity = abs(np.dot( a, b ) * np.dot( b, c ) / 
                      (np.linalg.norm(a)*np.linalg.norm(b)*np.linalg.norm(b)*np.linalg.norm(c)))
                    d1 = np.linalg.norm(b-a)
                    d2 = np.linalg.norm(c-b)
                    d3 = np.linalg.norm(c-a)
                    maxdist = np.max([d1,d2,d3])
                    if colinearity > 0.9 and maxdist < 200:
                        na = np.linalg.norm(a)
                        nb = np.linalg.norm(b)
                        syncbox.append({'c1': c1, 'c2': c2, 'c3': c3, 'frame': frame.frameID, 
                                        'spacing': 0.5 * (na + nb), 'timestamp': frame.time, 
                                        'camID': camID, 'equidist': max(na/nb,nb/na)})
    
    if len(syncbox) > 0:
        # Did we specify a known spacing? If so, sort by spacing, using a reasonable equidist threshold
        if spacing != None:
            syncbox = [x for x in syncbox if x['equidist'] < 1.3]
            syncbox = sorted(syncbox, key=lambda x: abs(spacing-x['spacing']))
            if len(syncbox) > 0:
                
                # Save original syncbox ranking as debug info
                if debug:
                    syncbox[0]['ranking'] = syncbox
                
                # Choose top one
                syncbox = syncbox[0]
            
                # Now check if triggers occurred
                additionalPts = []
                for i,c in zip(range(len(centroids)),centroids):
                    syncboxPts = None
                    try:
                        syncboxPts = [syncbox[m] for m in ['c1','c2','c3']]
                    except Exception as e:
                        traceback.print_exc()
                    mindist = np.min([np.linalg.norm(c-c2) for c2 in syncboxPts+additionalPts])
                    if mindist > 5:
                        colinearity = []
                        for sbp in [(syncboxPts[0],syncboxPts[1]),(syncboxPts[0],syncboxPts[2]),(syncboxPts[1],syncboxPts[2])]:
                            v1 = sbp[0] - c
                            v2 = c - sbp[1]
                            v3 = sbp[0] - sbp[1]
                            colinearity.append(np.abs(np.dot(v1, v2) * np.dot(v2, v3) * np.dot(v1, v3) /
                                (np.linalg.norm(v1) * np.linalg.norm(v2) *
                                 np.linalg.norm(v2) * np.linalg.norm(v3) *
                                 np.linalg.norm(v1) * np.linalg.norm(v3))))

                        isColinear = min(colinearity) > 0.95
                        isNearby = mindist < 0.7 * spacing

                        if isNearby and isColinear:
                            additionalPts.append(c)
                            syncbox['colinearity'] = [(mindist, colinearity), ] + (
                                [] if not 'colinearity' in syncbox else syncbox['colinearity'])

                syncbox['additionalPts'] = additionalPts
            else:
                syncbox = None
        else:
            syncbox = sorted(syncbox, key=lambda x: -x['equidist'])
            syncbox = syncbox[0]
            syncbox['additionalPts'] = []
    else:
        syncbox = None        
    
    # Add all centroids (useful for debugging)
    if syncbox != None and debug:
        syncbox['allcentroids'] = centroids
    
    # Triggered?
    triggered = False
    if syncbox != None:
        if len(syncbox['additionalPts']) >= 3:
            triggered = True

    # Done!
    return syncbox, triggered

# =======================================================================================
# Find triggerbox points in all frames
# =======================================================================================

def findTriggerBoxInFrames(fname, camID, startFrame=None, numFrames=None):

    i = -1
    numFramesProcessed = 0

    for frame in util.iterMocapFrames(fname, startFrame=startFrame, numFrames=100):

        if camID not in frame.centroids:
            continue

        tb, triggered = findTriggerBoxInFrame(frame, camID, spacing=38)

        if tb != None:
            tbc = []
            for m in ['c1','c2','c3']:
                tbc.append([tb['camID'],tb['frame'],tb[m][0],tb[m][1],'syncbox'])
            for c in tb['additionalPts']:
                tbc.append([tb['camID'],tb['frame'],c[0],c[1],'additionalpt'])
            for c in tb['allcentroids']:
                notYetIncluded = True
                for pt in [tb[m] for m in ['c1','c2','c3']] + tb['additionalPts']:
                    if np.linalg.norm(pt - c) < 1:
                        notYetIncluded = False
                        break
                if notYetIncluded:
                    tbc.append([tb['camID'],tb['frame'],c[0],c[1],'all'])

            yield tbc
            i += 1

        numFramesProcessed += 1
        if numFrames is not None and numFramesProcessed >= numFrames:
            break

# =======================================================================================
# Find triggers in parallel
# =======================================================================================

def findTriggerAsync(frame, camID):
    try:
        syncbox, triggered = findTriggerBoxInFrame(frame, camID, spacing=38)

        timestamp_str = ''
        if syncbox is not None:
            timestamp_str = str(datetime.datetime.fromtimestamp(syncbox['timestamp']/1000))

        if not DEBUG:
            if triggered:
                return 'trigger', syncbox['camID'], syncbox['frame'], syncbox['timestamp'], timestamp_str
            else:
                return None
        else:
            # Output more extensive debug info
            sb = []
            if syncbox is not None:
                for c in ['c1','c2','c3']:
                    sb.append(syncbox[c][0])
                    sb.append(syncbox[c][1])

            if len(sb) == 6:
                return tuple( ['trigger' if triggered else 'debug', triggered,
                               len(syncbox['additionalPts']),
                               camID, frame.frameID, syncbox['timestamp'], timestamp_str] + sb )
            else:
                return None # Don't print info for frames with no Syncbox found
    except Exception as e:
        if DEBUG:
            raise
        return e
       
# Worker function    
def findTriggers_Worker(tasks, output, async=True):
    while True:
        taskList = tasks.get()
        for frame, camIDs in taskList:
            if not frame is None:
                for camID in camIDs:
                    r = findTriggerAsync(frame, camID)
                    if isinstance(r, Exception):
                        print(r)
                    elif r != None:
                        # When a trigger was detected write the frame number and camera ID to file
                        output.put(','.join([str(y) for y in r]))
        tasks.task_done()
        output.put('DONE')
        # If async is false, we break the for loop, so control goes back to findTriggers while loop
        if not async:
            break

def findTriggers(fname, camIDs, async=True, startFrame=None, useLog=True):

    fnameLedTriggers = fname.replace('.raw.msgpack','').replace('.msgpack','')+'.led_triggers_tmp'

    # Don't process if file already exists
    if os.path.exists(fnameLedTriggers):
        print("Skipping trigger search, file already exists: " + fnameLedTriggers)
        return

    # Number of CPUs to use
    NUM_CPUS = max(1, multiprocessing.cpu_count() - 1)
    
    # Get total number of frames
    totalNumFrames = util.countRecords(fname)

    tasks = multiprocessing.JoinableQueue()
    output = multiprocessing.Queue()

    # Create worker threads
    pool = None
    if async:
        pool = multiprocessing.Pool(NUM_CPUS, findTriggers_Worker, (tasks, output))

    # Optionally, use log to look only at frames with known trigger point
    times = None
    if useLog:
        log = pd.read_csv(fname.replace('.raw.msgpack','').replace('.msgpack','')+'.log.txt',sep='\t', header=None, names=['i','time','msg'])
        log = log[log.msg.str.contains('Sent trigger')]
        times = sorted(log.time.tolist())
        print("Number of triggers sent: "+str(len(times)))
        for time in times:
            print("Looking for trigger around t="+str(time))

    # Open frame iterator
    mocap = None
    if not useLog:
        # Create an iterator over MoCap frames
        mocap = util.MocapFrameIterator(fname, startFrame=startFrame)

    # Start queueing frames
    numFramesProcessed = 0
    numBusy = 0
    with open(fnameLedTriggers,'w') as fOut:
        if not DEBUG:
            fOut.write('type,camID,frame,timestamp,timestamp_str\n')
        else:
            fOut.write('type,triggered,additionalPts,camID,frame,timestamp,timestamp_str,x1,y1,x2,y2,x3,y3\n')
        framesLeft = True
        while True:
            numFramesProcessed += 1000
            # We send frames to be processed in batches, which should speed up processing
            if framesLeft:
                newTasks = []
                if useLog:
                    if len(times) > 0:
                        time = times.pop()
                        newTasks = [(f, camIDs) for f in util.MocapFrameIterator(fname,
                           startTime=time - 10000, numFrames=20 * 200)]
                        print("Processing frame range (time="+str(time)+"): ["+str(newTasks[0][0].frameID)+","+str(newTasks[-1][0].frameID)+"]")
                    else:
                        framesLeft = False
                        #for i in range(NUM_CPUS):
                        #    tasks.put('NONE_LEFT')
                else:
                    newTasks = []
                    nf = 0
                    while nf < 1000:
                        try:
                            newTasks.append( (mocap.__next__(), camIDs) )
                            nf += 1
                        except StopIteration:
                            framesLeft = False
                            #for i in range(NUM_CPUS):
                            #    tasks.put('NONE_LEFT')
                tasks.put( newTasks )
                numBusy += 1

            if tasks.qsize() > 100:
                sleep(1)

            if ((numFramesProcessed%5000)==0 or useLog) and len(newTasks) > 0:
                s = "/" + str(totalNumFrames)+" ("+'{0:.{1}f}'.format(
                        100*numFramesProcessed/totalNumFrames, 3)+'%)'
                print("Processed "+str(numFramesProcessed)+s+" frames (frame="+
                      str(newTasks[0][0].frameID)+")")

            # If not async, process the newly queued item now
            if not async:
                findTriggers_Worker(tasks, output, async=False)

            # Write output
            while not output.empty():
                s = output.get()
                if s == 'DONE':
                    numBusy -= 1
                else:
                    print("Wrote to file: "+s)
                    fOut.write(s+'\n')
                    fOut.flush()

            # Done?
            if numBusy == 0 and not framesLeft and tasks.empty():
                break

    # Stop worker threads
    pool.terminate()

    print("Done searching for triggers.")

# =======================================================================================
# Find the camera containing the syncbox
# =======================================================================================

def findCameraWithSync(fname):

    if DEBUG:
        return [(x,1) for x in DEBUG_CAMS]

    # Write debug image with all 2d markers
    plot2dCentroids(fname)

    print("Finding camera's with sync box")

    maxCamID = getMaxCamID(fname)

    NUM_FRAMES_TO_CHECK = 100

    camsWithSync = []
    tbs = {}

    # Sample small chunks of the file at many different points
    # (100 frames (.5 second) every 60 seconds)
    startFrames = []
    for i, id in util.iterMocapFrameIDs(fname):
        if (i % (60 * 200 * 10)) == 0:
            startFrames.append(id)

    for camID in range(maxCamID+1):
        print("Checking if camera "+str(camID)+" contains the sync box...")
        fractions = []
        for startFrame in startFrames:
            if not camID in tbs:
                tbs[camID] = []
            tbsn = [x for x in findTriggerBoxInFrames(
                fname, camID, numFrames=NUM_FRAMES_TO_CHECK, startFrame=startFrame)]
            tbs[camID] += tbsn
            fractions.append( len(tbsn) / NUM_FRAMES_TO_CHECK )
        if max(fractions) >= 0.90:
            camsWithSync.append( (camID, max(fractions)) )

    # Write debug image with identified triggerboxes
    plotTriggerboxes(fname, tbs)

    # Done!
    return camsWithSync

# =======================================================================================
# Produce overlay image of all camera centroids to confirm that triggerbox exists
# if none is found
# =======================================================================================

def plot2dCentroids(fname):

    os.makedirs(os.path.join(os.path.dirname(fname), 'debug'), exist_ok=True)

    # Skip if files already exist
    if len([x for x in os.listdir(os.path.join(os.path.dirname(fname), 'debug/'))
        if 'MaxProjection' in x]) > 0:
        # Skip... (we assume if there is one .png file, they're all there...)
        return

    if not fname.endswith('.raw.msgpack'):
        fname = fname.replace('.msgpack','.raw.msgpack')

    # Figure out width and height of cameras
    w, h = None, None
    for frame in util.iterMocapFrames(fname):
        if w != None and h != None: break
        for cid in frame.centroids:
            w = frame.centroids[cid].width
            h = frame.centroids[cid].height
            break

    # Gather camera 2d histogram
    camImgs = [np.zeros((h, w)) for x in range(18)]
    totalRecords = util.countRecords(fname)
    iRecord = 0

    for x in util.iterMocapFrames(fname):

        iRecord += 1
        if (iRecord%20000) == 0:
            print("Plotting camera centroids ("+'{:.2f}'.format(100*iRecord/totalRecords)+" %)")

        for frameID, frame in x.centroids.items():
            for c in frame.centroids:
                for dx in range(0,1): #range(-1, 2):
                    for dy in range(0,1): #range(-1, 2):
                        camImgs[frameID - 1][max(0, min(h - 1, int(c.y + dy)))]\
                            [max(0, min(w - 1, int(c.x + dx)))] += 1

    # Save images (max-proj)
    for camID in range(18):
        scipy.misc.imsave(os.path.join(os.path.dirname(fname),
            'debug/MaxProjection_Camera_' + str(camID) + '.png'), camImgs[camID] >= 1)

    # Save images (log)
    for camID in range(18):
        img = np.log(camImgs[camID] + 1)
        img *= (255.0 / np.max(img))
        scipy.misc.imsave(os.path.join(os.path.dirname(fname),
            'debug/LogProjection_Camera_' + str(camID) + '.png'), img)

    # Done!
    pass

def plotTriggerboxes(fname, tbs):

    os.makedirs(os.path.join(os.path.dirname(fname), 'debug'), exist_ok=True)

    d = []
    for c in tbs:
        for x in tbs[c]:
            for y in x:
                d.append(y)
    d = pd.DataFrame(d, columns=['camID', 'frameID', 'x', 'y', 'type'])
    (ggplot(d, aes(x='x', y='y')) +
        geom_point(size=2) +
        facet_wrap('camID')).save(
        filename=os.path.join(os.path.dirname(fname),'debug/debug_triggerbox.png'))

    # Draw the triggerbox markers onto the MaxProjection images
    dr = os.path.join(os.path.dirname(fname), 'debug/')
    imgs = [(int(re.sub('[^0-9]', '', x)), os.path.join(dr, x)) for x in
            os.listdir(dr) if '_Camera_' in x and '_marked' not in x]
    for camID, fname in imgs:
        img = scipy.misc.imread(fname, mode='RGB')
        for irow, row in d[d['type'] == 'syncbox'].iterrows():
            iy = max(0, min(img.shape[0], int(row.y)))
            ix = max(0, min(img.shape[1], int(row.x)))
            img[iy, ix] += np.array([1, 0, 0], dtype=np.uint8)
        fnameOut = fname.replace('.png', '_marked.png')
        img[:, :, 0] = (img[:, :, 0] > 0) * 255
        scipy.misc.imsave(fnameOut, img)

    # Done!
    pass

# =======================================================================================
# Extract finalized trigger signals
# =======================================================================================

def finalizeTriggers(fname):
    # Create filename to read
    fnameLedTriggers = fname.replace('.raw.msgpack', '').replace('.msgpack', '') + '.led_triggers_tmp'

    fnameLedTriggersFinal = fname.replace('.raw.msgpack', '').replace('.msgpack', '') + '.led_triggers'

    # Read the trigger file
    triggers = pd.read_csv(fnameLedTriggers)

    # Look for continuous blocks of 5 frames with trigger
    # TODO: Do quality check by checking how many cameras actually support that trigger being there,
    #       and how reliable those cameras have been
    # TODO: Check for overlapping trigger ranges across different cameras... that is likely error...

    actualTriggers = {}
    for camID in triggers.camID.unique():
        # Get frames seen as triggers by this camera
        frames = sorted(list(set(triggers[(triggers.camID==camID)&(triggers.type=='trigger')].frame.tolist())))
        if len(frames) > 0:
            # Find continuous ranges
            # Code source: https://stackoverflow.com/questions/7352684/how-to-find-the-groups-of-consecutive-elements-from-an-array-in-numpy, user answer: unutbu
            ranges = [(x[0], x[-1]) for x in np.split(frames, np.where(np.diff(frames) != 1)[0] + 1)]
            # Find ranges that are 5 frames long
            for r in [r for r in ranges if (1+r[1]-r[0]) == 5]:
                if r[0] in actualTriggers:
                    actualTriggers[r[0]][0].append(camID)
                else:
                    # Get whether trigger is valid
                    #    - e.g. trigger that overlaps, and starts later than, another trigger is not valid
                    #      (however, this shouldn't happen, so report a warning)
                    isValid = (len([z for z in actualTriggers.keys() if z < r[0] and z > (r[0] - 5)]) == 0)
                    if not isValid:
                        print("WARNING: Overlapping triggers detected!")
                    # Save!
                    actualTriggers[r[0]] = [[camID], r[0], r[1], isValid]

    # Now output a new file with following data: triggerFrame, camIDs (i.e. list), timestamp, timestamp_str
    with open(fnameLedTriggersFinal, 'w') as fOut:
        fOut.write('camIDs,frameID,isvalid,timestamp,timestamp_str\n')
        for _, actualTrigger in actualTriggers.items():

            camIDs  = actualTrigger[0]
            frameID = actualTrigger[1]
            isValid = actualTrigger[3]

            timestamp, timestamp_str = None, None
            for camID in camIDs:
                s = (triggers.camID == camID) & (triggers.frame == frameID)
                if np.any(s):
                    timestamp     = triggers[s].timestamp.tolist()[0]
                    timestamp_str = triggers[s].timestamp_str.tolist()[0]
                    break

            fOut.write(','.join([' '.join([str(y) for y in camIDs]), str(frameID), str(isValid),
                                 str(timestamp), timestamp_str])+'\n')

# =======================================================================================
# Plot trigger signals, for debugging
# =======================================================================================

def plotTriggerSignals(fname):

    # Read triggers
    fnameLedTriggersFinal = fname.replace('.raw.msgpack', '').\
                                replace('.msgpack', '') + '.led_triggers'
    triggers = pd.read_csv(fnameLedTriggersFinal)

    # Ensure that the output directory exists
    os.makedirs(os.path.join(os.path.dirname(fname), 'debug/triggers'), exist_ok=True)

    # Get all cameras
    camsToPlot = range(getMaxCamID(fname))

    # Process each trigger
    for _, trigger in triggers.iterrows():

        # Output image name
        fnameOut = os.path.join(os.path.dirname(fname), 'debug/triggers/') + \
                   'trigger_' + str(trigger.frameID) + '.png'

        # camsToPlot = trigger.camIDs.split(' ')
        print("Plotting trigger at frame: "+str(trigger.frameID))

        # (Re-)Extract triggerbox signal
        td = []
        for camID in [int(x) for x in camsToPlot]:
            tbs = [(f.frameID, findTriggerBoxInFrame(f, camID, spacing=35)[0]) for f in
                   util.iterMocapFrames(fname, startFrame=trigger.frameID - 10, numFrames=20)]
            for i, tb in tbs:
                if tb is None: continue
                for x in ['c1', 'c2', 'c3']:
                    td.append([camID, i, 'syncbox'] + tb[x].tolist())
                for c in tb['additionalPts']:
                    td.append([camID, i, 'additionalpt'] + c.tolist())

        td = pd.DataFrame(td, columns=['camID', 'frame', 'type', 'x', 'y'])

        # Plot
        print("Saving image...")
        p = sns.FacetGrid(td, col='frame', row='camID',
                          sharex='row', sharey='row')
        p.map(plt.scatter, 'x', 'y')
        p.savefig(fnameOut)

    # Done!
    pass

# =======================================================================================
# Process file
# =======================================================================================

def processFile(fname):

    if not fname.endswith('.msgpack'):
        raise Exception("extract_mocap_trigger.processFile requires .msgpack input.")

    # Switch to the .raw.msgpack file
    if not fname.endswith('.raw.msgpack'):
        fname = fname.replace('.msgpack', '.raw.msgpack')

    # Don't process if file already exists
    fnameLedTriggers = fname.replace('.raw.msgpack','').replace('.msgpack','')+'.led_triggers_tmp'
    if os.path.exists(fnameLedTriggers):
        print("Skipping trigger search, file already exists: " + fnameLedTriggers)
    else:
        # Find the camera with the sync signal
        camsWithSync = findCameraWithSync(fname) # [(10,1),]

        # Get the camera with the maximum fraction of triggerboxes found
        camsWithSync = sorted(camsWithSync, key=lambda x:-x[1])

        # No cams?
        if len(camsWithSync) == 0:
            print("Can't extract trigger signals, no sync signal found.")
        else:
            print("Found multiple sync cameras:")
            for camWithSync in camsWithSync:
                print("Camera "+str(camWithSync[0])+" ("+str(camWithSync[1])+").")

            # Search for triggers
            findTriggers(fname, [x[0] for x in camsWithSync], async=True,
                         startFrame=(DEBUG_STARTFRAME if DEBUG else None))

    # Finalize trigger signals
    finalizeTriggers(fname)

    # Having found the triggers, now plot them for manual inspection
    plotTriggerSignals(fname)

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
    run(None)
