
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

# Imports
import os, gc, multiprocessing, dateutil, datetime, traceback
import numpy as np
import pandas as pd
import seaborn as sns
import matplotlib as mpl
import matplotlib.animation as mpl_animation
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D

# Global variables / settings
DEBUG = False
OVERWRITE = False
PLOT_APPEND_BEFORE = 25
PLOT_APPEND_AFTER = 200

trajectoriesLeft = {}

# Plot a given (1) flysim trajectory, (2) dF takeoff trajectory, (3) and frames after onset
def plotTrajectory(file, takeoffTraj, titleSuffix='', upward=True, flysim=False):
    
    dirname = file.replace('.msgpack','')
    if '_' in dirname: dirname = dirname[:dirname.find('_')]
    saveDir = '../output/'+dirname+'/'+ \
        ('flysim' if flysim else 'takeoff')+('_upward' if upward else '')+'/'
    os.makedirs(saveDir, exist_ok=True)
    foutname = saveDir+str(takeoffTraj)+'.mp4'
    
    # Skip if exists
    if os.path.exists(foutname) and not OVERWRITE: 
        print("["+file[0:30]+"] Skipping trajectory plot: "+str(takeoffTraj))
        return
    
    try:
        # Load takeoff info
        dataTakeoff = pd.read_csv(file.replace('.msgpack','.takeoffs.csv'))
        dataTakeoff = dataTakeoff.rename(columns=lambda x: x.strip())

        dataTakeoff = dataTakeoff[dataTakeoff.index==takeoffTraj]

        # Select yframe frames
        dataDf    = pd.read_csv(file.replace('.msgpack','.tracking.csv'))
        dataDf['takeoffFrame'] = dataDf['relframe'] # TMP #
        dataDfAll = dataDf
        dataDf    = dataDf[dataDf.takeoffTraj==takeoffTraj]
        
        # Select flysim frames
        dataFs = pd.read_csv(file.replace('.msgpack','.flysim.tracking.csv'))
        
        dataFs = dataFs[dataFs.trajectory==int(dataTakeoff.flysimTraj)]
        
        # Plot yframe data
        fig = plt.figure(figsize=(8,8), dpi=400, frameon=False)
        ax1 = fig.add_subplot(111, projection='3d')

        def animate(t):
        
            if len(dataDf.index) == 0: return
            
            takeoffFrame = dataDf.takeoffFrame.tolist()[0]
            
            minFrame = takeoffFrame - PLOT_APPEND_BEFORE
            maxFrame = max(takeoffFrame + PLOT_APPEND_AFTER, \
                max(dataFs.frame) if len(dataFs.index)>0 else max(dataDf.frame))
            maxFrame = int(minFrame + int(maxFrame-minFrame) * t / 200)
            
            # Clear the drawings in this subplot
            ax1.cla()
            
            s1, s2 = (None, None)

            d = dataDf[dataDf.frame <= maxFrame]
            s1 = ax1.scatter(d.x.tolist(), d.y.tolist(), d.z.tolist(), s=2, color='red')
            
            d = dataDfAll[(dataDfAll.frame <= maxFrame) & (dataDfAll.frame >= minFrame)]
            s3 = ax1.scatter(d.x.tolist(), d.y.tolist(), d.z.tolist(), s=1, color='gray')
                        
            d = dataFs[dataFs.frame <= maxFrame]
            s2 = ax1.scatter(d.x.tolist(), d.y.tolist(), d.z.tolist(), s=1, color='black')

            ax1.set_title('takeoff trajectory '+str(takeoffTraj).zfill(5)+' frames '+str(takeoffFrame)+ \
                ' + '+str(maxFrame-takeoffFrame).zfill(5)+titleSuffix, \
                family='monospace')

            # Set limits for entire dataset
            #ax1.set_xlim([min(dataDf.x.tolist()+dataFs.x.tolist()),max(dataDf.x.tolist()+dataFs.x.tolist())])
            #ax1.set_ylim([min(dataDf.y.tolist()+dataFs.y.tolist()),max(dataDf.y.tolist()+dataFs.y.tolist())])
            #ax1.set_zlim([min(dataDf.z.tolist()+dataFs.z.tolist()),max(dataDf.z.tolist()+dataFs.z.tolist())])
        
            ax1.set_xlim([-500, 500])
            ax1.set_ylim([-500, 500])
            ax1.set_zlim([-100, 800])
            
            return s1, s2, s3

        ani = mpl_animation.FuncAnimation(
            fig, animate, frames=200, interval=1, blit=True)

        writer = mpl_animation.writers['ffmpeg'](
            fps=30, metadata=dict(artist='Me'), bitrate=1800)
        
        foutnameTmp = foutname.replace('.mp4','.tmp.mp4')
        
        ani.save(foutnameTmp, writer=writer)
        
        # Close figure in order to release memory
        plt.close(fig)
        del fig
        gc.collect()
        
        # Rename file to indicate we're done
        os.rename(foutnameTmp, foutname)
        
        print("["+file[0:30]+"] Finished trajectory plot: "+str(takeoffTraj))
        
    except Exception as e:
        # Save error to the respective file
        with open(saveDir+str(takeoffTraj)+'.error','w') as f:
            f.write(traceback.format_exc())

