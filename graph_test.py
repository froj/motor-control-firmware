# -*- coding: utf-8 -*-
"""
This example demonstrates many of the 2D plotting capabilities
in pyqtgraph. All of the plots may be panned/scaled by dragging with
the left/right mouse buttons. Right click on any plot to show a context menu.
"""

# import initExample ## Add path to library (just for examples; you do not need this)


from pyqtgraph.Qt import QtGui, QtCore
import numpy as np
import pyqtgraph as pg
import thread

#QtGui.QApplication.setGraphicsSystem('raster')
app = QtGui.QApplication([])
#mw = QtGui.QMainWindow()
#mw.resize(800,800)

win = pg.GraphicsWindow(title="INS-board: dataplot")
win.resize(1000,600)

# Enable antialiasing for prettier plots
pg.setConfigOptions(antialias=True)



class RingBuffer(object):
    ''' from http://stackoverflow.com/a/19157187 '''

    def __init__(self, size_max, default_value=0.0, dtype=float):
        """initialization"""
        self.size_max = size_max

        self._data = np.empty(size_max, dtype=dtype)
        self._data.fill(default_value)

        self.size = 0

    def append(self, value):
        """append an element"""
        self._data = np.roll(self._data, 1)
        self._data[0] = value

        self.size += 1

        if self.size == self.size_max:
            self.__class__  = RingBufferFull

    def get_all(self):
        """return a list of elements from the oldest to the newest"""
        return(self._data)

    def get_partial(self):
        return(self.get_all()[0:self.size])

    def __getitem__(self, key):
        """get element"""
        return(self._data[key])

    def __repr__(self):
        """return string representation"""
        s = self._data.__repr__()
        s = s + '\t' + str(self.size)
        s = s + '\t' + self.get_all()[::-1].__repr__()
        s = s + '\t' + self.get_partial()[::-1].__repr__()
        return(s)

class RingBufferFull(RingBuffer):
    def append(self, value):
        """append an element when buffer is full"""
        self._data = np.roll(self._data, 1)
        self._data[0] = value




class PlotVar:
    def __init__(self, plot, name=None, maxhist=1000):
        self.name = name
        self.maxhist = maxhist
        self.buffer = RingBuffer(maxhist)
        self.plot = plot.plot(pen='y')

    def updatePlot(self, histlen):
        self.plot.setData(self.buffer.get_partial()[0:histlen])

    def updateData(self, datapt):
        self.buffer.append(datapt)


class Plot:
    def __init__(self, window, name='noname', histlen=1000):
        self.plt = window.addPlot(title=name)
        self.histlen = 100
        self.maxhistlen = histlen
        self.lines = []
        self.plt.sigYRangeChanged.connect(lambda: self._updateRegionHanlder())

    def addLine(self, name=None):
        l = PlotVar(self.plt, name=name, maxhist=self.maxhistlen)
        self.lines.append(l)
        return l

    def update(self):
        self.plt.setXRange(0, self.histlen, padding=0, update=False)
        # for l in self.lines:
            # l.updateData(np.random.normal())
        for l in self.lines:
            l.updatePlot(self.histlen)
        self.plt.enableAutoRange('y', True)

    def _updateRegionHanlder(self):
        d = self.plt.getViewBox().viewRange()[0]
        self.histlen = d[1] - d[0]
        if self.histlen > self.maxhistlen:
            self.histlen = self.maxhistlen


p1 = Plot(win, 'motor_current')
p1.addLine()

p2 = Plot(win, 'motor_voltage')
p2.addLine()

p3 = Plot(win, 'speed')
p3.addLine()

plotlist = [p1, p2, p3]

def plotupdate():
    for p in plotlist:
        p.update()

timer = QtCore.QTimer()
timer.timeout.connect(plotupdate)
timer.start(50)

import sys
import os
from serial_datagram import *
import msgpack
import serial



class DatagramRcv(QtCore.QThread):
    def __init__(self, fdesc):
        self.fdesc = fdesc
        super(DatagramRcv, self).__init__()

    def run(self):
        for dtgrm in SerialDatagram(self.fdesc).receive():
            # print('size = ' + str(len(dtgrm)) + ' bytes')
            data = msgpack.unpackb(dtgrm)
            # print(data)
            if 'i' in data:
                p2.lines[0].updateData(data['motor_current'])
            if 'u' in data:
                p2.lines[0].updateData(data['motor_voltage'])
            if 's' in data:
                p1.lines[0].updateData(data['speed'])


if len(sys.argv) > 2:
    baud = sys.argv[2]
else:
    baud = 115200

fdesc = serial.Serial(sys.argv[1], baudrate=baud)

thread = DatagramRcv(fdesc)
thread.start()

## Start Qt event loop unless running in interactive mode or using pyside.
if __name__ == '__main__':
    import sys
    if (sys.flags.interactive != 1) or not hasattr(QtCore, 'PYQT_VERSION'):
        QtGui.QApplication.instance().exec_()
