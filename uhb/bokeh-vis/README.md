# Visualization for GDP data

A visualization *tool* based on bokeh. In order to minimize repetition of
effort, this tool attempts to provide a generic visualization mechanism for
data in GDP logs. Note that this is not intended to be a perfect tool that fits
every possible use-case. There will always be cases when a more specialized
tool will be a better fit.

The high level idea is that this tool can be *hosted* somewhere and provides a
web-server that returns (dynamic) plots as a web-page, which can then be
embedded in other web-applications. Parameters for what log should be plotted,
time range, etc. are passed as part of the URL.

There is a basic assumption that the log(s) contains records which are simple
JSON records serialized as strings. Simple here means that it is a simple (key,
value) store; only the keys with integer/float values are used for plottings,
everything else is ignored. For the moment, and nested JSON objects or arrays
are not supported.

An example record is:

```
{"device" : "BLEES", "pressure_pascals" : 100327.9, "humidity_percent" : 41.18,
"temperature_celcius" : 22.56, "light_lux" : 20, "acceleration_advertisement" :
false, "acceleration_interval" : true, "sequence_number" : -1, "id" :
"c098e5300003", "_meta" : {"received_time" : "2016-10-20T04:27:44.672Z",
"device_id" : "c098e5300003", "receiver" : "ble-gateway", "gateway_id" :
"6c:ec:eb:ac:00:02"}}
```

In such a record, this visualization tool can plot the following keys:
`pressure_pascals`, `humidity_percent`, `temperature_celcius`, `light_lux`,
`sequence_number`.

An example plot for temperature data can be obtained by performing a query such
as (see more details below):

```
http://hostname/path?log_0=logname&start=1477000000&end=1477000600\
        &plot_0_title=Temperature&plot_0_keys=temperature_celcius
```

# Types of plots

As of now, this plotting tool supports the following

* One dimensional time-series data, where one or more keys can be plotted
  against the commit timestamp associated with the record.
* Two dimensional trajectory style plots, where each record specifies an `x`
  and `y` coordinate. Each point is connected to the previous point, thus
  mapping out a trajectory.
* Two dimensional heat-map style plots, where each record specifies an `x`
  and `y` coordinate, and optionally a value `v`.

For each of the plots, multiple logs can be specified to be plotted alongside
each other. Both static plots (from old data) and continuously updating plots
(like an oscilloscope) are supported for each category.

## Parameters

There are certain global parameters that are common across each plot type. In
addition, each type of plot has parameters that are specific to that type of
plot. All parameters are passed as URL arguments, such as:
`.../?param1=val1&param2=val2&param2=val3`

Note that `param2` is passed multiple times, which is okay. Within the context
of this tool, the above parameters are interpreted as:

```
param1 = val1
param2 = [val2, val3]
```


### Global parameters

* Names of logs that serve as the data source. Specified as `log_0`, `log_1`,
  and so on. (Required)
* Start time, expressed as seconds since epoch. Specified as `start`.
* End time, expressed as seconds since epoch. A special value of `0` signifies
  that the plot be continuously updated as new data arrives. In case of such
  continuously updating plots, the plot rollsover and overwrites the old
  data, thus making the number of points roughly constant. Specified as `end`.
* Height of indvidual plot, specified as `height`.
* Width of indvidual plot, specified as `width`.

### Plot specific parameters

Each request can render multiple plots, each of different type. Plots are
numbered from `0`; parameters for `i`-th plot are specified with a prefix
`plot_i_`. If no `plot_` parameters are specified, then no plots will be shown.

If there are more than one logs specified as global parameters, each plot will
contain data sourced from all the logs. Thus, if one wishes to plot temperature
data as a function of time (see the example record above), and specifies `3`
logs, the resulting plot will have `3` lines, each corresponding to one of the
logs. Same holds true for two-dimensional plots as well.

