#!/usr/bin/env python

"""
A generic plotting tool for time-series data. It use a number of logs
    containing JSON data as underlying data-sources. Using such data,
    it can plot multiple plots (for separate keys, or collection of keys).

To execute, run:
    $ python -m bokeh serve oneD.py

The general parameter passing scheme is as follows:

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

* Plot specific parameters:
    Each individual plot differs in title, key(s) plotted.

    An example of such parameters is:

    >  plot_0_title=Light\
      &plot_0_keys=light_lux\
      &plot_1_title=Temperature\
      &plot_1_keys=temperature_celcius\
      &plot_2_title=Humidity\
      &plot_2_keys=humidity_percent


XXX: Finish this
Example URL:

http://localhost:5006/oneD?log=edu.berkeley.eecs.bwrc.device.c098e5300009&log=edu.berkeley.eecs.swarmlab.device.c098e5300003&log=edu.berkeley.eecs.swarmlab.device.c098e530000a&start=1473225641.397882&end=1475817641.397882&height=200&width=800&plot_0_title=Light&plot_0_keys=light_lux&plot_1_title=Temperature&plot_1_keys=temperature_celcius&plot_2_title=Humidity&plot_2_keys=humidity_percent


"""

import gdp
import json
from GDPcache import GDPcache
import time
from datetime import datetime
from bokeh.io import curdoc
from bokeh.layouts import row, column
from bokeh.models import ColumnDataSource
from bokeh.models.widgets import PreText, Select
from bokeh.plotting import figure

## A list of parameters (and default values) that will be common to
##  all the plots (in case there are more than 1 plots)
# common_defaults = {
#     "log"      : ["edu.berkeley.eecs.swarmlab.device.c098e5300003"],
#     "start"     : cur-3600.0,
#     "end"       : cur,
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


def generatePlot(lines, title, height, width):

    # lines is a list of (linename, [t0, t1...], [v0, v1...]) items,
    #     that ought to be plotted as individual lines.

    plot = figure(plot_width=width, plot_height=height, tools='',
                        toolbar_location=None,
                        x_axis_type='datetime', title=title)
    Xs, Ys = [], []
    for (name, X, Y) in lines:
        Xs.append(X), Ys.append(Y)

    colors = ['red', 'green', 'blue', 'orange', 'black'][:len(Xs)]
    plot.multi_line(xs=Xs, ys=Ys, color=colors)

    return plot

# Process arguments and plot the data
args = curdoc().session_context.request.arguments

print args
cur = time.time()

plot_specific_keys = [k for k in args.keys() if k.startswith("plot_")]
plot_specific_keys.sort()

# plot_specific_keys should look exactly like
#   [ 'plot_0_keys', 'plot_0_title', 'plot_1_keys', 'plot_1_title', ... ]
assert len(plot_specific_keys)>0
assert len(plot_specific_keys)%2 == 0

plot_args = []
num_plots = len(plot_specific_keys)/2
for i in xrange(num_plots):
    keys = args.get('plot_%d_keys' % i)
    title = args.get('plot_%d_title' % i)

    assert len(keys)>0
    assert len(title)==1
    plot_args.append({'title': title[0], 'keys': keys })

# For sanity checks
print plot_args

## Also get the common arguments
common_args = {}
common_args['log']      = args.get('log')
common_args['start']    = float(args.get('start', [cur-3600.0])[0])
common_args['end']      = float(args.get('end', [cur])[0])
common_args['height']   = int(args.get('height', [400])[0])
common_args['width']    = int(args.get('width', [400])[0])

# For sanity checks
print common_args


## First fetch the data using common_args
alldata = []
for l in common_args['log']:
    __gc = GDPcache(l)
    __l_data = __gc.getRange(common_args['start'], common_args['end'])

    X, Y = [], []
    for r in __l_data:
        x = datetime.fromtimestamp(r["ts"]["tv_sec"] +
                                    (r["ts"]["tv_nsec"]*1.0)/(10**9))
        y = json.loads(r["data"])
        X.append(x)
        Y.append(y)

    # X is a list of timestamps, Y is a list of parsed JSON dictionaries
    alldata.append((l,X,Y))


plot_objs = []
## Here's the cool stuff, where we actually generate plots from data
for _args in plot_args:
    title = _args['title']
    keys = _args['keys']
    lines = []
    for d in alldata:
        for k in keys:
            _logname, _X, _Y = d[0], d[1], [_x[k] for _x in d[2]]
            _linename = "%s: %s" % (_logname, k)
            lines.append((_linename, _X, _Y))

    p = generatePlot(lines, title,
                        common_args['height'], common_args['width'])

    plot_objs.append(p)


## initialize data now
curdoc().add_root(column(*plot_objs))
