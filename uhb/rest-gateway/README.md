# A REST-ful endpoint for publishing

Certain applications that generate data, for whatever reason, can not
incorporate a full GDP client. An example category are web-browser based
applications which have only limited functionality.

In order to cater for such specific use cases, here is a REST=>GDP gateway. One
might be tempted to run a public facing web-server using the provided code,
that anyone can access and write to specific logs. However, this is *not* the
intended model. The intended model of execution for this specific code is
similar to a bluetooth wearable device, which must connect to a more capable
gatway device that speaks TCP/IP (and hopefully GDP). Another example is an
MQTT=>GDP bridge (see `../`). The responsibility of any access control, data
security, and so on is the responsibility of whoever runs this RESTful gateway.

With this model in mind, the RESTful server here intentionally is limited to
write to only one single log, which must be specified at the startup time. In
this case, the RESTful server is *the* single writer for the specified log. It
can, however, receive requests from multiple HTTP clients; all those clients
put their trust in the RESTful server to do the right thing.

## Requirements

This requires that you have a Python GDP client side bindings already setup. In
addition, it uses Twisted, which is an event driven framework for python. On an
Ubuntu based system, it comes bundled as the package `python-twisted`, so your
usual `apt-get` commands should work.

## Data format and interaction

You can either use `GET` or `POST` to send data. Any data sent is interepreted
as `key, value` pairs, converted to a JSON object, and appended to the log
specified at initialization time. By default, a single key can have multiple
values, hence values are always a list. However, this behavior can be modified
by passing appropriate flags at startup time.

## Execution

In order to see the various options/flags, use

```
python gdp-rest.py -h
```

### Example:

```
python gdp-rest.py --nolist -k key.pem edu.berkeley.eecs.mor.Oct22.00
```
