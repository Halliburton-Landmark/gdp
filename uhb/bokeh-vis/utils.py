#!/usr/bin/env python

"""
See README.md
"""


import gdp
from gdp.MISC import EP_STAT_Exception
from GDPcache import GDPcache

import sys
import json
import time
import traceback
from datetime import datetime
import colorsys

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

def numToColor(num):
    """ mapss a number from 0.0 to 1.0 to an RGB value """

    # uses the hsv gradient
    #   red is hue 0, green is hue 0.3

    r,g,b = colorsys.hsv_to_rgb((1.0-num)*0.3,1,1)
    color = "#%02x%02x%02x" % (r*255,g*255,b*255)
    return color


############ Classes ###################

class NoDataException(Exception):
    pass

class NoPlotsSpecifiedException(Exception):
    pass

class GDPPlot:
    """
    A structure to keep things organized. Note that bokeh has the
    concept of a 'figure'. Within each 'figure', there can be multiple
    'lines', or other kind of a things (a scatter plot, for example).
    """

    logs = []       # A list of logs from which the data is sourced
    start = 0.0     # Start time (definitely non-zero when initialized)
    end = 0.0       # Can be zero
    title = ""      # Title of this plot

    plot_type = ""  # type of this plot (oneD, twoD-traj, twoD-heat)

    # for one dimensional plots
    keys = []       # keys in a JSON record from logs that this plot plots

    # for two dimensional plots
    key_x = None
    key_y = None
    key_val = None  # this is optional and can stay uninitialized

    figure = None   # Bokeh figure object that represents this plot
    sources = []    # Bokeh's ColumnDataSource = len(logs) * len(keys)
                    #   it is a row major representation, each row represents
                    #   data from a single log

    # These are optional properties. Ideally, these should be set
    #   during the initialization
    height = 400
    width = 400
    x_min = None
    x_max = None
    y_min = None
    y_max = None
    val_min = 0.0
    val_max = 100.0


    def __init__(self, logs, start, end, plot_type, title, keys):
        """ The items that should be known to begin with """
        self.logs = logs
        self.start = start
        self.end = end
        self.plot_type = plot_type
        self.title = title

        # make sure we are not doing something crazy in invocation
        assert plot_type in ("oneD", "twoD-traj", "twoD-heat")

        if plot_type == "oneD":
            self.keys = keys
        else:
            self.key_x, self.key_y = keys[0], keys[1]
            if plot_type == "twoD-heat" and len(keys)>2:
                self.key_val = keys[2]


    def initFigure(self):
        """ setup bokeh figure, does not setup the items within a figure """

        x_range = None
        y_range = None
        if self.plot_type == "oneD":
            x_axis_type = 'datetime'
        elif self.plot_type == "twoD-traj" or self.plot_type == "twoD-heat":
            x_axis_type = 'auto'
            if self.x_min is not None and self.x_max is not None:
                x_range = (self.x_min, self.x_max)
            if self.y_min is not None and self.y_max is not None:
                y_range = (self.y_min, self.y_max)
        else:
            assert False

        self.figure = figure(plot_width=self.width, plot_height=self.height,
                                tools='', toolbar_location=None,
                                x_range=x_range, y_range=y_range,
                                x_axis_type=x_axis_type, title=self.title)

        self.figure_shapes = [self.figure.x, self.figure.asterisk,
                                self.figure.triangle, self.figure.square_x,
                                self.figure.square_cross]


    def __getLegend(self, logname, keyname=None):
        """ Returns a smart legend, only information that is necessary"""

        # get log number
        lognum = self.logs.index(logname)

        if len(self.logs)==1:
            legend = None
            if len(self.keys)>1:
                legend = str(keyname)
        else:
            legend = str(lognum)
            if len(self.keys)>1:
                legend += ": %s" % keyname

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

        self.sources == []

        for (_log_ctr, l) in enumerate(self.logs):

            (ts, _data) = data[l]

            if self.plot_type == "oneD":
                # generate a source with X axis as time, Y axis as value

                for (_key_ctr, k) in enumerate(self.keys):
                    val = [t[k] for t in _data]
                    s = ColumnDataSource(dict(x=ts, y=val))
                    self.figure.line('x', 'y', source=s,
                                line_color=colors[_log_ctr],
                                line_dash=line_styles[_key_ctr],
                                legend=self.__getLegend(l,k))
                    self.sources.append(s)

            else:
                # generate a source based on the keys provided, actual
                #   time of record generation does not really matter

                X = [t[self.key_x] for t in _data]
                Y = [t[self.key_y] for t in _data]
                if self.key_val is not None:
                    val = [t[self.key_val] for t in _data]
                else:
                    val = [1.0]*len(X)

                # generate color based on some guesswork
                _min, _max = self.val_min, self.val_max
                color = [numToColor(((v-_min)/(_max-_min))) for v in val]
                # keep color for trajectory as well, doesn't hurt
                s = ColumnDataSource(dict(x=X, y=Y, color=color))
                self.sources.append(s)

                if self.plot_type == "twoD-traj":
                    self.figure.line('x', 'y', source=s,
                                line_color=colors[_log_ctr],
                                line_dash=line_styles[0],
                                legend=self.__getLegend(l))

                if self.plot_type == "twoD-heat":
                    self.figure_shapes[_log_ctr]('x', 'y',
                                            source=s, size=10,
                                            line_color='color',
                                            fill_color='color',
                                            fill_alpha=0.6,
                                            legend=self.__getLegend(l))


        # for some reason, this doesn't work unless we have a legend
        self.figure.legend.location = "top_left"


    def updateData(self, data):
        """
        Similar to initData, except that we just need to update already
            existing sources. We still need to do filtering, however.
        """
        ctr = 0
        rollover = max([len(_s.data['x']) for _s in self.sources])
        for l in self.logs:

            (ts, _data) = data[l]

            if self.plot_type=="oneD":

                for k in self.keys:
                    val = [t[k] for t in _data]
                    self.sources[ctr].stream(dict(x=ts, y=val),
                                                rollover=rollover)
                    ctr += 1

            else:
                X = [t[self.key_x] for t in _data]
                Y = [t[self.key_y] for t in _data]
                if self.key_val is not None:
                    val = [t[self.key_val] for t in _data]
                else:
                    val = [1.0]*len(X)
                _min, _max = self.val_min, self.val_max
                color = [numToColor(((v-_min)/(_max-_min))) for v in val]

                self.sources[ctr].stream(dict(x=X, y=Y, color=color),
                                                rollover=rollover)
                ctr += 1




