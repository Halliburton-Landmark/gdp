#!/usr/bin/env python

"""
* Common parameters:
    These are the parameters that are common across all plots, such as
    the name of the log(s), start time, end time, height, width, etc.

	An example of such parameters is:

    >  log=edu.berkeley.eecs.swarmlab.device.c098e5300003\
      &log=edu.berkeley.eecs.swarmlab.device.c098e530000a\
      &start=1473225641.397882\
      &end=1475817641.397882\
      &height=200\
      &width=800\
"""

import gdp
from gdp.MISC import EP_STAT_Exception
from GDPcache import GDPcache

import sys
import json
import time
import traceback
from datetime import datetime

from bokeh.io import curdoc
from bokeh.models.sources import ColumnDataSource
from bokeh.layouts import row, column
from bokeh.plotting import figure
from bokeh.models.widgets import Paragraph


## A list of parameters (and default values) that will be common to
##  all the plots (in case there are more than 1 plots)
# common_defaults = {
#     "log"      : ["edu.berkeley.eecs.swarmlab.device.c098e5300003"],
#     "start"     : cur_time-3600.0,
#     "end"       : cur_time,
#     "height"    : 400,
#     "width"     : 900,
#     }
# 
# plot_defaults = {
#     "title" : "Temperature",
#     "keys"  : ["temperature_celcius"],
#     }
# 

# First, find out how many plots do we have to generate? We support
#   an arbitrary number of plots (ideally, let us keep it a small number)
#
# i-th plot is specified by (i starting from 0):
# { 'plot_i_title': 'x',
#   'plot_i_keys' : ['k1', 'k2']}


########### Global variables ###########
# these are the variables we'd like everyone to inherit

args = None
cur_time = 0.0
 

############ Classes ###################

class GDPPlot:
    """ Just a structure to keep things organized """

    logs = []       # A list of logs from which the data is sourced
    start = 0.0     # Start time (definitely non-zero when initialized)
    end = 0.0       # Can be zero
    title = ""      # Title of this plot
    keys = []       # keys in a JSON record from logs that this plot plots
    figure = None   # Bokeh figure object that represents this plot    
    sources = []    # Bokeh's ColumnDataSource = len(logs) * len(keys)

    def __init__(self, logs, start, end, title, keys):
        """ The items that should be known to begin with """
        self.logs = logs
        self.start = start
        self.end = end
        self.title = title
        self.keys = keys


############ Functions go here ##########

def generatePlot(lines, title, height, width):

    # lines is a list of (linename, [t0, t1...], [v0, v1...]) items,
    #     that ought to be plotted as individual lines.

    colors = ['red', 'green', 'blue', 'orange', 'black']
    plot = figure(plot_width=width, plot_height=height, tools='',
                        toolbar_location=None,
                        x_axis_type='datetime', title=title)

    for i in xrange(len(lines)):
        _legend, _source = lines[i]
        color = colors[i]   # Hope nobody comes along asking for 20 plots

        plot.line('x', 'y', source=_source, color=color)

    return plot



def parseCommonArgs():
    # Parse arguments common to all plots

    global args

    common_args = {}
    common_args['log']      = args.get('log')
    common_args['start']    = float(args.get('start', [cur_time-86400.0])[0])
    common_args['end']      = float(args.get('end', [cur_time])[0])
    common_args['height']   = int(args.get('height', [200])[0])
    common_args['width']    = int(args.get('width', [800])[0])
    
    # For sanity checks
    print "Common parameters for all plots", common_args
    return common_args



def parsePlots(common_args):
    # Parse arguments for individual plots (as passed in URL)
    # Can return empty. In that case, a fallback is to plot all

    global args

    plot_specific_keys = [k for k in args.keys() if k.startswith("plot_")]
    plot_specific_keys.sort()
    
    # plot_specific_keys should look exactly like
    #   [ 'plot_0_keys', 'plot_0_title', 'plot_1_keys', 'plot_1_title', ... ]
    assert len(plot_specific_keys)%2 == 0
    
    plots = []
    num_plots = len(plot_specific_keys)/2
    for i in xrange(num_plots):
        keys = args.get('plot_%d_keys' % i)
        title = args.get('plot_%d_title' % i)
        assert len(keys)>0
        assert len(title)==1

        p = GDPPlot(common_args['log'], common_args['start'],
                        common_args['end'], title, keys)
    
    return plots 



def getGDPdata(logs, start, end):
    # Fetch the data from GDP. This returns records parsed as JSON
    #   objects. Does not attempt to do any filtering based on keys

    # Returned data is a list of tuples; each tuple has the following
    #   format:
    #   (logname, Xvals, Yvals)         # Xvals, Yvals are lists

    alldata = {}
    for l in logs:

        # TODO: make a shared cache of caches

        ## This is the only part where any GDP specific code runs
        __gc = None
        try:
            __gc = GDPcache(l)
            __l_data = __gc.getRange(start, end)
            del __gc
        except EP_STAT_Exception as e:
            del __gc
            raise e
        ## End of GDP specific code
    
        X, Y = [], []
        for r in __l_data:
            x = datetime.fromtimestamp(r["ts"]["tv_sec"] +
                                        (r["ts"]["tv_nsec"]*1.0)/(10**9))
            y = json.loads(r["data"])
            X.append(x)
            Y.append(y)
    
        # l is the name of the log,
        # X is a list of timestamps
        # Y is a list of parsed JSON dictionaries
        alldata[l] = (X, Y)

    return alldata 


def init():
    ## This is what everyone should call to start with
    global args
    global cur_time
    args = curdoc().session_context.request.arguments
    print "GET parameters", args
    cur_time = time.time()
    # sys.excepthook = excepthook

