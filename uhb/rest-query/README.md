# A REST-ful endpoint for querying data out of a specific GDP log

**Do not use this in production without understanding the consequences.**

Certain applications that consume data, for whatever reason, can not
incorporate a full GDP client. An example category are web-browser based
applications which have only limited functionality.

In order to cater for such specific use cases, here is a GDP=>REST gateway. One
might be tempted to run a public facing web-server using the provided code,
that anyone can access and read logs. However, this is *not* the intended
model. The responsibility of any access control, data security, and so on is
the responsibility of whoever runs this RESTful gateway.

With this model in mind, the RESTful server here intentionally is limited to
read from only one single log, which must be specified at the startup time. In
addition, there is no subscripition support; polling is a poor man's
subscription.

## Requirements

This requires that you have a Python GDP client side bindings already setup. In
addition, it uses Twisted, which is an event driven framework for python. On an
Ubuntu based system, it comes bundled as the package `python-twisted`, so your
usual `apt-get` commands should work.

## Data format and interaction

You can use `GET` to request data from the log specified at startup time. You
have to provide a `start` and `end`, both of which are expressed as seconds
since epoch.

The resulting data is returned as a list of JSON objects, with each object
containing `recno`, `ts` and `data`. `recno` specifies the record number,
`ts` is a commit timestamp as recorded by the server (in seconds since epoch),
and `data` is the raw data as stored in the record. However, note that
encoding data to JSON is highly fragile. If your log contains arbitrary
binary data, it will most certainly break things here and there.

## Execution

In order to see the various options/flags, use

```
python gdp-rest.py -h
```

### Example:

```
python gdp-rest.py edu.berkeley.eecs.bwrc.device.c098e5300054
```

You can go to a web-browser and fetch the following URL:

```
http://localhost:8080/?start=1477000000&end=1477000010
```
