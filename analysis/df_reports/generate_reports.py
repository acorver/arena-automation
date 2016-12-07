
#
# This script generates a report summarizing the daily activity in the 
# dragonfly arena.
#
# Author: Abel Corver, November 2016
#         abel.corver@gmail.com
#         (Anthony Leonardo Lab)
#
# TODO: - Create 3D Vispy Axis
#       - Create AxisGrid to connect Axes (later: depending on viewpoint)
#             --> This takes two or three axes as argument
#             --> Later, it will determine which axis-end combinations 
#                 to show based on the least grid-to-camera distance to the camera

# ========================================================
# Imports
# ========================================================

import os, gc, shutil, math, dateutil, datetime
from distutils import dir_util
import numpy as np
import pandas as pd
import seaborn as sns
import matplotlib as mpl
import matplotlib.animation as mpl_animation
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D
import imageio
from jinja2 import Environment, FileSystemLoader
from vispy import app, gloo, scene
import vispy.visuals, vispy.scene, vispy.scene.visuals
from vispy.color import Color

# Change working directory
os.chdir(os.path.dirname(os.path.abspath(__file__)))

# ========================================================
# Global Variables
# ========================================================

# Change output directory
DIR_REPORTS         = './reports/'
DIR_DATA            = './data/'
OVERWRITE           = True
SHOW_3DCANVAS       = False
IMG_DPI             = 200
PLOT_APPEND_BEFORE  = 25
PLOT_APPEND_AFTER   = 200
RENDER_SIZE_3D      = (1400, 1000)

PLOT_TRAJECTORIES_3D_NUMFRAMES = 200

# ========================================================
# Helper functions
# ========================================================

def roundTime(dt=None, roundTo=60):
   """Round a datetime object to any time laps in seconds
   dt : datetime.datetime object, default now.
   roundTo : Closest number of seconds to round to, default 1 minute.
   Author: Thierry Husson 2012 - Use it as you want but don't blame me.
   """
   if dt == None : dt = datetime.datetime.now()
   seconds = (dt.replace(tzinfo=None) - dt.min).seconds
   rounding = (seconds+roundTo/2) // roundTo * roundTo
   return dt + datetime.timedelta(0,rounding-seconds,-dt.microsecond)

# ========================================================
# Helper functions: Data loading
# ========================================================

def getDataTakeoffs(data):
    
    fname = DIR_DATA + data['file'].replace('.msgpack','.takeoffs.csv')
    
    dataTakeoffs = pd.read_csv(fname, sep=',')
    dataTakeoffs = dataTakeoffs.rename(columns=lambda x: x.strip())
    dataTakeoffs['time_bin'] = [roundTime(dateutil.parser.parse(x),300) \
        for x in dataTakeoffs['time']]
    
    return dataTakeoffs

def getDataPerches(data):
    fname = DIR_DATA + data['file'].replace('.msgpack','.perches.csv')
    d = pd.read_csv( fname )
    d.numframes = pd.to_numeric(d.numframes, errors='coerce').fillna(0).astype('int')
    return d

# ========================================================
# Plot perching locations
# ========================================================

def plotPerchingLocations2D(dir, data):
    
    # Load perching data
    perches = getDataPerches(data)
    
    # Plot 1
    with sns.axes_style("white"):
        fig = sns.jointplot(x=perches['x'], y=perches['y'], kind="hex", color="k", \
            size=8, joint_kws=dict(gridsize=50), marginal_kws=dict(bins=50))
        plt.subplots_adjust(top=0.9)
        fig.fig.suptitle('Number of Perches > 10 seconds')
        
        # Save
        outName = 'perching_locations_2d_count.png'
        outFile = dir + outName
        fig.savefig(outFile, dpi=IMG_DPI)
        plt.close(fig.fig)
        # Save info for report generation
        data['srcPerchingCount2D'] = outName
    
    print('Finished perching plot 1')
    
    # Plot 2
    x, y = [], []
    for i, p in perches.iterrows():
        x += [p['x'] for j in range(p['numframes'])] # Inefficient! =) Better to manually create heatmap data
        y += [p['x'] for j in range(p['numframes'])] # Inefficient! =)
    with sns.axes_style("white"):
        fig = sns.jointplot(x=perches['x'], y=perches['y'], kind="hex", color="k", \
            size=8, joint_kws=dict(gridsize=50), marginal_kws=dict(bins=50))
        plt.subplots_adjust(top=0.9)
        fig.fig.suptitle('Total perching duration')
        # Save
        outName = 'perching_locations_2d_total.png'
        outFile = dir + outName
        fig.savefig(outFile, dpi=IMG_DPI)
        plt.close(fig.fig)
        # Save info for report generation
        data['srcPerchingTotal2D'] = outName
    
    print('Finished perching plot 2')

    # Free data
    gc.collect()
    
