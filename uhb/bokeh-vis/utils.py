#!/usr/bin/env python

"""
* Common parameters:
    These are the parameters that are common across all plots, such as
    the name of the log(s), start time, end time, height, width, etc.

	An example of such parameters is:

    >  log_0=edu.berkeley.eecs.swarmlab.device.c098e5300003\
      &log_1=edu.berkeley.eecs.swarmlab.device.c098e530000a\
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
colors = ['black', 'red', 'green', 'blue', 'orange', 'yellow'] 
line_styles = ['solid', 'dashed', 'dotted', 'dotdash', 'dashdot']

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
                    #   it is a row major representation, each row represents
                    #   data from a single log

    def __init__(self, logs, start, end, title, keys):
        """ The items that should be known to begin with """
        self.logs = logs
        self.start = start
        self.end = end
        self.title = title
        self.keys = keys


    def initFigure(self, width, height):
        self.figure = figure(plot_width=width, plot_height=height,
                                tools='', toolbar_location=None,
                                x_axis_type='datetime', title=self.title)
        self.figure.legend.location = "top_left"


    def __getLegend(self, logname, keyname):
        """ Returns a smart legend, only information that is necessary"""

        # get log number
        lognum = self.logs.index(logname)

        if len(self.logs)==1 and len(self.keys)==1:
            legend = None
        elif len(self.logs)==1 and len(self.keys)>1:
            legend = str(keyname)
        elif len(self.logs)>1 and len(self.keys)==1:
            legend = str(lognum)
        else:
            legend = "%d: %s" %(lognum, keyname)

        return legend 


    def initData(self, data):
        """
        Initialize the data for this specific plot using the raw data.
        data is a dictionary of { lognames => (X,Y) }
        => Involves filtering and updating self.sources
        Remeber, self.sources is a row major representation
            of a matrix, with each row representing logs, columns represent
            a single key
        """

        self.sources = []
        _log_ctr = 0

        for l in self.logs:

            (X, _Y) = data[l]        # _Y is the list of raw JSON recs
            _key_ctr = 0

            for k in self.keys:

                Y = [t[k] for t in _Y]
                s = ColumnDataSource(dict(x=X, y=Y))
                self.figure.line('x', 'y', source=s,
                                line_color=colors[_log_ctr],
                                line_dash=line_styles[_key_ctr],
                                legend=self.__getLegend(l,k))
                self.sources.append(s)
                _key_ctr += 1

            _log_ctr += 1


    def updateData(self, data):
        """
        Similar to initData, except that we just need to update already
            existing sources. We still need to do filtering, however.
        """
        ctr = 0
        rollover = max([len(_s.data['x']) for _s in self.sources])
        for l in self.logs:
            (X, _Y) = data[l]
            for k in self.keys:
                Y = [t[k] for t in _Y]
                self.sources[ctr].stream(dict(x=X, y=Y), rollover=rollover)
                ctr += 1


############ Functions go here ##########

def parseCommonArgs():
    # Parse arguments common to all plots

    global args

    common_args = {}

    # We'd like to label plots with log numbers. In order for that, we need
    #   to ensure that ordering doesn't get messed up. We can't just ask
    #   for 'log' and get a list. Instead, we need to ask for 'log_0'...
    num_logs = 0
    for k in args.keys():
        if k.startswith('log_'):
            num_logs+=1
    logs = []
    for i in xrange(num_logs):
        _k = "log_%d" % i
        logs.append(args[_k][0]) 
    common_args['log']      = logs

    common_args['start']    = float(args.get('start', [cur_time-86400.0])[0])
    common_args['end']      = float(args.get('end', [cur_time])[0])
    common_args['height']   = int(args.get('height', [200])[0])
    common_args['width']    = int(args.get('width', [800])[0])
    common_args['type']     = args.get('type', ['oneD'])[0]
    
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
                        common_args['end'], title[0], keys)
        plots.append(p)
    
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