# ...
def getTakeoffTrajectories(file, upward=True, flysim=False):
    dataTakeoff = pd.read_csv(file.replace('.msgpack','.takeoffs.csv'))
    dataTakeoff = dataTakeoff.rename(columns=lambda x: x.strip())
    
    # Only process takeoffs with a minimum amount of perching preceding it
    dataTakeoff = dataTakeoff[dataTakeoff.perchframes>=200]
    
    if upward:
        dataTakeoff = dataTakeoff[dataTakeoff.upward==upward]
    
    if flysim:
        dataTakeoff = dataTakeoff[dataTakeoff.flysimTraj!=-1]
    
    titleSuffixes = []
    for index, row in dataTakeoff.iterrows():
        titleSuffix  = '\nupward='+'{0:.4f}'.format(float(row['upwardVelocityMax']))
        titleSuffix += ', bbox='+'{0:.4f}'.format(float(row['bboxsize']))
        titleSuffix += ', perchframes='+str(float(row['perchframes']))
        titleSuffix += ', r2='+'{0:.5f}'.format(float(row['r2']))
        titleSuffix += '\n'
        titleSuffix += row['time']
        titleSuffix += '\n'
        titleSuffix +=     '{0:.4f}'.format(float(row['p1']))
        titleSuffix += ','+'{0:.4f}'.format(float(row['p2']))
        titleSuffix += ','+'{0:.4f}'.format(float(row['p3']))
        titleSuffixes.append(titleSuffix)
    
    return zip(dataTakeoff.index.tolist(), titleSuffixes)

# =======================================================================================
# Entry point
# =======================================================================================

def run(async=False, settings=None):
    
    # Get files
    if settings == None:
        settings = util.askForExtractionSettings()    

    # Files to plot:
    print('Files to plot:\n'+'\n'.join(settings.files))
    
    params = []
    for file in settings.files:
        for upward in [False,]: #True, False]:
            for flysim in [True,False]: 
                takeoffTrajs = getTakeoffTrajectories(file, upward=False, flysim=flysim)
                params += [(file, x[0], x[1], upward, flysim) for x in takeoffTrajs]
    
    if DEBUG:
        for param in params:
            plotTrajectory(param[0], param[1], param[2], param[3])
    else:
        trajectoriesLeft[file] = len(params)
        with multiprocessing.Pool(processes=12) as pool:
            (pool.starmap_async if async else pool.starmap)(plotTrajectory, params)
            return pool
    
    return None

if __name__ == "__main__":
    run()