
"""
Demonstrates use of GLScatterPlotItem with rapidly-updating plots.
"""

## Add path to library (just for examples; you do not need this)
from pyqtgraph.Qt import QtCore, QtGui
import pyqtgraph.opengl as gl
import numpy as np
import pandas as pd

app = QtGui.QApplication([])
w = gl.GLViewWidget()
w.opts['distance'] = 20
w.show()
w.setWindowTitle('pyqtgraph example: GLScatterPlotItem')

g = gl.GLGridItem()
w.addItem(g)

data = pd.read_csv('../data/2016-12-14 12-10-17.flysim.tracking.csv')

pos = data.as_matrix(['flysim.x','flysim.y','flysim.z'])
size = np.empty((pos.shape[0]))
color = np.empty((pos.shape[0], 4))

for i in range(pos.shape[0]):
    size[i] = 5
    color[i] = (1,0,0,1)
    
sp1 = gl.GLScatterPlotItem(pos=pos, size=size, color=color, pxMode=False)
w.addItem(sp1)

## Start Qt event loop unless running in interactive mode.
if __name__ == '__main__':
    import sys
    if (sys.flags.interactive != 1) or not hasattr(QtCore, 'PYQT_VERSION'):
        QtGui.QApplication.instance().exec_()
