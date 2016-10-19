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
import sys
import traceback
from bokeh.io import curdoc
from bokeh.layouts import row, column
from bokeh.plotting import figure
from bokeh.models.widgets import PreText


## Initialize stuff
try:
    utils.init()
    # first parse the arguments
    common_args = utils.parseCommonArgs()
    plot_args = utils.parsePlots()

    alldata = utils.getGDPdata(common_args)

    # Do we have at least one record? If not, probably tell the
    #   user that they did not choose properly
    assert len(alldata)>0   # We should have at least one log
    assert len(alldata[0][2])>0 # We should have at least one data point

 
    if len(plot_args)==0:
        # The user didn't tell us what keys to plot. We are going to plot
        #   everything that we can using some heuristics from data
        sampleRecord = alldata[0][2][0] # FIXME: this is going to break
        plottable_types = [int, float]
        for k in sampleRecord.keys():
            if type(sampleRecord[k]) in plottable_types:
                plot_args.append( {'title': k, 'keys': [k] })
    
        print "Updated parameters for individual plots", plot_args
    
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
    
        p = utils.generatePlot(lines, title,
                            common_args['height'], common_args['width'])
    
        plot_objs.append(p)
    
    
    ## initialize data now
    curdoc().add_root(column(*plot_objs))

    ## Add the exceptions and other junk
    # p = PreText(text=str(utils.error_text))
    # curdoc().add_root(p)

except Exception as e:
    ## Any other exception encountered above will be passed to our
    ##  custom exception hook, which will lend us here.
    error_string = "Exception: %s\n\n" % str(e)
    error_string += traceback.format_exc()
    p = PreText(text=str(error_string), width=800)
    curdoc().add_root(p)
