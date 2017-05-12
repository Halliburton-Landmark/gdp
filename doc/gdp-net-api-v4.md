% What I Want to See in a Network API
  for the GDP
% Eric Allman
% 2017-05-11


SEE ALSO: Nitesh's documents.

## Key Points

This is essentially a message-based L4/L5 (Transport/Session
Layer) model.  The L5 part is about advertisements and routing.
Normally routing would be L3 (Network Layer), but our model
seems to be somewhat inverted in that the routing commands
live on top of the reliable transmission layer.  This is part
of the legacy of the "overlay network" model.

For the purposes of this document, "I", "me", and "my" refer
to the user of this API.  "You" and "your" refer to the
implementer of the API.

GDP clients and servers ("my side" of the API) see:

* Fragmentation, flow control, etc. already handled at some lower
  layer.
* Individual PDUs delivered reliably and in order.
* Different PDUs may be delivered out of order.
* PDU sizes are not inherently limited by underlying MTUs.

The network ("your side" of the API) sees:

* Source and destination GDPnames (256-bit).
* A dynamically sized buffer in which to read or store a blob.
* Advertisements of known GDPnames, authenticated using a
  certificate-based scheme, still not fully defined.
* Probably some hints vis-a-vis client expectations such as
  Quality of Service.  These remain _for further study_.
* _Other information to be determined._

... and is responsible for:

* Routing.
* Reliability (retransmissions, etc.).
* Fragmentation/Reassembly.
* In order delivery (i.e., all pieces of the PDU delivered in order).
* Compression (notably header compression).
* On-the-wire crypto (TLS or DTLS as appropriate).
* DoS mitigation.  Attack traffic should be stopped as soon as
  possible.

"Your side" of the API can live partially in the client library,
but some amount of it might live elsewhere, either in the
switch/forwarder/router layer or in a separate service.  I assume
that this layer will not rely on threads in client processes to
make it possible to run in low-end (non-MMU) processors.
_Note: this is a change from the V3 design, which assumes a
dedicated thread for I/O._

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

* This should be easy to implement (from the network perspective)
  for the existing TCP-based, no fragmentation model, but is it
  reasonably sane for other I/O models?

* Some way of doing multicast, notably for subscriptions.

Design/Implementation Notes:

