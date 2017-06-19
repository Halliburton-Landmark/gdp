% What I Want to See in a Network API
  for the GDP
% Eric Allman
% 2017-06-16

***This is a proposal, not a specification***

This defines an API, not "bits on the wire" or, to the extent
possible, specifics of implementation.  The goal is that subject
to a few constraints, everything "above the line" can be insulated
from the actual network implementation, which must define the
packet format in whatever way it sees fit.  Conversely, everything
"below the line" will not be in any way affected by changes in the
higher level protocol.  Obviously both of those specifications will
be required, but this document isn't the place.

SEE ALSO: Nitesh's documents.

> [[Note: I'm doing a "proof of concept" implementation, essentially
just adapting the current version, in order to test the API for
completeness and simplicity.  In that process I'm still finding
new problems, so this **WILL** change.]]

> [[NOTE: "PDU", "blob", and "payload" are used somewhat interchangeably,
with a bit of "message" thrown in for good measure.  This is a bug,
not a feature.]]


## Key Points

This is essentially a message-based L4/L5 (Transport/Session
Layer) model.  The L5 part is about advertisements and routing.
Normally routing would be L3 (Network Layer), but our model
seems to be somewhat inverted in that the routing commands
live on top of the reliable transmission layer.  This is part
of the legacy of the "overlay network" model.

For the purposes of this document, "I", "me", "my", and "up" refer
to the user of this API.  "You", "your", and "down" refer to the
implementer of the API.  "Payload" is an opaque blob for you;
the term is approximately equal to "PDU".

> Using OSI language, a "payload" would be called a "Service
Data Unit" or SDU.  You add a header to that, and then it
becomes the PDU for the next layer down.  This document does
not discuss the internal structure of the payload.

GDP clients and servers ("my side" of the API) see:

* A message-based interface (i.e., PDUs are delimited by the
  network layer).
* Fragmentation, flow control, etc. already handled.
* Individual PDUs delivered reliably, that is, fragments of
  PDUs will be delivered in order, fragments will not be
  duplicated, and an error will be delivered if a fragment
  is lost.
* Different PDUs may be delivered out of order.
* PDU sizes are not inherently limited by underlying MTUs.

The network ("your side" of the API) sees (that is, I will give
you):

* Source and destination GDPnames (256-bit).
* A dynamically sized buffer in which to read or store a
  payload, which you must treat as an opaque blob.
* Advertisements of known GDPnames, authenticated using a
  certificate-based scheme, still not fully defined.
* Probably some hints vis-a-vis client expectations such as
  Quality of Service.  These remain _for further study_.
* A `libevent` event base to be used for registering I/O
  events.  I may register my own events in the same event
  base.
* _Other information to be determined._

Your side is responsible for:

* Routing.
* Retransmissions, etc.
* Reestablishing dropped connections (e.g., in presence of
  router failure).
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
  Note: it's impossible to write a header that includes the payload
  length until the size has been determined, which often means that
  the entire payload must be assembled in advance.

* This should be easy to implement (from the network perspective)
  for the existing TCP-based, no fragmentation model, but is it
  reasonably sane for other I/O models?

* Some way of doing multicast, notably for subscriptions.

Design/Implementation Notes:

