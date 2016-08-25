% Writing GDP applications

# A typical application

The log interface acts as a proxy to interact with sensors and actuators. We
describe how one would go about designing a simple application. Imagine an
office building with some temperature sensors and some motor controlled window
blinds. We would like to design an application that looks at the current
temperature and some weather prediction feeds from the cloud to make a smart
decision about the window blinds. 

![An example GDP application](GDP_application_example.png)

As shown above, in order to do so, the application subscribes to all the
relevant logs and writes the actuation values to the actuator log. A gateway
working on behalf of the actuator subscribes to this actuation log and controls
the motors for the window blinds. Note that instead of two separate logs, a
composite log can be created for `Sensor 1` and `Sensor 2` in the diagram
above, provided that the gateway implements the single-writer semantics.

---

# Software Installation and Configuration

In this section, we talk about how to get access, install and use the GDP
client side library. As mentioned earlier, we have a C library for clients with
wrappers in other languages around this C-library.

As of now, we have packaged the GDP client-side code for Debian based systems
as `.deb` packages. For other platforms, unfortunately no one-click packages
exist. However, compilation from source works on Linux, BSD and Mac-OS.

For now, access to GDP is granted on a case-by-case basis (email Eric
<eric@cs.berkeley.edu> for access). The source code for GDP client library and
log-server is maintained in a git repository that can be accessed either over
HTTPS (requires username, password), or over SSH (requires key setup):

```
git clone https://repo.eecs.berkeley.edu/git/projects/swarmlab/gdp.git
git clone repoman@repo.eecs.berkeley.edu:projects/swarmlab/gdp.git
```

GDP-router is maintained in a separate repository. However, you should not need
access to that for just playing around with GDP.


## Compilation
Refer to README in the main git tree.

## Infrastructure information
Refer to README in the main git tree.

*Note that the software/infrastructure is still in very early experimental
phase. We do not make any guarantees on data retention/backups at this moment.
As the code stabilizes, we will make better effort to maintain data. In the
meantime, contact us if you would like to use GDP for anything serious.*


## Configuration
Refer to README in the main git tree.


## Creating logs

Until such time as we implement automatic placement, you have to tell the GDP
where a GCL should be created. By convention we are naming gdplogd instances
(the daemon that implements physical log storage) by the reversed domain name
followed by `.gdplogd`, for example, `edu.berkeley.eecs.gdp-01.gdplogd` is the
name of the gdplogd running on the machine `gdp-01.eecs.berkeley.edu`.

To create a new GCL you need to determine the node on which the log should be
created and the name of the new log and pass them to `apps/gcl-create`. For
example,

```
gcl-create -k none org.example.project.log17a
```

will create a log named `org.example.project.log17a` on one of the default
log-servers at Berkeley. Although you can create logs with any name, please
stick to this convention (with "project" being the project name or the user
name, as appropriate) so we can avoid name collisions. `-k none` means that
`gcl-create` will not attempt to create a new signature key for signing
appended data.  Although crucial to the operation, key-management is better
deferred to a stage when you are familiar with the basic operations of the GDP.
Also, note that `gcl-create` has several other command-line options that will
be useful later on.

---

# Writing applications in Python

Even though the GDP library is written in C, we provide a python package `gdp`
that acts as a wrapper around the C-library. This python package enables quick
prototyping using an object-oriented interface to GDP. What follows is a quick
how-to on writing simple GDP programs in Python. Note that this document is
just a starting point and is not intended to be a comprehensive guide to the
complete interface. For a more thorough API documentation, refer to
`/lang/python/README`.

## Appending data

Let's start with a simple `Hello world` program, that writes some data to a
pre-existing log and reads it back.

We need to import the package `gdp` to begin with. It should be installed in
your system path for python packages for this to work, however a simple
workaround is to create a symlink `~/.local/lib/python2.7/site-packages/gdp`
that points to `lang/python/gdp/` in your repository tree. 

```python
>>> import gdp
```

Once imported, we need to initialize the GDP package by calling `gdp_init()`.
An optional argument to `gdp_init()` is the address of a GDP-router. If no
address provided, a default value of the parameter `swarm.gdp.routers` is used
as configured by EP library. (See README for details).

```python
>>> gdp.gdp_init()
```

As mentioned earlier, we support human readable names for logs. The mechanism
for translating a human readable name to a 256-bit name is probably going to
change in the future, however, it is our hope that it should be a simple
change. The Python wrapper uses instances of class `gdp.GDP_NAME` for a name,
which could be initialized using a human readable name.

```python
>>> # Create a GDP_NAME object from a human readable python string
>>> gcl_name = gdp.GDP_NAME("edu.berkeley.eecs.mor.01")
```