# ========================================================
# Plot trajectories in 3D
# ========================================================

def plotTrajectories3D(dir, data):
    
    file = data['file']
    foutfname = 'takeoffs.mp4'
    foutname = dir + foutfname
    foutnameTmp = foutname.replace('.mp4','.tmp.mp4')

    # Skip if already created video
    if not os.path.isfile(foutname) or OVERWRITE:
        # Load takeoff info
        dataTakeoff = getDataTakeoffs(data)
        
        # Select yframe frames
        dataDf    = pd.read_csv('data/'+file.replace('.msgpack','.tracking.csv'))
        dataDf['takeoffFrame'] = dataDf['relframe'] # TMP #
        
        if len(dataDf.index) == 0: 
            data['srcTrajectories3D'] = 'Not Available'
            return
        
        takeoffFrame = dataDf.takeoffFrame.tolist()[0]
        
        # Plot yframe data
        canvas = vispy.scene.SceneCanvas(show=SHOW_3DCANVAS, size=RENDER_SIZE_3D)
        grid = canvas.central_widget.add_grid(spacing=10)
        widget_1 = grid.add_widget(row=0, col=0)
        widget_1.bgcolor = "#fff"
        view = widget_1.add_view()
        view.bgcolor = '#fff'
        
        cm = vispy.color.get_colormap('husl')
        uniqueTrajs = np.unique(dataDf.as_matrix(['trajectory',]))
        
        for traji, traj in zip(range(len(uniqueTrajs)),uniqueTrajs):
            c = cm[(traj%10)/10.0]
            vispy.scene.visuals.Line(
                pos=dataDf[dataDf.trajectory==traj].as_matrix(['x','y','z']),
                color=c,
                antialias=True,
                connect='strip',
                parent=view.scene)
        
        # Set camera view
        view.camera = 'turntable'
        #view.camera.elevation = 45
        view.camera.distance = 1000
        view.camera.azimuth = 135

        xax = scene.Axis(pos='x', view=view, tick_direction=(0, 1, 0),
                 font_size=20, axis_color='k', tick_color='k', text_color='k',
                 major_tick_length=10, minor_tick_length=5,
                 major_density = 0.2, minor_density = 0.2, 
                 parent=view.scene)
        yax = scene.Axis(pos='y', view=view, tick_direction=(1, 0, 0),
                font_size=20, axis_color='k', tick_color='k', text_color='k',
                major_tick_length=10, minor_tick_length=5,
                 major_density = 0.2, minor_density = 0.2, 
                parent=view.scene)
        zax = scene.Axis(pos='z', view=view, tick_direction=(1, 0, 0),
                font_size=20, axis_color='k', tick_color='k', text_color='k',
                major_tick_length=10, minor_tick_length=5,
                 major_density = 0.2, minor_density = 0.2, 
                parent=view.scene)
        g = scene.AxisGrid([xax, yax, zax], parent=view.scene, minor_color='#555', 
            major_color='#222', bg_color='#0002')
        
        writer = imageio.get_writer(foutname)
        for i in range(PLOT_TRAJECTORIES_3D_NUMFRAMES):
            im = canvas.render()
            writer.append_data(im)
            view.camera.azimuth = \
                110 + 60*i/PLOT_TRAJECTORIES_3D_NUMFRAMES
        writer.close()

        # Close figure in order to release memory
        gc.collect()
        
        # Rename file to indicate we're done
        # os.rename(foutnameTmp, foutname)
        
    # Save location of file
    data['srcTrajectories3D'] = foutfname

# ========================================================
# Trial statistics
# ========================================================

def plotTrialStatistics(dir, data):
    # Plot trial statistics
    dataTrials = pd.read_csv('data/2016-11-14 14-09-16.trials.csv', sep=',')
    dataTrials = dataTrials.rename(columns=lambda x: x.strip())

    fig = plt.figure(figsize=(8,6), dpi=IMG_DPI)
    ax1 = fig.add_subplot(221)
    ax2 = fig.add_subplot(222)
    ax3 = fig.add_subplot(223)
    
    sns.distplot(dataTrials['speed'] , ax=ax1)
    sns.distplot(dataTrials['height'], ax=ax2)
    sns.distplot(dataTrials['wait']  , ax=ax3)
    
    # Save plot
    outName = 'trial_statistics.png'
    outFile = dir + outName
    fig.savefig(outFile, dpi=IMG_DPI)
    plt.close(fig)
    
    data['srcTrialStatistics'] = outName
    