* The existing GDP code uses `libevent` (<http://libevent.org>),
  and this interface has been designed to play well with that library.

Documentation Notes:

* This version is loosey-goosey with status codes.  If we decide
  that this is a good direction I will provide more detail.


## _OPEN ISSUES_

_This section is not intended to be part of the final document._

#### Integration with `libevent`

Currently the I/O loop (i.e., what I want out of this interface)
is implemented by `libevent`, and I use other features of that
library such as timeout and signal events.  Requiring the network
client library to use libevent seems inappropriate.

However, if the network gave me a file descriptor on which I
could base an event (or if the network added a `libevent` event
to an existing event base), I could own the event loop.  Does
that constrain the network implementation too much?  Notably, can I
demand `libevent` as an underlying technology?

Owning the event loop would also make it easier to create a
"low end" `libgdp` implementation which would not depend on
threads or an MMU and hence might be feasible on embedded
processors.


## API

Background:

* `gdp_chan_t` is opaque to "my side" of the API.
* `gdp_pdu_t` is opaque to "my side" of the API; it maintains
  the state of a single logical message from a known source to
  a known destination during receipt.  It exists to provide the
  potential of streaming interface, and is only used when
  receiving a PDU.  Internally ("your side") it is fair game
  for other use, in particular, for sending a PDU.
* `gdp_buf_t` implements dynamically allocated, variable sized
  buffers (already exists; based on `libevent`).  Details of that
  interface are not included here.
* `gdp_name_t` is the 256-bit version of a GDP name.
* `gdp_adcert_t` is whatever information is needed to advertise
  a GDPname.  This is _for further study_.

The API is intended to map easily into an object-oriented paradigm
with the first parameter to instance methods being `self`.  Class
methods generally return a status code; if they are allocating a
new object it will be returned through the last parameter.

The parenthetical comments in the titles are intended to provide
a model of how this would map into an OO environment.


### Initialization


#### gdp_chan_init (class global)

I promise to call this routine on startup:

~~~
	EP_STAT gdp_chan_init(
		void *unused);
~~~

The `unused` parameter is there for future use.  Until then, I'll
always pass it as `NULL`.


### Channel Management

A Channel is a somewhat arbitrary class used to store whatever the
network ("your") side needs.  I promise to create at least one
channel when I start up.

#### gdp_chan_open (constructor)

~~~
	EP_STAT gdp_chan_open(
		const char *addrspec;
		void *unused,
		gdp_chan_t **chan);
~~~

Creates a channel to a GDP switch located at `addrspec` and stores
the result in `*chan`.  If `addrspec` is NULL, the `swarm.gdp.routers`
runtime parameter is used.  The `unused` parameter is intended to
hold specifications (e.g., QoS requirements), but for now I promise to
pass it as `NULL`.

_[[Note: it isn't clear we need `gdp_chan_init`; it could just be
done on the first call to `gdp_chan_open` --- unless we need to use
the `unused` parameter.]]_

#### gdp_chan_close (destructor)

~~~
	EP_STAT gdp_chan_close(
		gdp_chan_t *chan);
~~~

Deallocate `chan`.  All resources are freed.  I promise I will not
use `chan` after it is freed.

#### gdp_chan_set_callbacks (chan::set_callbacks)

~~~
	EP_STAT gdp_chan_set_callbacks(
		gdp_chan_t *chan,
		EP_STAT *gdp_recv_cb(
			gdp_message_t *message,
			gdp_name_t *src,
			gdp_name_t *dst,
			size_t payload_len),
		EP_STAT *gdp_send_cb(
			gdp_message_t *message));
~~~

When a new PDU is ready to read on `chan`, call `gdp_recv_cb`,
including the total size of the payload (not necessarily what is
available for read right now).  Data is actually read using
`gdp_chan_recv` (see below).  The `message` is a handle created by
the network layer that I must pass into subsequent functions
during the lifetime of this callback, and that I promise to
never use outside this callback.  A single message is equivalent
to a single PDU.

The `gdp_send_cb` is not used at this time; for now, if it is non-NULL
`gdp_chan_set_callbacks` should return `GDP_STAT_NOT_IMPLEMENTED`.
The parameters to this function are subject to change.

_[[Nitesh brings up the question of `dst` filtering.  It isn't clear
which side of the interface this belongs on.]]_

#### gdp_chan_poll (chan::poll)

_[[I'm increasingly feeling like this is wrong --- I should own
the event loop, in which case I'll do this myself.]]_

~~~
	EP_STAT gdp_chan_poll(
		gdp_chan_t *chan,
		bool wait);
~~~

I will call this when I'm ready for new work.  I promise to call it
frequently, but never while running a read callback (???).  Roughly
speaking, it should check to see if there is any work to be done and
invoke appropriate callbacks.  If `wait` is set, it should block
until something happens, otherwise it should return immediately if
there is no work.  If the work completes successfully, return
`EP_STAT_OK`; otherwise, return an appropriate status TBD.

_[[Implementation Note: this is the equivalent of calling the
libevent routine `event_base_loop` with `EVLOOP_ONCE` included in
`flags`.]]_

_[[Implementation Note: this is not necessary if I "own" the event
loop.  If I do, the initialization code must add an appropriate
event to detect input during initialization.]]_

_[[Implementation Note: the semantics we probably want will be to
poll all channels, not a single one.  That's another argument for
having me own the event loop; individual channels can add events
to the global event loop as appropriate.]]_


### Advertising and Certificates

_[[Who is responsible for certificate management and advertisements?
I guess that's likely to be me.  Drat.]]_

#### gdp_chan_advertise (chan::advertise)

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
		gdp_cr_func_t *callback,
		void *udata);
~~~

Advertises the name `gname` on the given `chan`.  If a certificate
needs to be presented, it should be passed as `adcert`.  If the
underlying layer needs further interaction (e.g., for challenge/response)
it should call `callback`.

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


### Sending and Receiving Messages

#### gdp_chan_send (chan::send)

~~~
	EP_STAT gdp_chan_send(
		gdp_chan_t *chan,
		gdp_XXX_t target,
		gdp_name_t src,
		gdp_name_t dst,
		gdp_buf_t *blob);
~~~

Sends the entire contents of `blob` to the indicated `dst` over
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
`GDP_STAT_PDU_TOO_LONG` if the size of `blob` exceeds an
implementation-defined limit.  This should be as large as possible
since it limits the size of any single record stored in the GDP.]]_


### gdp_chan_multicast (chan::multicast)

~~~
	EP_STAT gdp_chan_multicast(
		gdp_chan_t *chan,
		gdp_name_t src,
		gdp_XXX_t multicast_addr,
		gdp_buf_t *blob);
~~~

Sends `blob` to multiple destinations as indicated by
`multicast_addr`.  It isn't clear what that is.

This is primarily intended for delivering subscription data.

This will certainly need support to set up a multicast channel,
probably with "new", "join", "leave", and "free" style interface.


#### gdp_pdu_recv (pdu::recv)

This can only be called within a receive callback.  It acts on a
single PDU rather than on the channel itself.

~~~
	EP_STAT gdp_pdu_recv(
		gdp_pdu_t *pdu,
		gdp_buf_t *blob,
		size_t *len);
~~~

Read up to `*len` octets from `pdu` into `blob`.  If `*len` = 0,
block until all octets comprising the current PDU have been read.
Returns the number of octets actually read into `*len`.
If `blob` already has data, the new data is appended.

Data read on a given message is guaranteed to be presented in the
same order it was written with no duplicates or dropouts.
There is no such "in order" guarantee between messages.

* If all data has been read, returns `EP_STAT_OK`.
* If data remains to be read, returns `GDP_STAT_KEEP_READING`.

_[[Include a timeout, or is it sufficient to say that if
`*len` ≠ 0 it will return immediately? And the semantics imply
that it wcan return at any time once it has at least one
octet.]]_

_[[Should it wait until at least one octet is read?
This should never happen, since it can only be called during a
receive callback, which asserts that at least some data is
available.]]_


### Utilities

#### gdp_pdu_get_endpoints (pdu::get_endpoints)

~~~
	EP_STAT gdp_pdu_get_endpoints(
		gdp_pdu_t *pdu,
		gdp_name_t *src,
		gdp_name_t *dst);
~~~

Returns the endpoints of the given `pdu` into `src` and `dst`.
These will be the same as passed to `gdp_recv_cb`.


## Status Codes

Later.
