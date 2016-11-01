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
plots = []  # A list of utils.GDPPlot objects



def updateData():
    # this function gets called every once in a while.
    # > Fetches new data out of GDP and calls updateData on individual plots
    global last_update_time
    current_time = time.time()
    try:
        newdata = utils.getGDPdata(logs, last_update_time, current_time)
    except (EP_STAT_Exception, utils.NoDataException) as e:
        return

    last_update_time = current_time

    # update the sources for the data
    for p in plots:
        p.updateData(newdata)

try:

    ### Initialize things and parse the URL arguments
    utils.init()
    common_args, plot_args = utils.parseArgs()

    if len(plot_args)==0:
        raise utils.NoPlotsSpecifiedException

    logs = common_args['log']
    start = common_args['start']
    end = common_args['end']

    # populate the list of plots
    plots = utils.initPlots(plot_args, common_args)

    ### Now get data out of GDP (raw data, no filtering on keys yet)
    if common_args['end'] > 0.0:
        _end = end
    else:
        # We probably need to do live oscilloscope like plots
        _end = time.time()
        # Also set up things for the periodic callback
        last_update_time = _end
        curdoc().add_periodic_callback(updateData, 1000)

    alldata = utils.getGDPdata(logs, start, _end)


    ### Here's the cool stuff, where we actually generate plots from data
    for p in plots:

        # initialize the high level figure
        p.initFigure()

        # initialize indvidual lines with dat
        p.initData(alldata)


    ### Put the generated plots into a document now, bokeh will take care
    ### of plotting them for us.
    _figures = [p.figure for p in plots]
    curdoc().add_root(column(*_figures))

except Exception as e:
    ## Any exception encountered above will lead us here, and we will
    ##  tell the user to fix their stuff. It's better than silently ignoring
    ##  any errors and generating nothing.
    error_string = "Exception: %s\n\n" % str(e)
    error_string += traceback.format_exc()
    p = PreText(text=str(error_string), width=800)
    curdoc().add_root(p)
