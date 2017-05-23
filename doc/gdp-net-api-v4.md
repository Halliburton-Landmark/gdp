% What I Want to See in a Network API
  for the GDP
% Eric Allman
% 2017-05-20

***This is a proposal, not a specification***


SEE ALSO: Nitesh's documents.

[[NOTE: "PDU", "blob", and "payload" are used somewhat interchangeably,
with a bit of "message" thrown in for good measure.  This is a bug,
not a feature.]]


## Key Points

This is essentially a message-based L4/L5 (Transport/Session
Layer) model.  The L5 part is about advertisements and routing.
Normally routing would be L3 (Network Layer), but our model
seems to be somewhat inverted in that the routing commands
live on top of the reliable transmission layer.  This is part
of the legacy of the "overlay network" model.

For the purposes of this document, "I", "me", and "my" refer
to the user of this API.  "You" and "your" refer to the
implementer of the API.  "Payload" is an opaque blob for you;
the term is approximately equal to "PDU".

> Using OSI language, a "payload" would be called a "Service
Data Unit" or SDU.  You add a header to that, and then it
becomes the PDU for the next layer down.  This document does
not discuss the internal structure of the payload.

GDP clients and servers ("my side" of the API) see:

* A message-based interface (i.e., PDUs are delimited by the
  network layer).
* Reliable data transmission:
    + Fragmentation, flow control, retransmission, etc. already
      handled.
    + Individual PDUs delivered reliably and in order, that is,
      fragments of PDUs will not be delivered out of order,
      fragments will not be duplicated, and an error will be
      delivered if a fragment is lost.
* Different PDUs may be delivered out of order.
* PDU sizes are not inherently limited by underlying MTUs.

The network ("your side" of the API) sees (that is, I will give
you):

* Source and destination GDPnames (256-bit).
* A dynamically sized buffer in which to read or store a
  payload, which must be treated as an opaque blob.
* Advertisements of known GDPnames, authenticated using a
  certificate-based scheme, still not fully defined.
* Probably some hints vis-a-vis client expectations such as
  Quality of Service.  These remain _for further study_.
* _Other information to be determined._
* Functions to declare I/O event handlers.

Your side is responsible for:

* Routing.
* Reliability (retransmissions, etc.).
* Fragmentation/Reassembly.
* In order delivery (i.e., all pieces of the PDU delivered in order).
* Compression (notably header compression).
* On-the-wire crypto (TLS or DTLS as appropriate).
* DoS mitigation.  Attack traffic should be stopped as soon as
  possible.
* Invoking callbacks when I/O events occur.

"Your side" of the API can live partially in the client library,
but some amount of it might live elsewhere, either in the
switch/forwarder/router layer or in a separate service.  I assume
that this layer will not rely on threads in client processes to
make it possible to run in low-end (non-MMU) processors.
_Note: this is a change from the V3 design, which assumes a
dedicated thread for I/O.  This is intended to support Kubi's
dream of the GDP on an Arduino-class platform._

Issues that we should consider:

* All internal structure of messages (including signatures, etc)
  are hidden from the forwarding/routing layer.  Is there anything
  else (other than source and destination identifiers) that needs
  to be exposed to the lower level (e.g., QoS requests)?
    + Nitesh says: ability to address individual replicas.
    + Nitesh says: ability to send to all replicas.
    + Eric says: anything needed for DoS mitigation?  Maybe
      an HMAC key?

* This interface allows streaming receipt of data (within a single
  record) but not streaming transmission.  How serious is this?
  _[[Note to self: use bufferevents?  This might dramatically
  change the interface, but ties us even more closely to libevent.]]_
  Note: it's impossible to write a header that includes the payload
  length until the entire payload has been read.

* This should be easy to implement (from the network perspective)
  for the existing TCP-based, no fragmentation model, but is it
  reasonably sane for other I/O models?

* Some way of doing multicast, notably for subscriptions.

Design/Implementation Notes:

