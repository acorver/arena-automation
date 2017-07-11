
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
#       - Plot all FlySim points as trajectories, to see what area was covered
#       - Plot all frame processing delays
#       - Compute FlySim speeds
#       - Plot all internally computed motion trajectories after takeoff, aligned to trigger

# ========================================================
# Imports
# ========================================================

# Change working directory so this script can be run independently as well as as a module
import os, sys
if __name__ == "__main__": 
    p = os.path.dirname(os.path.abspath(__file__))
    os.chdir(os.path.join(p,'../../data'))
    sys.path.append(os.path.join(p,'../'))

# Import libraries
import gc, shutil, math, dateutil, datetime
from distutils import dir_util
import numpy as np
import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt
import imageio
from jinja2 import Environment, FileSystemLoader
from vispy import app, gloo, scene
import vispy.visuals, vispy.scene, vispy.scene.visuals
from shared import util
import multiprocessing
import traceback
import ntpath
import ipyvolume.pylab as p3

from shared import trajectories

# ========================================================
# Global Variables
# ========================================================

# Change output directory
DIR_REPORTS         = '../../reports/'
DIR_DATA            = ''
SINGLEFILE          = ''
OVERWRITE           = True
SHOW_3DCANVAS       = False
IMG_DPI             = 200
PLOT_APPEND_BEFORE  = 25
PLOT_APPEND_AFTER   = 200
RENDER_SIZE_3D      = (1400, 1000)
DEBUG               = False
SYNCHRONOUS         = False

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
    d = d[d.numframes>=200]
    return d

# ========================================================
# Plot frame processing times
# ========================================================

def plotProcessingDelays(dir, data):
    
    # Open motion log file if it exists
    with open(data['file'].replace('.msgpack','.motionlog'),'r') as fML:
        pass
    
    # ...
    pass

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
# Helper function to create 3D plot with large amount of data
# ========================================================

