
# =======================================================================================
# Imports
# =======================================================================================

import os
from datetime import datetime
import pandas as pd
import numpy as np
from ggplot import *
from mpl_toolkits.mplot3d import Axes3D
from matplotlib import cm
from matplotlib.ticker import LinearLocator, FormatStrFormatter
import matplotlib.pyplot as plt
from mpl_toolkits.axes_grid1 import make_axes_locatable
from scipy.interpolate import Rbf, griddata
import to_docx, process_performance

os.chdir(os.path.dirname(os.path.abspath(__file__)))

# =======================================================================================
# Globals
# =======================================================================================

cmap = cm.get_cmap('seismic')

data, dataOld, dataNew = None, None, None

# =======================================================================================
# Load data
# =======================================================================================

def loadData(fnames):
    global data, dataOld, dataNew
    
    dataOld = pd.read_csv(fnames[1][1].replace('.log','') + '.csv')
    dataNew = pd.read_csv(fnames[0][1].replace('.log','') + '.csv')

    dataOld['z'] = 0.5 * (dataOld.z1 + dataOld.z2)
    dataNew['z'] = 0.5 * (dataNew.z1 + dataNew.z2)

    dataOld['device'] = 'old'
    dataNew['device'] = 'new'
    
    dataOld['signal'] = np.zeros( dataOld.shape[0] )
    dataNew['signal'] = np.zeros( dataNew.shape[0] )
    
    data = pd.concat( (dataNew,dataOld) )

# =======================================================================================
# Interpolate and pre-process data
# =======================================================================================

def interp(data, v, transform='mean'):
    data['za'] = (data.z/50).round()*50
    data['xa'] = (data.x/40).round() * 40
    data.logErrorRate[np.isinf(data.logErrorRate)] = -7
    data['zal'] = ['Z='+str(int(x)).zfill(3) for x in data['za']]
    
    d = data.copy()
    #d[v][np.isinf(d[v])] = -6
    if v=='signal':
        def _d(x): return datetime.strptime(x,'%Y-%m-%d %H:%M:%S.%f')
        d['pos_duration'] = [(_d(r['time_pos_end'])-_d(r['time_pos'])).total_seconds() 
            for i,r in d.iterrows()]
        d = d.groupby(['xa','za','zal','device','pos_duration']).count().\
            reset_index().rename_axis({'Unnamed: 0':'numsamples'}, axis='columns')
        d['signal'] = d.numsamples / d.numsamples.max()
        d['x'] = d.xa
        d['z'] = d.za
    elif transform=='mean':
        d = d.groupby(['x','z']).mean().reset_index() # Does this mean drop NAs?
    elif transform=='std':
        d = d.groupby(['x','z']).std().reset_index() # Does this std drop NAs?
    else:
        raise Exception("Unknown transformation requested") 
    
    #x = np.linspace(-580, 580, 1170/30)
    #y = np.linspace( -40, 560, 600/40)
    #x = np.linspace(-25*28, 25*28, 56)
    #y = np.linspace(-30* 1, 30*25, 26)
    x = np.linspace(-40 * 17, 40*17, 34)
    y = np.linspace(-50 *  1, 50*14, 15)
    
    xi, yi = np.meshgrid(x, y)
    zi = None
    if transform=='mean':
        zi = griddata((d.x.values, d.z.values), d[v].values, (xi, yi), method='cubic')
    elif transform=='std':
        zi = griddata((d.x.values, d.z.values), d[v].values, (xi, yi), method='nearest')
        _, _, _zi, _, _, _, _ = interp(data, v)
        _zi[~np.isnan(_zi)] = 1
        zi = np.multiply(zi, _zi)
        
    return (xi, yi, zi, d.x.values, d.z.values, d[v].values, d)


# =======================================================================================
# Plot
# =======================================================================================

