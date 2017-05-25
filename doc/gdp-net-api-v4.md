% What I Want to See in a Network API
  for the GDP
% Eric Allman
% 2017-05-24

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
  use threads.

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
	EP_STAT _gdp_chan_open(
			const char *addrspec,
			void *qos,
			void *udata,
			gdp_chan_t **chan);
~~~

Creates a channel to a GDP switch located at `addrspec` and stores
the result in `*chan`.  The format is a semicolon-delimited list of
`hostname:port` entries.  The entries in the list should be tried
in order.  If `addrspec` is NULL, the `swarm.gdp.routers`
runtime parameter is used.  The `qos` parameter is intended to
hold specifications (e.g., QoS requirements), but for now I promise to
pass it as `NULL`.  The `udata` parameter is saved and is available
to any callbacks on this channel.

#### \_gdp\_chan\_close (destructor)

~~~
	EP_STAT _gdp_chan_close(
			gdp_chan_t *chan);
~~~

Deallocate `chan`.  All resources are freed.  I promise I will not
attempt to use `chan` after it is freed.

#### \_gdp\_chan\_set\_callbacks (chan::set_callbacks)

> [[Need callbacks for received data, failure on a connection.
Anything else?]]

~~~
	typedef EP_STAT chan_recv_cb_t(
			gdp_chan_t *chan,
			gdp_cursor_t *cursor,
			gdp_name_t *src,
			gdp_name_t *dst,
			size_t payload_len);
	typedef EP_STAT gdp_send_cb(
			gdp_chan_t *chan,
			gdp_buf_t *payload),
	typedef EP_STAT (*gdp_close_cb)(
			gdp_chan_t *chan,
			int what);

	EP_STAT _gdp_chan_set_callbacks(
			gdp_chan_t *chan,
			chan_recv_cb_t *gdp_recv_cb,
			chan_send_cb_t *gdp_send_cb,
			chan_close_cb_t *gdp_close_cb);
~~~

When a new PDU is ready to read on `chan`, call `gdp_recv_cb`,
including the total size of the payload (not necessarily what is
available for read right now).  The `cursor` is a handle created
by the network layer that I must pass into `_gdp_cursor_recv` (see below)
during the lifetime of this callback in order to actually read the
data, and that I promise to never use outside this callback.

The `gdp_send_cb` is not used at this time; for now, if it is non-NULL
`_gdp_chan_set_callbacks` should return `GDP_STAT_NOT_IMPLEMENTED`.
The parameters to this function are subject to change.

When a channel is closed by the other end of the connection, or on
I/O error, `gdp_close_cb` is called.  the `what` parameter is from
the same set of values, i.e., `GDP_IOEVENT_CLOSE` or `GDP_IOEVENT_ERROR`.

> [[Nitesh brings up the question of `dst` filtering.  It isn't clear
which side of the interface this belongs on.]]


#### \_gdp\_chan\_get\_udata (chan::get_udata)

~~~
	void *_gdp_chan_get_udata(
			gdp_chan_t *chan);
~~~

Returns the `udata` associated with `chan`.


### Advertising and Certificates

> [[Who is responsible for certificate management and advertisements?
I guess that's likely to be me.  Drat.]]

#### \_gdp\_chan\_advertise (chan::advertise)

_This interface is still under development._

~~~
	// challenge/response callback function type
	typedef EP_STAT (*advert_cr_cb_t)(
			gdp_chan_t *chan,
			int action,
			void *cdata,
			void *adata);

	// advertisement method
	EP_STAT _gdp_chan_advertise(
			gdp_chan_t *chan,
			gdp_name_t gname,
			gdp_adcert_t *adcert,
			gdp_cr_func_t *challenge_cb,
			void *adata);
~~~

Advertises the name `gname` on the given `chan`.  If a certificate
needs to be presented, it should be passed as `adcert`.  If the
underlying layer needs further interaction (e.g., for challenge/response)
it should call `challenge_cb`.

The callback function is passed the `chan`, an `action` **to be
determined**, and any challenge data needed in order to continue as
`cdata`.  The `adata` field is passed directly from `_gdp_chan_advertise`.

> [[What is `adcert` exactly?  Where does it come from?]]

#### \_gdp\_chan\_withdraw (chan::withdraw)

~~~
	EP_STAT _gdp_chan_withdraw(
			gdp_chan_t *chan,
			gdp_name_t gname);
~~~

Withdraw a previous advertisement, for example, if a log is removed
from a given server.

> [[Question: should `_gdp_chan_advertise` return a data structure
that can be passed to `_gdp_chan_withdraw` or is the gname enough?]]


### Sending and Receiving Messages

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
replica.  How it is specified is _for further study_.

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


#### \_gdp\_cursor\_recv (cursor::recv)

This can only be called within a receive callback.  It acts on a
cursor rather than on the channel itself.  It is not necessary for
the entire payload to be in memory for this call to return data,
nor is it necessary to read the entire payload at once.

~~~
	EP_STAT _gdp_cursor_recv(
			gdp_cursor_t *cursor,
			gdp_buf_t *payload,
			size_t *payload_len,
			uint32_t flags);
~~~

Read up to `*payload_len` octets from `cursor` into `payload`.
Returns the number of octets actually read into `*payload_len`.
If `payload` already has data, the new data is appended.

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

> [[Include a timeout, or is it sufficient to say that if
`*len` ≠ 0 it will return immediately? And the semantics imply
that it wcan return at any time once it has at least one
octet.]]

> [[Should it wait until at least one octet is read?
This should never happen, since it can only be called during a
receive callback, which asserts that at least some data is
available.]]

> [[If I try to read a portion of the PDU that is not yet available
should it block or return a different status code (which would
mean that the callback would be invoked again when more data
became available)?  If it fails there should be some way to say
"no, I really want to block."]]

Flags:

| Flag Name		| Meaning				|
|-----------------------|---------------------------------------|
| `GDP_CURSOR_BLOCK`	| Block until all PDU data is read	|


### Utilities

#### \_gdp\_cursor\_get\_endpoints (cursor::get_endpoints)

~~~
	EP_STAT _gdp_cursor_get_endpoints(
			gdp_cursor_t *cursor,
			gdp_name_t *src,
			gdp_name_t *dst);
~~~

Returns the endpoints of the given `cursor` into `src` and `dst`.
These will be the same as passed to `gdp_recv_cb`.

It isn't clear this function is needed.


## Status Codes

Later.