def plot3D(data, groupByColumn, out, cols=['x','y','z']): 
    
    canvas = vispy.scene.SceneCanvas(show=SHOW_3DCANVAS, size=RENDER_SIZE_3D)
    grid = canvas.central_widget.add_grid(spacing=10)
    widget_1 = grid.add_widget(row=0, col=0)
    widget_1.bgcolor = "#fff"
    view = widget_1.add_view()
    view.bgcolor = '#fff'
        
    cm = vispy.color.get_colormap('husl')
    uniqueTrajs = np.unique(groupByColumn.as_matrix()).astype('int')
        
    for traji, traj in zip(range(len(uniqueTrajs)),uniqueTrajs):
        c = cm[(traj%10)/10.0]
        d = data[groupByColumn==traj].as_matrix(cols)
        if d.shape[0] > 0:
            vispy.scene.visuals.Line(
                pos=d,
                color=c,
                antialias=True,
                connect='strip',
                parent=view.scene)
    
    xax = scene.Axis(pos='x', domain = [-500,500], view=view, tick_direction=(0, 1, 0),
                font_size=20, axis_color='k', tick_color='k', text_color='k',
                major_tick_length=10, minor_tick_length=5,
                major_density = 0.2, minor_density = 0.2, 
                parent=view.scene)
    yax = scene.Axis(pos='y', domain = [-500,500], view=view, tick_direction=(1, 0, 0),
            font_size=20, axis_color='k', tick_color='k', text_color='k',
            major_tick_length=10, minor_tick_length=5,
                major_density = 0.2, minor_density = 0.2, 
            parent=view.scene)
    zax = scene.Axis(pos='z', domain = [-100,700], view=view, tick_direction=(1, 0, 0),
            font_size=20, axis_color='k', tick_color='k', text_color='k',
            major_tick_length=10, minor_tick_length=5,
                major_density = 0.2, minor_density = 0.2, 
            parent=view.scene)
    
    # Set camera view
    view.camera = 'turntable'
    #view.camera.elevation = 45
    view.camera.distance = 1000
    view.camera.azimuth = 135
    
    g = scene.AxisGrid([xax, yax, zax], parent=view.scene, minor_color='#555', 
        major_color='#222', bg_color='#0002')
    
    if isinstance(out, str):
        writer = imageio.get_writer(out)
        writer.append_data(canvas.render())
        writer.close()
    
    # Free memory
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
        dataTakeoffs = getDataTakeoffs(data)

        if len(dataTakeoffs) == 0:
            data['srcTrajectories3D'] = 'Not Available'
        else:
            # Plot yframe data
            canvas = vispy.scene.SceneCanvas(show=SHOW_3DCANVAS, size=RENDER_SIZE_3D)
            grid = canvas.central_widget.add_grid(spacing=10)
            widget_1 = grid.add_widget(row=0, col=0)
            widget_1.bgcolor = "#fff"
            view = widget_1.add_view()
            view.bgcolor = '#fff'

            cm = vispy.color.get_colormap('husl')

            for traji, traj in dataTakeoffs.iterrows():
                c = cm[(traji%10)/10.0]
                d = trajectories.getTakeoff(file, traj.trajectory, traj.frame).as_matrix(['x','y','z'])
                if d.shape[0] > 0:
                    vispy.scene.visuals.Line(
                        pos=d,
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
# Plot trajectories in 3D (interactive)
# ========================================================

def plotTrajectories3DInteractive(dir, data):

    fname = data['file']

    mocap = pd.read_csv(fname.replace('.raw.msgpack','').replace('.msgpack','')+'.mocap.all.csv')

    # ===================================================
    # Helper function
    # ===================================================

    def _plot(d, suffix='', embed=True, all=False, reduce=2, metadata={}, quiver=True):
        os.makedirs(os.path.join(dir, 'snippets/'), exist_ok=True)

        takeoffs = d[d.type == 'yf_center'][::reduce].as_matrix(['x', 'y', 'z'])
        takeoffs = takeoffs[~np.isnan(takeoffs).any(axis=1)]
        flysim = d[d.type == 'flysim'][::reduce].as_matrix(['x', 'y', 'z'])
        flysim = flysim[~np.isnan(flysim).any(axis=1)]

        p3.figure(width=1000, height=600, camera_control='trackball',
                  xlabel='x', ylabel='z', zlabel='y')

        if all:
            a = d[::reduce].as_matrix(['x','y','z'])
            a = a[~np.isnan(a).any(axis=1)]
            p3.scatter(a[:, 0], a[:, 2], a[:, 1], size=0.15, color='#999')

        if takeoffs.shape[0] > 0:
            for _, t in d[(d.type=='yf_center')&
                    (~np.isnan(d.as_matrix(['x','y','z'])).any(axis=1))].groupby('trajectory'):
                x = np.tile(t.as_matrix(['x','y','z'])[0,:], (2,1))
                p3.scatter(x[:,0], x[:,2], x[:,1], size=1.0, color='#f00')

                to = t[::20].as_matrix(['x','y','z'])
                gr = np.diff(to, axis=0)

                if quiver:
                    p3.quiver(to[:, 0], to[:, 2], to[:, 1],
                              gr[:, 0], gr[:, 2], gr[:, 1], size=3.00, color='#22a')

            p3.scatter(takeoffs[:, 0], takeoffs[:, 2], takeoffs[:, 1], size=0.40, color='#222')

        if flysim.shape[0] > 0:
            p3.scatter(flysim[:, 0], flysim[:, 2], flysim[:, 1], size=0.3, color='#a22')

        # Determine the right filename to use
        fnameOut = os.path.join(dir, 'snippets/takeoff' + suffix + '.html')
        k = 'snippets_takeoffs' + suffix
        i = 0
        if k in data and isinstance(data[k], list):
            i = len(data[k])
        fnameOutMod = fnameOut.replace('.html','_'+str(i)+'.html') if i > 0 else fnameOut

        # If we're embedding, write to the main directory, as we're deleting anyway...
        # This also copies the .js file to the right directory
        if embed:
            fnameOut.replace('snippets/','')

        # Save the html to file
        print("Writing: "+fnameOutMod)
        p3.save(fnameOutMod, copy_js=True, makedirs=True)

        # Save the filename or file contents to the 'data' variable, which will be used
        # to generate the index.html file.
        if not embed:
            c = fnameOutMod
        else:
            with open(fnameOutMod, 'r') as f:
                c = f.read().replace('"', '&quot;')
            # We don't need the HTML snippet anymore
            os.remove(fnameOutMod)

        # In addition to the file contents/url, save any metadata
        keycontent = [(k, c)]
        for key, content in metadata.items():
            keycontent.append( (key, content) )
        for key, content in keycontent:
            if key not in data:
                data[key] = content
            else:
                if not isinstance(data[key], list):
                    data[key] = [data[key], content]
                else:
                    data[key].append(content)

    # ===================================================
    # Plot FlySim (sparsely)
    # ===================================================

    fnt = fname.replace('.raw.msgpack', '').replace('.msgpack', '') + '.flysim.tracking.csv'
    flysim = trajectories.getFlysims(fname)
    if os.path.isfile(fnt) and flysim is not None:

        d = pd.read_csv(fnt)
        d = d[d.trajectory.isin(flysim.flysimTraj)]
        if len(d) > 0:
            d.loc[:, 'type'] = 'flysim'
            _plot(d, '_flysim', reduce=8)

        # ===============================================
        # Plot non-flysim unID'ed marker trajectories
        # ===============================================

        d = pd.read_csv(fnt)
        d = d[~d.trajectory.isin(flysim.flysimTraj)]
        if len(d) > 0:
            d.loc[:, 'type'] = 'flysim'
            _plot(d, '_nonflysim', reduce=8)

    # ===================================================
    # Plot all flythrough trajectories, no matter how short, or
    # how little perching occurred
    # ===================================================

    # Plot all trajectories
    fn  = fname.replace('.raw.msgpack', '').replace('.msgpack', '') + '.takeoffs.csv'
    fnt = fname.replace('.raw.msgpack', '').replace('.msgpack', '') + '.tracking.csv'
    d = pd.read_csv(fnt)
    if len(d) > 0:
        d.loc[:, 'type'] = 'yf_center'

        md = [
            'takeoff_individual_rejected_startframe',
            'takeoff_individual_rejected_endframe',
            'takeoff_individual_rejected_trajectory',
            'takeoff_individual_rejected_time'
        ]

        print("Plotting rejected flights")
        g = d.groupby('trajectory')
        i = 0
        for _, t in g:
            if t.shape[0] > 0:
                if not t.frame.isin(mocap.frame).any():
                    print("Plot individual rejected...")
                    _plot(t, '_individual_rejected', embed=False,
                          metadata={ md[0]: t.frame.min(),
                                     md[1]: t.frame.max(),
                                     md[2]: t.trajectory.iloc[0],
                                     md[3]: t.time.iloc[0]})
                    i += 1

        for mdk in md:
            if not mdk in data: data[mdk] = []

        print("Plotting all flights")
        _plot(d, '_allflights', reduce=10, quiver=False)

    # ===================================================
    # Plot each trajectory individually
    # ===================================================

    md = [
        'takeoff_individual_startframe',
        'takeoff_individual_endframe',
        'takeoff_individual_trajectory',
        'takeoff_individual_time'
    ]

    g = mocap.groupby('trajectory')
    i = 0
    for _, t in g:
        if t.shape[0] > 0:
            _plot(t, '_individual', embed=False, all=True,
                  metadata={md[0]: t.frame.min(),
                            md[1]: t.frame.max(),
                            md[2]: t.trajectory.iloc[0],
                            md[3]: t.time.iloc[0]})
            i += 1

    for mdk in md:
        if not mdk in data: data[mdk] = []

    # ===================================================
    # Plot all takeoffs
    # ===================================================

    _plot(mocap)

    # ===================================================
    # Plot all markers in frames that were neither rejected nor accepted...
    # ===================================================

    fnt = fname.replace('.raw.msgpack', '').replace('.msgpack', '') + '.tracking.csv'
    xyz = fnt.replace('.tracking.csv', '.xyz.csv')
    if os.path.isfile(xyz):
        d = pd.read_csv(fnt)
        d.loc[:, 'type'] = 'yf_center'
        dxyz = pd.read_csv(xyz)
        dxyz = dxyz[dxyz.markerset.str.contains('yframe', case=False, regex=False)]
        dxyz = dxyz[~dxyz.frame.isin(d.frame)]
        dxyz.loc[:, 'type'] = 'yf_center'
        dxyz.loc[:, 'trajectory'] = -1

        _plot(dxyz, '_nontrajectory')

    # ===================================================
    # Hack: Fix a few things in ipyvolume:
    # ===================================================

    for prefix in ['', 'snippets/']:
        fnameIpyvol = os.path.join(dir, prefix+'ipyvolume.js')
        if os.path.exists(fnameIpyvol):
            with open(fnameIpyvol, 'r', encoding="utf8") as f:
                ipyvol = f.read()
                ipyvol = ipyvol.replace(
                    'this.control_trackball.noPan = true;',
                    'this.control_trackball.noPan = false;')
            with open(fnameIpyvol, 'w', encoding="utf8") as f:
                f.write(ipyvol)

    # Done!
    pass

# ========================================================
# Trial statistics
# ========================================================

def plotTrialStatistics(dir, data):
    # Plot trial statistics
    dataTrials = pd.read_csv(data['file'].replace('.msgpack','')+'.trials.csv', sep=',')
    dataTrials = dataTrials.rename(columns=lambda x: x.strip())

    fig = plt.figure(figsize=(8,6), dpi=IMG_DPI)
    ax1 = fig.add_subplot(221)
    ax2 = fig.add_subplot(222)
    ax3 = fig.add_subplot(223)
    ax4 = fig.add_subplot(224)

    sns.distplot(dataTrials['speed'], ax=ax1)
    sns.distplot(dataTrials['speedchange'], ax=ax2)
    sns.distplot(dataTrials['height'], ax=ax3)
    sns.distplot(dataTrials['wait']  , ax=ax4)
    
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
    
    data['report_date'] = ntpath.basename(data['file']).\
        replace('.raw.msgpack','').replace('.msgpack','')
    
    data['timespan'] = ''

# ========================================================
# Plot trajectory properties
# ========================================================

def plotTrajectoryProperties(dir, data):
    outName = 'traj_statistics.png'
    outFile = dir + outName

    # Load takeoff statistics
    dataTakeoffs = trajectories.getTakeoffs(data['file'])

    if not os.path.exists(outFile):
        # Of the takeoffs, what is the bounding box size distribution?
        fig = plt.figure(figsize=(12,8), dpi=600)
        ax1 = fig.add_subplot(121)
        ax2 = fig.add_subplot(122)

        # Plot:
        d = dataTakeoffs[dataTakeoffs.perchframes>200] #<(200*3600*1)]
        sns.distplot(d.perchframes/(200*60), ax=ax1, bins=20)

        # Plot:
        d = dataTakeoffs[dataTakeoffs.perchframes>=200]
        sns.distplot(d.bboxsize, ax=ax2, bins=20)

        # Set titles/labels
        ax1.set_title("Perching Length Distribution Before Takeoff")
        ax2.set_title("Bounding Box Size Distribution of Takeoffs")

        ax1.set(xlabel='Perching Time in Minutes', ylabel='Proportion')
        ax2.set(xlabel='Bounding Box Diameter', ylabel='Proportion')

        # Save plot
        fig.savefig(outFile, dpi=IMG_DPI)
        plt.close(fig)

    data['srcTrajStatistics'] = outName

    # Save number of takeoffs
    data['numTakeoffs'] = len(dataTakeoffs)

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
# Plot velocity
# ========================================================

def plotFlysimVelocity(dir, data):
    
    outName = ''
    
    fs = util.loadFlySim(data['file'])
    # TEMPORARY: FILTER OUT BY HEIGHT, THIS CONDITION IS NOW INCORPORATED INTO extract_flysim.py
    def f(x):
        x['minz'] = x['flysim.z'].min()
        x['zspan'] = x['flysim.z'].max() - x['flysim.z'].min()
        return x
    fs = fs.groupby(['trajectory',]).apply(f)
    fs = fs[ (fs.is_flysim) & (fs.zspan<50) & (fs.minz>100) ]
    
    # 
    
    data['srcTrajectoryVelocity'] = outName

# ========================================================
# Plot all flysim poinst to indicate the range of trajectories
# ========================================================

def plotFlysim3D(dir, data):
    
    # Load flysim tracking data
    fs = util.loadFlySim(data['file'])
    
    # Plot in 3D
    outName = 'flysim3d_unfiltered.png'
    plot3D(fs, fs.trajectory, dir+outName, cols=['flysim.x','flysim.y','flysim.z'])
    data['srcFlysim3D_unfiltered'] = outName
    
    # Plot, by now filter out trials unlikely to be flysim (i.e. unrecognized)
    outName = 'flysim3d.png'

    # TEMPORARY: FILTER OUT BY HEIGHT, THIS CONDITION IS NOW INCORPORATED INTO extract_flysim.py
    #d = d[(d.is_flysim) & (d.zspan < 50) & (d.minz > 100)]
    d = fs[fs.is_flysim]

    plot3D(d, d.trajectory, dir+outName, cols=['flysim.x','flysim.y','flysim.z'])
    
    data['srcFlysim3D'] = outName
    
    # Plot, now center point only, without noise
    outName = 'flysim3d_nonoise.png'
    d = d[d.istraj]
    plot3D(d, d.trajectory, dir+outName, cols=['flysim.x','flysim.y','flysim.z'])
    data['srcFlysim3D_nonoise'] = outName

    gc.collect()

# ========================================================
# Process file
# ========================================================

def processFile(file):
    try:
        # Determine output directory/files
        dir = os.path.abspath(os.path.join(os.path.dirname(__file__),DIR_REPORTS))
        fname = ntpath.basename(file)
        dir = os.path.join(dir, fname[:(fname.find('_') if '_' in fname
                                       else len(fname))].replace('.msgpack','') + '/')
        outFile = dir + 'index.html'

        # Initialize report directory
        # shutil.rmtree(dir, ignore_errors=True)
        srcPath = os.path.join(os.path.dirname(os.path.abspath(__file__)),'templates/static/')
        dir_util.copy_tree(srcPath, dir)

        # Create Jinja environment (for HTML template rendering)
        envPath = os.path.join(os.path.dirname(os.path.abspath(__file__)),'templates/')
        env = Environment(loader=FileSystemLoader(envPath))
        template = env.get_template('index.html')

        # Initialize report data
        data = {}
        data['file'] = file
        data['errors'] = []

        # Generate data
        for f in [ \
            plotTrajectories3DInteractive,
            plotTrajectoryProperties, plotDailyActivity,
            plotTrialStatistics, plotFlysim3D, plotPerchingLocations2D, \
            loadMetadata, plotTrajectories3D]:
            # Due to missing data, etc., some functions occasionally fail. However, we don't want this to crash
            # the generation of the remaining part of the report, so catch and report any exception here
            if DEBUG:
                f(dir, data)
            else:
                try:
                    # Compute data
                    f(dir, data)
                except Exception as e:
                    print(str(e))
                    traceback.print_exc()
                    # Log this error, so it can be displayed in the HTML report
                    data['errors'].append( str(e) )
                    # Log to an error log file as well
                    with open(dir + 'log.txt','a') as f:
                        f.write(traceback.format_exc() + '\n\n')

        # Write template
        with open(outFile, 'w') as fo:
            fo.write(template.render(data).replace(u'\ufeff',u''))
    except Exception as e:
        if DEBUG:
            raise
        traceback.print_exc()
        print(e)
    
# ========================================================
# Entry point
# ========================================================

def run(async=False, settings=None):
    if SINGLEFILE != "":
        processFile(SINGLEFILE)
    else:
        if settings is None:
            settings = util.askForExtractionSettings(
                allRecentFilesIfNoneSelected=True)

        if SYNCHRONOUS or len(settings.files) == 1:
            for file in settings.files:
                processFile(file)
        else:
            with multiprocessing.Pool(processes=12) as pool:
                (pool.map_async if async else pool.map)(processFile, settings.files)
                return pool
    
    return None
    
if __name__ == "__main__":

    run()