def processPlotType(pltType):    
    
    # ===============================================================================
    # Faceted plots
    # ===============================================================================
        
    if pltType == 'facets':
            
        for vi, v in zip(range(4), ['signal','logErrorRate','PowerV','MissingFrames']):
            data2 = None
            if v == 'signal':
                _, _, _, _, _, _, data2 = interp(data, v='signal')
                data2['signalMin'] = data2['signalMax'] = data2.signal
                data2['signal_mean'] = data2.signal
            else:
                data2 = data.groupby(['xa','zal','device']).agg(['mean','std']).reset_index()
                data2.columns = pd.Index([e[0]+'_'+e[1] if e[1]!='' else e[0] for e in data2.columns.tolist()])
                data2[v+'_std'].fillna(0, inplace=True)
                data2[v+'Min'] = data2[v+'_mean'] - data2[v+'_std']
                data2[v+'Max'] = data2[v+'_mean'] + data2[v+'_std']
            
            # Plot:
            p = ggplot(aes(x='xa', y=v+'_mean', ymin=v+'Min', ymax=v+'Max', color='device'), 
                    data=data2) + geom_line() + geom_ribbon(alpha=0.3) + facet_wrap('zal') + xlab("X Position") + \
                    ylab(v) + theme(axis_text_x  = element_text(angle = 90, hjust = 1))
            p.save('facets_'+v+'.png', dpi=600)
        
    # ===============================================================================
    # 2D and 3D plots
    # ===============================================================================
    
    elif pltType in ['2d','3d']:
        
        fig = plt.figure(figsize=(20,24), facecolor='w')
        
        for vi, v in zip(range(4), ['logErrorRate','PowerV','MissingFrames','signal']):
            
            for transform in (['std','mean'] if v!='signal' else ['mean',]):
                xio, yio, zio, dxo, dzo, dvo, do = interp(dataOld, v, transform=transform)
                xin, yin, zin, dxn, dzn, dvn, dn = interp(dataNew, v, transform=transform)
            
                for i, l, zi in [(1, 'New', zin), (2, 'Old', zio), (3, 'New-Old', zin-zio)]:
                
                    vmin = min(np.nanmin(zio), np.nanmin(zin))
                    vmax = min(np.nanmax(zio), np.nanmax(zin))
                    if l == 'New-Old':
                        # Use different extrema for difference
                        vmin = np.nanmin(zi)
                        vmax = np.nanmax(zi)
                    
                    zi[np.isnan(zi)] = vmin 
                
                    ax = fig.add_subplot(7,3,6*vi + i + (0 if transform=='mean' else 3), \
                                            projection='3d' if pltType=='3d' else 'rectilinear')

                    ax.set_xlabel("X")
                    ax.set_ylabel("Z")
                    ax.set_title(v + "(" + l + ") [" + transform + "]")

                    # ===================================================================
                    # 2D Plot
                    # ===================================================================
                    
                    if pltType=='2d':
                        im = ax.imshow(zi, interpolation='none', extent=[
                            np.min(xio),np.max(xio),np.min(yio),np.max(yio)]) #, aspect=0.5)
                            
                        #ax.set_xticklabels(xio[0,:].astype('int'))
                        #ax.set_yticklabels(yio[:,0].astype('int'))

                        # See: http://stackoverflow.com/questions/18195758/set-matplotlib-colorbar-size-to-match-graph
                        # create an axes on the right side of ax. The width of cax will be 5%
                        # of ax and the padding between cax and ax will be fixed at 0.05 inch.
                        divider = make_axes_locatable(ax)
                        cax = divider.append_axes("right", size="5%", pad=0.05)

                        plt.colorbar(im, cax=cax)
                    
                    # ===================================================================
                    # 3D Plot
                    # ===================================================================
                    
                    elif pltType=='3d':
                        surf = ax.plot_surface(xin, yin, zi,
                                                rstride = 1, cstride = 1, cmap=cmap,
                                                vmin=vmin, vmax=vmax,
                                                linewidth=0.05, antialiased=True)
                        ax.set_zlabel(v)
        
        # Close 2D/3D plot
        fig.savefig('telemetry_'+pltType+'.png', dpi=500, transparent=False, facecolor='w')
        plt.close(fig)

# =======================================================================================
# Entry point
# =======================================================================================

def run(fnames):    
    
    pltTypes = ['2d', '3d', 'facets']
    
    # Load datasets
    loadData(fnames)
    
    # Draw each plot
    for pltType in pltTypes:
        processPlotType(pltType)
    
    # Conver to document!
    to_docx.run(fnames)
    
    print("Done!")

if __name__ == "__main__":
    
    fnames = process_performance.getFilenames()

    # Change the working directory (i.e. output directory) to the directory of the first file 
    # (presumably all four files are in the same directory)
    outdir = os.path.dirname(fnames[0][0])
    os.chdir(outdir)
    
    run(fnames)