In order to choose the type of plot, one has to specify the plot type by
setting the parameter `plot_i_type`. Valid values are `oneD`, `twoD-traj`,
`twoD-heat`. If ommitted, a default value of `oneD` is assumed.

#### One dimensional time series data

For one dimensional time series data, each plot has the following required
parameters: `plot_i_title`, `plot_i_keys`. Other optional parameters are
`plot_i_height` and `plot_i_width`.

An example is an environmental sensor that records both humidity and
tempearture at the same time (see example record above), which ought to be
plotted on two separate plots.  Such result can be achieved by the setting the
following parameters (note that `plot_i_type` is optional for one dimensional
time series plots):

```
plot_0_type = oneD
plot_0_title = Temperature
plot_0_keys = temperature_celcius
plot_0_height = 200
plot_0_width = 800
plot_1_title = Humidity
plot_1_keys = humidity_percent
plot_1_height = 200
plot_1_width = 800
```

In addition, mulitple keys from the same record can be plotted on the same
plot. An example is acceleration represented along three separate axes, but
plotted together.

```
plot_0_type = oneD
plot_0_title = Acceleration
plot_0_keys = accel_x
plot_0_keys = accel_y
plot_0_keys = accel_z
```

This above example will result in a single plot with three different lines
(assuming only a single log provided as global parameter). In case, say `2`
logs were provided as global parameters, this will result into `6` lines on
the same plot.


#### Two dimensional plots

For two dimensional plots (both trajectory style or heatmap style), plot
specific parameters are somewhat similar to one dimensional plots, except that
plotting multiple keys from the same log on the same plot does not make too
much sense (at the moment).

The required parameters for two dimensional plots are (`i` starts from `0`):

* `plot_i_type`: The type of the plot (either `twoD-traj` or `twoD-heat`)
* `plot_i_title`: Title of the plot
* `plot_i_key_x`: The key in JSON data that contains the X coordinate
* `plot_i_x_min`: (Optional) Minimum value for X.
* `plot_i_x_max`: (Optional) Maximum value for X.
* `plot_i_key_y`: The key in JSON data that contains the Y coordinate
* `plot_i_y_min`: (Optional) Minimum value for Y.
* `plot_i_y_max`: (Optional) Maximum value for Y.
* `plot_i_key_val`: (meaningful only for heatmap style plot) An *optional*
  value that corresponds to the value at the specific `(x,y)` location. If
  provided, all `(x,y)` points are assigned a weight proportional to this
  value.
* `plot_i_val_min`: (Optional) Minimum possible value.
* `plot_i_val_max`: (Optional) Maximum possible value.

As an example, the following plots a trajectory assumed by a particle that
periodically posts its location.

```
plot_0_type = twoD-traj
plot_0_title = Location
plot_0_key_x = x
plot_0_x_min = 0.0
plot_0_x_max = 100.0
plot_0_key_y = y
plot_0_y_min = 0.0
plot_0_y_max = 100.0
```

The following plots a heat map of signal strength.

```
plot_0_type = twoD-heat
plot_0_title = Signal
plot_0_key_x = x
plot_0_x_min = 0.0
plot_0_x_max = 100.0
plot_0_key_y = y
plot_0_y_min = 0.0
plot_0_y_max = 100.0
plot_0_key_val = val
plot_0_val_min = 0.0
plot_0_val_max = 100.0
```

## A note about legends

At this point, a naive legend scheme is used. A legend is automatically
generated if required. No control is provided to the user.


## How to run

Install bokeh using

    pip install bokeh

Also, ensure that you have GDP client-side installation done appropriately
(including Python bindings). Next, run a bokeh server using (from
`../bokeh-vis/.`)

    python -m bokeh serve main.py

This starts a bokeh server at port 5006, only available to local clients.
You can start a public facing server by passing in additional parameter
`--host HOST[:PORT]`.

Open the file `template/index.html` in a web-browser on a little HowTo for
using the plotting tool.