* This interface uses `libevent` (<http://libevent.org>).
  There is really no way around my picking the library without
  inverting the control flow, which leaves us with threads.
  However, the network internals are presented with a new
  interface ("gdp_ioevent") that masks the details and gives
  us an opportunity to swap out the implementation in the
  future.

Documentation Notes:

* This version is loosey-goosey with status codes.  If we decide
  that this is a good direction I will provide more detail.


## API

The API is intended to map easily into an object-oriented paradigm
with the first parameter to instance methods being `self`.  Class
methods generally return a status code; if they are allocating a
new object it will be returned through the last parameter.

The parenthetical comments in the titles are intended to provide
a model of how this would map into an OO environment.


### Data Types

* `gdp_chan_t` contains the state of the channel itself.  It is
  opaque to "my side" of the API.
* `gdp_cursor_t` is opaque to "my side" of the API; it provides a
  streaming interface to a payload while the client is receiving.
  Internally ("your side") it is fair game for other use, in
  particular, it may be useful on the sending side.
* `gdp_buf_t` implements dynamically allocated, variable sized
  buffers (already exists; based on `libevent`).  Details of that
  interface are not included here.
* `gdp_ioevent_t` encodes callbacks for particular I/O events.
  It is opaque to "your side" of the API.
* `gdp_name_t` is the 256-bit version of a GDP name.
* `gdp_adcert_t` is whatever information is needed to advertise
  a GDPname.  This is _for further study_.


### Initialization


#### gdp_chan_init (class method)

I promise to call this routine on startup:

~~~
	EP_STAT gdp_chan_init(
		void *unused);
~~~

The `unused` parameter is there for future use.  Until then, I'll
always pass it as `NULL`.


#### gdp_ioevent_add (constructor)

~~~
	typedef void gdp_ioevent_cb_t(
		int socket_fd,
		uint32_t what,
		void *cb_arg);

	EP_STAT gdp_ioevent_add(
		int socket_fd,
		uint32_t when,
		EP_TIME *timeout,
		gdp_ioevent_cb_t *cb_func,
		void *cb_arg,
		gdp_ioevent_t **ioevent);
~~~

`gdp_ioevent_add` adds a new event to the event loop (a.k.a.
"reactor" in Twisted-speak) and returns it indirectly through
`ioevent`.  You only need to save it if you want to be able to
delete the ioevent in the future.  When something indicated by `when`
happens on `socket_fd`, the indicated `cbfunc` will be called.
The `cb_arg` value is passed through to the callback unchanged.
If a `timeout` is specified, `cbfunc` will also be invoked if
there is no activity in the given time interval.  The `when`
parameter says what activity should trigger the callback, and
the `what` callback parameter tells what event(s) actually
occurred.  Both paramters are based on the set:

| Value			| Meaning	|
|-----------------------|---------------|
| `GDP_IOEVENT_READ`	| Input is available for reading	|
| `GDP_IOEVENT_WRITE`	| The indicated socket can be written	|
| `GDP_IOEVENT_TIMEOUT`	| There was a timeout on this event	|
| `GDP_IOEVENT_CLOSE`	| The socket was closed by the other end	|
| `GDP_IOEVENT_ERROR`	| There was an I/O error on the socket	|

These are bitmaps, and it is possible for multiple events to
trigger a single callback.


#### gdp_ioevent_del (ioevent::del)

~~~
	EP_STAT	gdp_ioevent_del(
		gdp_ioevent_t *ioevent)
~~~

Delete the indicated `ioevent`.  If the indicated event was set
with a `cb_arg`, it is the responsibility of the caller to free
any memory associated with that argument.



### Channel Management

A Channel is a somewhat arbitrary class used to store whatever the
network ("your") side needs.  I promise to create at least one
channel when I start up.

#### gdp_chan_open (constructor)

~~~
	EP_STAT gdp_chan_open(
		const char *addrspec,
		void *unused,
		void *udata,
		gdp_chan_t **chan);
~~~

Creates a channel to a GDP switch located at `addrspec` and stores
the result in `*chan`.  If `addrspec` is NULL, the `swarm.gdp.routers`
runtime parameter is used.  The `unused` parameter is intended to
hold specifications (e.g., QoS requirements), but for now I promise to
pass it as `NULL`.  The `udata` parameter is saved and is available
to any callbacks on this channel.

_[[Note: it isn't clear we need `gdp_chan_init`; it could just be
done on the first call to `gdp_chan_open` --- unless we need to use
the `unused` parameter during `gdp_chan_init`.]]_

#### gdp_chan_close (destructor)

~~~
	EP_STAT gdp_chan_close(
		gdp_chan_t *chan);
~~~

Deallocate `chan`.  All resources are freed.  I promise I will not
attempt to use `chan` after it is freed.

#### gdp_chan_set_callbacks (chan::set_callbacks)

_[[Need callbacks for received data, failure on a connection.
Anything else?]]_

~~~
	EP_STAT gdp_chan_set_callbacks(
		gdp_chan_t *chan,
		EP_STAT *gdp_recv_cb(
			gdp_chan_t *chan,
			gdp_cursor_t *cursor,
			gdp_name_t *src,
			gdp_name_t *dst,
			size_t payload_len),
		EP_STAT *gdp_send_cb(
			gdp_chan_t *chan,
			gdp_buf_t *payload),
		EP_STAT (*gdp_close_cb)(
			gdp_chan_t *chan,
			int what));
~~~

When a new PDU is ready to read on `chan`, call `gdp_recv_cb`,
including the total size of the payload (not necessarily what is
available for read right now).  The `cursor` is a handle created
by the network layer that I must pass into subsequent functions
during the lifetime of this callback in order to actually read the
data, and that I promise to never use outside this callback.  Data
is actually read using `gdp_cursor_recv` (see below).

The `gdp_send_cb` is not used at this time; for now, if it is non-NULL
`gdp_chan_set_callbacks` should return `GDP_STAT_NOT_IMPLEMENTED`.
The parameters to this function are subject to change.

When a channel is closed by the other end of the connection, or on
I/O error, `gdp_close_cb` is called.  the `what` parameter is from
the same set of values, i.e., `GDP_IOEVENT_CLOSE` or `GDP_IOEVENT_ERROR`.

_[[Nitesh brings up the question of `dst` filtering.  It isn't clear
which side of the interface this belongs on.]]_


#### gdp_chan_get_udata(

~~~
	void *gdp_chan_get_udata(
		gdp_chan_t *chan);
~~~

Returns the `udata` associated with `chan`.


### Advertising and Certificates

_[[Who is responsible for certificate management and advertisements?
I guess that's likely to be me.  Drat.]]_

#### gdp_chan_advertise (chan::advertise)

_This interface is still under development._

~~~
	// challenge/response callback function type
	typedef EP_STAT (*gdp_cr_func_t)(
		gdp_chan_t *chan,
		int action,
		void *ndata,
		void *udata);

	// advertisement method
	EP_STAT gdp_chan_advertise(
		gdp_chan_t *chan,
		gdp_name_t gname,
		gdp_adcert_t *adcert,
		gdp_cr_func_t *challenge_cb,
		void *udata);
~~~

Advertises the name `gname` on the given `chan`.  If a certificate
needs to be presented, it should be passed as `adcert`.  If the
underlying layer needs further interaction (e.g., for challenge/response)
it should call `challenge_cb`.

The callback function is passed the `chan`, an `action` **to be
determined**, and any network data needed in order to continue as
`ndata`.  The `udata` field is passed directly from `gdp_chan_advertise`.

_[[What is `adcert` exactly?  Where does it come from?]]_

#### gdp_chan_withdraw (chan::withdraw)

~~~
	EP_STAT gdp_chan_withdraw(
		gdp_chan_t *chan,
		gdp_name_t gname);
~~~

Withdraw a previous advertisement, for example, if a log is removed
from a given server.

_[[Question: should `gdp_chan_advertisement` return a data structure
that can be passed to `gdp_chan_withdraw`?]]_


### Sending and Receiving Messages

#### gdp_chan_send (chan::send)

~~~
	EP_STAT gdp_chan_send(
		gdp_chan_t *chan,
		gdp_XXX_t target,
		gdp_name_t src,
		gdp_name_t dst,
		gdp_buf_t *payload);
~~~

Sends the entire contents of `payload` to the indicated `dst` over
`chan`.  The source address is specified by `src`.

The `target` give clues as to exactly where to deliver the
message.  For example, it might be any replica of a given log,
all replicas of a given log (e.g., for quorum read), or a specific
replica.  How it is specified is _for further study_.

_[[Issue: There are issues regarding allowing an arbitrary `src` that
need to be explored.  You should never be permitted to send from
an address you aren't authorized to speak for, but the ultimate
responsibility for avoiding problems falls to the receiver.]]_

_[[Implementation Note: in the short run this may return
`GDP_STAT_PDU_TOO_LONG` if the size of `payload` exceeds an
implementation-defined limit.  This should be as large as possible
since it limits the size of any single record stored in the GDP.]]_


### gdp_chan_multicast (chan::multicast)

_[[Note: this is a placeholder.]]_

~~~
	EP_STAT gdp_chan_multicast(
		gdp_chan_t *chan,
		gdp_name_t src,
		gdp_XXX_t multicast_addr,
		gdp_buf_t *payload);
~~~

Sends `payload` to multiple destinations as indicated by
`multicast_addr`.  It isn't clear what that is.

This is primarily intended for delivering subscription data.

This will certainly need support to set up a multicast channel,
probably with "new", "join", "leave", and "free" style interface.


#### gdp_cursor_recv (cursor::recv)

This can only be called within a receive callback.  It acts on a
cursor rather than on the channel itself.  It is not necessary for
the entire payload to be read for this call to return data, nor is
it necessary to read the entire payload at once.

~~~
	EP_STAT gdp_cursor_recv(
		gdp_cursor_t *cursor,
		gdp_buf_t *payload,
		size_t *payload_len,
		uint32_t flags);
~~~

Read up to `*payload_len` octets from `cursor` into `payload`.  If
`*payload_len` = 0, block until all octets comprising the current
payload have been read.  Returns the number of octets actually read
into `*payload_len`.  If `payload` already has data, the new data is
appended.

Data read from a cursor is guaranteed to be presented in the
same order it was written with no duplicates or dropouts.
There is no such "in order" guarantee between different payloads.

* If all data for the entire PDU has been read, returns `EP_STAT_OK`.
* If data remains to be read, returns `GDP_STAT_KEEP_READING`
  (this is an information status, not an error).
* If the PDU is complete, but there is no additional data available
  at this moment, returns `GDP_STAT_READ_LATER`.  (this is an
  information status, not an error).
* If a fragment of payload cannot be read, returns
  `GDP_STAT_PDU_READ_FAIL` and any remainder of the payload is discarded.

_[[Include a timeout, or is it sufficient to say that if
`*len` ≠ 0 it will return immediately? And the semantics imply
that it wcan return at any time once it has at least one
octet.]]_

_[[Should it wait until at least one octet is read?
This should never happen, since it can only be called during a
receive callback, which asserts that at least some data is
available.]]_

_[[If I try to read a portion of the PDU that is not yet available
should it block or return a different status code (which would
mean that the callback would be invoked again when more data
became available)?  If it fails there should be some way to say
"no, I really want to block."]]_

Flags:

| Flag Name		| Meaning				|
|-----------------------|---------------------------------------|
| `GDP_CURSOR_BLOCK`	| Block until all PDU data is read	|


### Utilities

#### gdp_cursor_get_endpoints (cursor::get_endpoints)

~~~
	EP_STAT gdp_cursor_get_endpoints(
		gdp_cursor_t *cursor,
		gdp_name_t *src,
		gdp_name_t *dst);
~~~

Returns the endpoints of the given `cursor` into `src` and `dst`.
These will be the same as passed to `gdp_recv_cb`.

It isn't clear this function is needed.


### Buffers

These routines already exist.  This list is not exhaustive.

#### gdp_buf_read

~~~
	size_t buf_read(
		gdp_buf_t *buf,
		void *out,
		size_t size);
~~~

#### gdp_buf_peek

~~~
	size_t buf_peek(
		gdp_buf_t *buf,
		void *out,
		size_t size);
~~~

#### gdp_buf_write

~~~
	EP_STAT buf_peek(
		gdp_buf_t *buf,
		void *out,
		size_t size);
~~~

#### gdp_buf_reset

~~~
	EP_STAT gdp_buf_reset(
		gdp_buf_t *buf);
~~~

#### gdp_buf_getlength

~~~
	size_t gdp_buf_getlength(
		const gdp_buf_t *buf);
~~~


## Status Codes

Later.