# ========================================================
# Save metadata
# ========================================================

def loadMetadata(dir, data):
    
    data['report_date'] = data['file'][:data['file'].find('_')]
    
    data['timespan'] = ''

# ========================================================
# Plot trajectory properties
# ========================================================

def plotTrajectoryProperties(dir, data):
    # Of the takeoffs, what is the bounding box size distribution?
    fig = plt.figure(figsize=(12,8), dpi=600)
    ax1 = fig.add_subplot(121)
    ax2 = fig.add_subplot(122)
    
    # Load takeoff statistics
    dataTakeoffs = getDataTakeoffs(data)
    
    # Plot:
    d = dataTakeoffs[dataTakeoffs.perchframes>200] #<(200*3600*1)]
    sns.distplot(np.log10(d.perchframes/(200*60)), ax=ax1)

    # Plot:
    d = dataTakeoffs[dataTakeoffs.perchframes>=200]
    sns.distplot(d.bboxsize, ax=ax2)

    # Set titles/labels
    ax1.set_title("Perching Length Distribution Before Takeoff")
    ax2.set_title("Bounding Box Size Distribution of Takeoffs")

    ax1.set(xlabel='Log(Perching Time in Minutes)', ylabel='Proportion')
    ax2.set(xlabel='Bounding Box Diameter', ylabel='Proportion')
    
    # Save plot
    outName = 'traj_statistics.png'
    outFile = dir + outName
    fig.savefig(outFile, dpi=IMG_DPI)
    plt.close(fig)
    
    data['srcTrajStatistics'] = outName

# ========================================================
# Plot daily activity
# ========================================================

def plotDailyActivity(dir, data):
    
    # Load takeoff statistics
    dataTakeoffs = getDataTakeoffs(data)
    
    # Bin takeoffs
    dt = dataTakeoffs[dataTakeoffs.perchframes>200]

    d1 = pd.DataFrame( \
        data={'takeoffs': dt.groupby('time_bin').count()['trajectory']}, \
        index=dt['time_bin'])

    # Bin takeoff bbox size
    d2 = pd.DataFrame( \
        data={'takeoffs': dt.groupby('time_bin').mean()['bboxsize']}, \
        index=dt['time_bin'])

    # Plot the takeoff activity
    fig = plt.figure(figsize=(20,15), dpi=400)
    ax1 = fig.add_subplot(211)
    ax2 = fig.add_subplot(212)

    ax1.plot(d1.index, d1['takeoffs'])
    ax2.plot(d2.index, d2['takeoffs'])

    # Save plot
    outName = 'daily_statistics.png'
    outFile = dir + outName
    fig.savefig(outFile, dpi=IMG_DPI)
    plt.close(fig)
    
    data['srcDailyActivity'] = outName

# ========================================================
# Process file
# ========================================================

def processFile(file):
    
    # Determine output directory/files
    dir = DIR_REPORTS + file[:file.find('_')] + '/'
    outFile = dir + 'index.html'
    
    # Initialize report directory
    # shutil.rmtree(dir, ignore_errors=True)
    dir_util.copy_tree('./df_reports/templates/static/', dir)

    # Create Jinja environment (for HTML template rendering)
    env = Environment(loader=FileSystemLoader('./df_reports/templates'))
    template = env.get_template('index.html')
    
    # Initialize report data
    data = {}
    data['file'] = file
    
    # Generate data
    for f in [\
        plotPerchingLocations2D, \
        plotTrajectories3D, loadMetadata, plotTrialStatistics, \
        plotTrajectoryProperties, plotDailyActivity]:
        # Compute data
        f(dir, data)
    
    # Write template
    with open(outFile, 'w') as fo:
        fo.write(template.render(data).replace(u'\ufeff',u''))
    
# ========================================================
# Entry point
# ========================================================

def run():
    # Get all raw data files in data directory
    files = [x for x in os.listdir('./data/') if x.endswith('.msgpack')]
        
    # Process newest files first
    files.sort(key=lambda x: -os.path.getmtime('./data/'+x))

    #for file in ['2016-11-08 10-34-19_Cortex.msgpack','2016-11-11 12-20-41_Cortex.msgpack','2016-11-15 11-23-04_Cortex.msgpack']:
    processFile(files[0])

if __name__ == "__main__":
    run()