* This interface uses `libevent` (see <http://libevent.org>).
  There is really no way around my picking the library without
  inverting the control flow, which would leave us forced to
  use threads.  As a result, `libevent` semantics are built
  into this proposed API.

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
* `gdp_chan_x_t` contains "My" private data.  This evaluates to
  `struct gdp_chan_x`, which I must define if I want to dereference
  that structure.  It is opaque to you.
* `gdp_cursor_t` is opaque to "my side" of the API; it provides a
  streaming interface to a payload while the client is receiving.
  Internally ("your side") it is fair game for other use, in
  particular, it may be useful on the sending side.
* `gdp_cursor_x_t` is the cursor equivalent of `gdp_chan_x_t`.
* `gdp_buf_t` implements dynamically allocated, variable sized
  buffers (already exists; based on `libevent`).  Details of that
  interface are not included here (but are described in the "GDP
  Programmatic API" document).
* `gdp_name_t` is the 256-bit version of a GDP name.
* `gdp_adcert_t` is whatever information is needed to advertise
  a GDPname.  This is _for further study_.


### Initialization


#### \_gdp\_chan\_init (class method)

I promise to call this routine on startup:

~~~
	EP_STAT _gdp_chan_init(
			struct event_base *evbase,
			void *options);
~~~

The `evbase` parameter is a `libevent` event base that I will
create and pass to you.
The `options` parameter is for future use.  Until then, I'll
always pass it as `NULL`.



### Channel Management

A Channel is a somewhat arbitrary class used to store whatever the
network ("your") side needs.  I promise to create at least one
channel when I start up.

#### \_gdp\_chan\_open (constructor)

~~~
	typedef EP_STAT gdp_cursor_recv_cb_t(
			gdp_cursor_t *cursor,
			uint32_t recv_flags);
	typedef EP_STAT gdp_chan_send_cb_t(
			gdp_chan_t *chan,
			gdp_buf_t *payload);
	typedef EP_STAT gdp_chan_ioevent_cb_t(
			gdp_chan_t *chan,
			int ioevent_flags);
	typedef EP_STAT gdp_chan_advert_func_t(
			gdp_chan_t *chan,
			int action,
			void *adata);

	EP_STAT _gdp_chan_open(
			const char *addrspec,
			void *qos,
			gdp_cursor_recv_cb_t *cursor_recv_cb,
			gdp_chan_send_cb_t *chan_send_cb,
			gdp_chan_ioevent_cb_t *chan_ioevent_cb,
			gdp_chan_advert_func_t *advert_func,
			gdp_chan_x_t *cdata,
			gdp_chan_t **chan);
~~~

Creates a channel to a GDP switch located at `addrspec` and stores
the result in `*chan`.  The format is a semicolon-delimited list of
`hostname:port` entries.  The entries in the list should be tried
in order.  If `addrspec` is NULL, the `swarm.gdp.routers`
runtime parameter is used.  The `qos` parameter is intended to
hold additional open parameters (e.g., QoS requirements), but for now
I promise to pass it as `NULL`.  The callbacks are described below.
The `cdata` parameter is saved and is available to callbacks on this
channel.

**`cursor_recv_cb`**:  When a new PDU is ready to read on the
associated channel (as encapsulated into `cursor`, call
`cursor_recv_cb`, including the total size of the payload (not
necessarily what is available for read right now).  The `cursor`
is a handle created by the network layer that I must pass into
`_gdp_cursor_recv` (see below) during the lifetime of this callback
in order to actually read the data, and that I promise to never
use outside this callback.

**`chan_send_cb`**: The `gdp_send_cb` is not used at this time; for
now, if it is non-NULL `_gdp_chan_open` should return
`GDP_STAT_NOT_IMPLEMENTED`.  The parameters to this function are
subject to change.

**`chan_ioevent_cb`**: When a channel is closed by the other end of
the connection, or on I/O error, `gdp_close_cb` is called.  The
`ioevent_flags` parameter is from the following set:

| Flag			| Meaning					|
|-----------------------|-----------------------------------------------|
| `GDP_IOEVENT_CONNECT`	| Connection established			|
| `GDP_IOEVENT_EOF`	| End of file on channel (i.e., it was closed)	|
| `GDP_IOEVENT_ERROR`	| Error occurred on channel			|

Note that this is a bitmap; multiple flags may be set on a single call.

> [[Nitesh brings up the question of `dst` filtering.  It isn't clear
which side of the interface this belongs on.]]

#### \_gdp\_chan\_close (destructor)

~~~
	EP_STAT _gdp_chan_close(
			gdp_chan_t *chan);
~~~

Deallocate `chan`.  All resources are freed.  I promise I will not
attempt to use `chan` after it is freed.



#### \_gdp\_chan\_get\_cdata (chan::get_cdata)

~~~
	void *_gdp_chan_get_cdata(
			gdp_chan_t *chan);
~~~

Returns the `cdata` associated with `chan`.


### Advertising and Certificates

> [[Who is responsible for certificate management and advertisements?
I guess that's likely to be me.  Drat.]]

Note that the naive implementation of this interface would cause at
least one round trip for each known name.  This will be particularly
expensive for log servers with large numbers of logs.  One possible
solution is to allow batching of advertisements, so the caller does
something like:

~~~
	_gdp_chan_advertise(chan, gnameA, ...);
	...
	_gdp_chan_advertise(chan, gnameZ, ...);
	_gdp_chan_advert_commit(chan);
~~~

This would cause an actual send to the routing layer.  This would
mean that challenge callbacks would not be synchronous with the
`_gdp_chan_advertise` calls.

#### \_gdp\_chan\_advertise (chan::advertise)

_This interface is still under development._

~~~
	// advertising challenge/response callback function type
	typedef EP_STAT (*chan_advert_cr_t)(
			gdp_chan_t *chan,
			gdp_name_t gname,
			int action,
			void *cdata,
			void *adata);

	// advertisement method
	EP_STAT _gdp_chan_advertise(
			gdp_chan_t *chan,
			gdp_name_t gname,
			gdp_chan_adcert_t *adcert,
			gdp_advert_cr_t *challenge_cb,
			void *adata);
~~~

Advertises the name `gname` on the given `chan`.  If a certificate
needs to be presented, it should be passed as `adcert`.  If the
underlying layer needs further interaction (e.g., for challenge/response)
it should call `challenge_cb`.  The `adata` is passed through untouched.

If the routing subsystem challenges `adcert` the `challenge_cb`
function will be invoked with the `chan`, the `gname` being
challenged, an `action` **to be determined**, any challenge data
issued by the router side as `cdata`, and the `adata` field directly
from `_gdp_chan_advertise`.

> [[What is `adcert` exactly?  Where does it come from?]]

#### \_gdp\_chan\_withdraw (chan::withdraw)

~~~
	EP_STAT _gdp_chan_withdraw(
			gdp_chan_t *chan,
			gdp_name_t gname,
			void *adata);
~~~

Withdraw a previous advertisement, for example, if a log is removed
from a given server.

> [[Question: should `_gdp_chan_advertise` return a data structure
that can be passed to `_gdp_chan_withdraw` or is the gname enough?]]

> [[Question: is there any point in sending `adata` here?]]


### Sending Messages

#### \_gdp\_chan\_send (chan::send)

~~~
	EP_STAT _gdp_chan_send(
			gdp_chan_t *chan,
			gdp_XXX_t *target,
			gdp_name_t src,
			gdp_name_t dst,
			gdp_buf_t *payload);
~~~

Sends the entire contents of `payload` to the indicated `dst` over
`chan`.  The source address is specified by `src`.

> [[Does this clear `payload` or leave it unchanged?]]

The `target` give clues as to exactly where to deliver the
message.  For example, it might be any replica of a given log,
all replicas of a given log (e.g., for quorum read), or a specific
replica.  It can also be used to indicate the equivalent of IPv4
"type of service" (precedence, reliability, etc).  How it is specified
is _for further study_.  For now, I promise to pass in NULL; if it
is not NULL, `GDP_STAT_NOT_IMPLEMENTED` should be returned.

> [[Issue: There are issues regarding allowing an arbitrary `src` that
need to be explored.  You should never be permitted to send from
an address you aren't authorized to speak for, but the ultimate
responsibility for avoiding problems falls to the receiver.]]

> [[Implementation Note: in the short run this may return
`GDP_STAT_PDU_TOO_LONG` if the size of `payload` exceeds an
implementation-defined limit.  This should be as large as possible
since it limits the size of any single record stored in the GDP.]]


#### \_gdp\_chan\_multicast (chan::multicast)

> [[Note: this is a placeholder.]]

~~~
	EP_STAT _gdp_chan_multicast(
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


### Receiving Messages (cursor operations)

Message data is returned via a cursor, not a channel.  This
allows long messages to be returned in "chunks".

All of these routines should only be called from within
`cursor_recv_cb`.  The basic model is that the network layer
("you") invokes the callback when there is new data to be read
for a message on a channel.

I then do whatever processing is needed on that data.  It is not
required that the entire message be read before the callback is
invoked.  However, if the callback returns without consuming
data and new data arrives, it will be appended to the existing
data buffer.

#### \_gdp\_cursor\_get\_buf (cursor::get\_buf)

~~~
	gdp_buf_t *_gdp_cursor_get_buf(
			gdp_cursor_t *cursor,
			uint32_t flags);
~~~

Returns the (read-only) data buffer associated with the cursor.
This can only be called within a `cursor_recv_cb` callback, and the
resulting buffer must not be used outside of that callback.
If the callback does not consume the buffer data before returning,
any new data for that cursor will be appended to the buffer and
the callback will be invoked again.

The flags are:

| Name				| Meaning				|
|-------------------------------|---------------------------------------|
| `GDP_CURSOR_PARTIAL`		| Partial data; continuation expected	|
| `GDP_CURSOR_CONTINUATION`	| This data is continuation data	|
| `GDP_CURSOR_READ_ERROR`	| A read error occurred (see below)	|

Data read from a cursor is guaranteed to be presented in the
same order it was written with no duplicates or dropouts.
There is no such "in order" guarantee between different payloads.
If there is a read error on a partially delivered message, this
callback will be invoked one more time with the `GDP_CURSOR_READ_ERROR`
flag set.  Error detail may be exposed via `_gdp_cursor_get_estat`.

#### \_gdp\_cursor\_get\_payload\_size (cursor::get\_payload\_size)

~~~
	size_t _gdp_cursor_get_payload_size(
			gdp_cursor_t *cursor);
~~~

Returns the size of the complete payload.  Note that this is may be
greater than the amount of the payload in the current buffer.

#### \_gdp\_cursor\_get\_chan (cursor::get_chan)

~~~
	gdp_chan_t *_gdp_cursor_get_chan(
			gdp_cursor_t *cursor);
~~~

Returns the channel associated with `cursor`.

#### \_gdp\_cursor\_get\_endpoints (cursor::get\_endpoints)

~~~
	EP_STAT _gdp_cursor_get_endpoints(
			gdp_cursor_t *cursor,
			gdp_name_t *src,
			gdp_name_t *dst);
~~~

Returns the endpoints of the given `cursor` into `src` and `dst`.
These will be the same as passed to `gdp_recv_cb`.

#### \_gdp\_cursor\_get\_estat (cursor::get\_estat)

~~~
	EP_STAT _gdp_cursor_get_estat(
			gdp_cursor_t *cursor);
~~~

Returns the error status associated with the last input on the
`cursor`.  Normally of interest if `GDP_CURSOR_READ_ERROR` is set
in the flags.

#### \_gdp\_cursor\_set\_udata (cursor::set\_udata)

~~~
	void _gdp_cursor_set_udata(
			gdp_cursor_t *cursor,
			void *udata);
~~~

Sets a user-defined field in the cursor to `udata`.  The lifetime of
that data is limited to one message.  It may be used for carrying
state between partial messages.

It is the responsibility of the caller to clean up this data (e.g.
free any allocated memory) before the cursor is destroyed.

#### \_gdp\_cursor\_get\_udata (cursor::get\_udata)

~~~
	void *_gdp_cursor_set_udata(
			gdp_cursor_t *cursor);
~~~

Returns the user-defined field set by `_gdp_cursor_set_udata`.
If no user data has been set, returns NULL.


### Utilities


#### \_gdp\_cursor\_lock, \_gdp\_cursor\_unlock (cursor::lock, cursor::unlock)

~~~
	void _gdp_cursor_lock(
			gdp_cursor_t *cursor);

	void _gdp_cursor_unlock(
			gdp_cursor_t *cursor);
~~~

Lock or unlock a `cursor`.  This is a mutex lock.


## Status Codes

Later.