Once we have a `GDP_NAME`, we can use this to open a handle to a log/GCL. A log
handle works like a file handle in some ways. We need to tell whether we want
to open the GCL in read-only mode (`gdp.GDP_MODE_RO`), or append-only mode
(`gdp.GDP_MODE_AO`), or read-and-append mode (`gdp.GDP_MODE_RA`).

```python
>>> # assume that this log already exists.
>>> gcl_handle = gdp.GDP_GCL(gcl_name, gdp.GDP_MODE_RA)
```

Next, let's append a few records to the log. The unit of read/write to a log is
called a record---data with some automatically generated metadata---represented
as a python dictionary. The special key `data` represents the data we wish to
append to the log, and its value should be a python (binary) string.

```python
>>> for idx in xrange(10):
...   datum = {"data": "Hello world " + str(idx)}
...   gcl_handle.append(datum)
```

That's it. Ideally, it should finish without throwing any errors, resulting in
10 records append to the log specified. 

Look at `/lang/python/apps/writer_test.py` for a full example.

## Reading data by record number

Next, let's read some data back and see if it matches what we wrote. Note that
we need to tell what record number we want to read, and record numbers start
from 1. To read data, we just use `read` method of the GDP_GCL instance with
the record number.

```python
>>> for idx in xrange(1,11):
...   datum = gcl_handle.read(idx)
...   print datum
{'recno': 1, 'data': 'Hello world 0', 'ts': {'tv_sec': 1442965633, 'tv_accuracy': 0.5, 'tv_nsec': 424722000L}}
{'recno': 2, 'data': 'Hello world 1', 'ts': {'tv_sec': 1442965633, 'tv_accuracy': 0.5, 'tv_nsec': 425629000L}}
{'recno': 3, 'data': 'Hello world 2', 'ts': {'tv_sec': 1442965633, 'tv_accuracy': 0.5, 'tv_nsec': 426345000L}}
{'recno': 4, 'data': 'Hello world 3', 'ts': {'tv_sec': 1442965633, 'tv_accuracy': 0.5, 'tv_nsec': 427150000L}}
{'recno': 5, 'data': 'Hello world 4', 'ts': {'tv_sec': 1442965633, 'tv_accuracy': 0.5, 'tv_nsec': 427989000L}}
{'recno': 6, 'data': 'Hello world 5', 'ts': {'tv_sec': 1442965633, 'tv_accuracy': 0.5, 'tv_nsec': 428745000L}}
{'recno': 7, 'data': 'Hello world 6', 'ts': {'tv_sec': 1442965633, 'tv_accuracy': 0.5, 'tv_nsec': 429484000L}}
{'recno': 8, 'data': 'Hello world 7', 'ts': {'tv_sec': 1442965633, 'tv_accuracy': 0.5, 'tv_nsec': 430200000L}}
{'recno': 9, 'data': 'Hello world 8', 'ts': {'tv_sec': 1442965633, 'tv_accuracy': 0.5, 'tv_nsec': 431135000L}}
{'recno': 10, 'data': 'Hello world 9', 'ts': {'tv_sec': 1442965633, 'tv_accuracy': 0.5, 'tv_nsec': 431962000L}}
```

So far, we saw how to read and write data by record number. However, most of
the times, we are interested in the most recent record. For this, we support
negative record numbers, i.e. `-1` refers to the most recent record, `-2`
refers to the second most recent record, and so on.

Look at `/lang/python/apps/reader_test.py` for a full example.

## Subscriptions

Next, let's see how can we subscribe to a log to get new data from a log as it
gets appended. For this, we use `subscribe` method of the `gdp.GDP_GCL`
instance.

```python
>>> # ignore the parameters for the moment
>>> gcl_handle.subscribe(0, 0, None)
```

This subscription returns events, that we need to process in order to get
notified of the data as it appears.

```python
>>> while True:
...   # this blocks until there is a new event
...   event = gcl_handle.get_next_event(None)
...   # Event is a dictionary itself.
...   if event["type"] == gdp.GDP_EVENT_DATA:
...     datum = event["datum"]
...     print datum
...   else: 
...     # we ignore other event types for simplicity
...     break
```

In the above code, `event` itself is a dictionary that has `datum` as the key
containing the latest data. In order to see the above code in action, open
another python console (while this is running), and append some new data to the
log just the way you saw above.

Look at `/lang/python/apps/reader_test_subscribe.py` for a full example.

## Multiread

Reading one record at a time can be very inefficient, especially when reading
large amount of data. For this, we support `multiread` to read a range of
records at a time. The interface is similar to `subscribe` in some
sense---events are returned as a result of a `multiread` call. 

Look at `/lang/python/apps/reader_test_multiread.py` for a full example.


## Asynchronous write

*Partially implemented*. In the normal `append` call above, a client sends some
data to the log-server and waits for an acknowledgement before returning
control back to the application. In order to convert this blocking operation to
a non-blocking operation, `append_async` could be used instead of regular
`append`.

Refer to the API documentation at `/lang/python/README` for more details.
