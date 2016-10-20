#!/usr/bin/env python

"""
A generic plotting tool for time-series data. It use a number of logs
    containing JSON data as underlying data-sources. Using such data,
    it can plot multiple plots (for separate keys, or collection of keys).

To execute, run:
    $ python -m bokeh serve oneD.py

The general parameter passing scheme is as follows:


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

import utils
from gdp.MISC import EP_STAT_Exception
import time
import sys
import traceback
from bokeh.io import curdoc
from bokeh.models.sources import ColumnDataSource
from bokeh.layouts import row, column
from bokeh.plotting import figure
from bokeh.models.widgets import PreText


last_update_time = 0.0
logs = []
plot_sources = []
plot_args = []

class NoDataFound(Exception):
    def __init__(self, arg):
        self.msg = arg


def updateData():
    # this function gets called every once in a while.
    # > Fetches new data out of GDP and updates the source for plots
    global last_update_time
    current_time = time.time()
    try:
        alldata = utils.getGDPdata(logs, last_update_time, current_time)
    except EP_STAT_Exception as e:
        return
    last_update_time = current_time

    # update the sources for the data
    ctr = 0
    for _args in plot_args:
        keys = _args['keys']
        for d in alldata:
            for k in keys:
                _logname, _X, _Y = d[0], d[1], [_x[k] for _x in d[2]]
                _source = plot_sources[ctr]
                ctr += 1
                rollover = len(_source.data['x'])
                _source.stream(dict(x=_X, y=_Y), rollover=rollover)
 

try:

    ### Initialize things and parse the URL arguments
    utils.init()
    common_args = utils.parseCommonArgs()
    logs = common_args['log']
    plot_args = utils.parsePlots()

    ### Now get data out of GDP (raw data, no filtering on keys yet)
    if common_args['end'] > 0.0:
        alldata = utils.getGDPdata(logs, common_args['start'],
                                        common_args['end'])
    else:
        # We probably need to do live oscilloscope like plots
        current_time = time.time()
        alldata = utils.getGDPdata(logs, common_args['start'], current_time)

        # Also set up things for the periodic callback
        last_update_time = current_time
        curdoc().add_periodic_callback(updateData, 500)



    # Do we have at least one record? If not, probably tell the
    #   user that they did not choose properly
    try:
        assert len(alldata)>0   # We should have at least one log
        assert len(alldata[0][2])>0 # We should have at least one data point
    except AssertionError as e:
        print alldata
        raise NoDataFound("No data found for the selected time range")


    ### If the user didn't specify a list of keys, we try to plot all the
    ###     keys. This is as general as it can be
    if len(plot_args)==0:
        # The user didn't tell us what keys to plot. We are going to plot
        #   everything that we can using some heuristics from data
        sampleRecord = alldata[0][2][0] # there'll be at least one record
        plottable_types = [int, float]
        for k in sampleRecord.keys():
            if type(sampleRecord[k]) in plottable_types:
                plot_args.append( {'title': k, 'keys': [k] })
        print "Updated parameters for individual plots", plot_args
    
    ### Here's the cool stuff, where we actually generate plots from data
    plot_objs = []
    for _args in plot_args:
        title = _args['title']
        keys = _args['keys']
        lines = []
        for d in alldata:
            for k in keys:
                _logname, _X, _Y = d[0], d[1], [_x[k] for _x in d[2]]
                _legend = "%s: %s" % (_logname, k)
                _source = ColumnDataSource(dict(x=_X, y=_Y))
                plot_sources.append(_source)
                lines.append((_legend, _source))
    
        p = utils.generatePlot(lines, title,
                            common_args['height'], common_args['width'])
    
        plot_objs.append(p)
    
    
    ### Put the generated plots into a document now, bokeh will take care
    ### of plotting them for us.
    curdoc().add_root(column(*plot_objs))

except Exception as e:
    ## Any exception encountered above will lead us here, and we will
    ##  tell the user to fix their stuff. It's better than silently ignoring
    ##  any errors and generating nothing.
    error_string = "Exception: %s\n\n" % str(e)
    error_string += traceback.format_exc()
    p = PreText(text=str(error_string), width=800)
    curdoc().add_root(p)