############ Functions go here ##########

def parseArgs():

    common_args = parseCommonArgs()
    plot_args = parsePlotSpecificArgs(common_args)

    return common_args, plot_args


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
    return common_args



def parsePlotSpecificArgs(common_args):
    # Parse arguments for individual plots (as passed in URL)
    # Can return empty. In that case, a fallback is to plot all

    global args

    # Get all the plot specific parameters
    plot_keys = [k for k in args.keys() if k.startswith("plot_")]

    # find out how many plots do we have
    plot_ids = set([int(k.split('_')[1]) for k in plot_keys])
    num_plots = len(plot_ids)

    # check whether plot numbers start from 0, and increase by one?
    for i in xrange(num_plots):
        assert i in plot_ids

	# Actual parsing happens here
    plot_args = []
    for i in xrange(num_plots):

        d = {}
        _keys = [k for k in args.keys() if k.startswith("plot_%d_" % i)]
        _short_keys = ['_'.join(k.split('_')[2:]) for k in _keys]

        for k in _short_keys:
            d[k] = args['plot_%d_%s' % (i,k)]

        d['type'] = d.get('type', ['oneD'])[0]
        assert d['type'] in ("oneD", "twoD-traj", "twoD-heat")

        for __k in ['x_min', 'x_max', 'y_min', 'y_max', 'val_min', 'val_max']:
            if __k in _short_keys:
                d[__k] = float(d[__k][0])

        for __k in ['height', 'width']:
            if __k in _short_keys:
                d[__k] = int(d[__k][0])

        # fix lists to non-lists
        d['title'] = d['title'][0]

        if d['type'] == "oneD":
            assert "keys" in _short_keys
        else:
            assert "key_x" in _short_keys
            d['key_x'] = d['key_x'][0]
            assert "key_y" in _short_keys
            d['key_y'] = d['key_y'][0]
            if "key_val" in _short_keys:
                d['key_val'] = d['key_val'][0]

        plot_args.append(d)

    print plot_args

    return plot_args


def initPlots(plot_args, common_args):

    # plot_args is  a list of plot specific keys. In particular,
    # plot_args[i]['key'] is 'plot_i_key' in the original argument list
    # passed by the user. Any sanity checks on user input have already
    # been performed.
    #
    # returns a list of GDPPlot objects, one for each of the plots

    plots = []
    for params in plot_args:
        plot_type = params.get('type', 'oneD')
        plot_title = params['title']
        if plot_type == 'oneD':
            keys = params['keys']
        else:
            keys = [params['key_x'], params['key_y']]
            t = params.get('key_val', None)
            if t is not None:
                keys.append(t)

        p = GDPPlot(common_args['log'], common_args['start'],
                        common_args['end'], plot_type, plot_title, keys)

        # Set up the parameters. UGLY!!!
        if 'height' in params.keys(): p.height = params['height']
        if 'width' in params.keys(): p.width = params['width']
        if 'x_min' in params.keys(): p.x_min = params['x_min']
        if 'x_min' in params.keys(): p.x_min = params['x_min']
        if 'x_min' in params.keys(): p.x_min = params['x_min']
        if 'x_max' in params.keys(): p.x_max = params['x_max']
        if 'y_min' in params.keys(): p.y_min = params['y_min']
        if 'y_max' in params.keys(): p.y_max = params['y_max']
        if 'val_min' in params.keys(): p.val_min = params['val_min']
        if 'val_max' in params.keys(): p.val_max = params['val_max']

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
            if len(__l_data)==0:
                raise NoDataException
            del __gc
        except (EP_STAT_Exception, NoDataException) as e:
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
    cur_time = time.time()
    # sys.excepthook = excepthook

