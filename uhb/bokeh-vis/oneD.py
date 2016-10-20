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
    for p in plots:
        ctr = 0
        rollover = max([len(_s.data['x']) for _s in p.sources])
        for l in p.logs:
            (X, _Y) = alldata[l]
            for k in p.keys:
                Y = [t[k] for t in _Y]
                p.sources[ctr].stream(dict(x=X, y=Y), rollover=rollover)
                ctr += 1


try:

    ### Initialize things and parse the URL arguments
    utils.init()
    common_args = utils.parseCommonArgs()

    logs = common_args['log']
    start = common_args['start']
    end = common_args['end']

    # populate the list of plots
    plots = utils.parsePlots(common_args)

    ### Now get data out of GDP (raw data, no filtering on keys yet)
    if common_args['end'] > 0.0:
        _end = end
    else:
        # We probably need to do live oscilloscope like plots
        _end = time.time()
        # Also set up things for the periodic callback
        last_update_time = _end
        curdoc().add_periodic_callback(updateData, 500)

    alldata = utils.getGDPdata(logs, start, _end)


    # Do we have at least one record? If not, probably tell the
    #   user that they did not chioose properly
    sampleRecord = None
    try:
        assert len(alldata.keys())>0   # We should have at least one log
        for l in alldata.keys():
            if len(alldata[l][1])>0:
                sampleRecord = alldata[l][1][0] # Get the Y value
        assert sampleRecord is not None
    except AssertionError as e:
        print alldata
        raise NoDataFound("No data found for the selected time range")


    ### If the user didn't specify a list of keys, we try to plot all the
    ###     keys. This is as general as it can be
    if len(plots)==0:
        # The user didn't tell us what keys to plot. We are going to plot
        #   everything that we can using some heuristics from data
        plottable_types = [int, float]
        for k in sampleRecord.keys():
            if type(sampleRecord[k]) in plottable_types:
                plots.append(utils.GDPPlot(logs, start, end, k, [k]))
    
    ### Here's the cool stuff, where we actually generate plots from data
    for p in plots:

        p.figure = figure(plot_width=common_args['width'],
                            plot_height=common_args['height'],
                            tools='', toolbar_location=None,
                            x_axis_type='datetime', title=p.title)

        p.sources = []
        _log_ctr = 0
        for l in p.logs:
            (X, _Y) = alldata[l]        # _Y is the list of raw JSON recs
            _key_ctr = 0
            for k in p.keys:
                Y = [t[k] for t in _Y]
                s = ColumnDataSource(dict(x=X, y=Y))
                if len(p.logs)==1 and len(p.keys)==1:
                    legend = None
                elif len(p.logs)==1 and len(p.keys)>1:
                    legend = k
                elif len(p.logs)>1 and len(p.keys)==1:
                    legend = str(_log_ctr)
                else:
                    legend = "%d: %s" %(_log_ctr, k)
                p.figure.line('x', 'y', source=s,
                                line_color=utils.colors[_log_ctr],
                                line_dash=utils.line_styles[_key_ctr],
                                legend=legend)
                p.figure.legend.location = "top_left"

                p.sources.append(s)
                _key_ctr += 1

            _log_ctr += 1

    
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
