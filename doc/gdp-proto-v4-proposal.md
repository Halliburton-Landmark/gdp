<!-- Use "pandoc -sS -o gdp-proto-v4-proposal.html gdp-proto-v4-proposal.md" to process this to HTML -->
<!-- vim: set ai sw=4 sts=4 ts=4 : -->

GDP Protocol Version 4 Proposal
===============================

_Eric Allman, 2018-02-25_

Everything here is for discussion.

Principles
----------

These are guiding principles for this proposal.

* What we have previously called the routing layer is broken up
  into two sub-layers: a "forwarding" (a.k.a. "switch") layer that
  shovels PDUs from a source to a destination as quickly as
  possible, and a "routing" layer that deals with advertisements,
  DHT lookup, etc.

* Forwarders only look at the PDU header.  Forwarders should need
  a minimum amount of information.  Generally speaking, this means
  the source and destination of the PDU, Time to Live (TTL), PDU
  size, and fragmentation and sequencing information.  If the
  switching layer does not have enough information to fully process
  the PDU, it can invoke the routing layer.

* Ideally the TTL would not be in a higher-level encoding of the
  header, since that is the only field that forwarders need to
  change.  It should certainly _not_ be included in any signature.
  
* The routing layer has a distinguished address.  Advertisements
  are sent using this address as the destination.

* "End to end" PDU contents should be in some language-agnostic format
  (such as Protocol Buffers or Cap'n Proto).

* End-to-end PDU contents should be opaque to the switching layer; in
  particular, it should be possible for it to be encrypted.

* One option in this proposal includes a format amenable to transport
  over limited size, unreliable, datagram-based (L3) transport
  protocols (UDP or raw IP), and hence needs to have the information
  necessary for PDU fragmentation/reassembly, packet retransmission,
  out of order delivery, and all that standard low-level stuff.
  This need not be an "all-or-nothing" decision: for example, one
  possible transport would be RUDP, which includes retransmissions and
  acknowledgements, flow control, and guaranteed ordering, but
  not fragmentation/reassembly.

* Implementation of some operations (e.g., delivery of subscribed
  data) should play well with multicast.

* Size matters.  PDU headers need to be constrained.  In particular,
  the addresses (totaling 64 bytes when expanded) are just too large;
  they need to be encodable into a "FlowID", which can be thought of
  as a cache of common header information.

* Implementation matters.  Notably, forwarders should be able to view
  most of the PDU as opaque (not just the data), and should be optimized
  for speed.  In particular, forwarder-based header information should
  _not_ be encoded in a format that requires that a complex data
  structure be unserialized and reserialized.


Definitions
-----------

* FlowID: a mechanism for encoding source and destination information
  into a smaller size to optimize bandwidth utilization.

* Forwarding: the process of transmitting a PDU from a source to
  a destination, with the assumption that the endpoints are already
  known.  If they are unknown, the Routing layer must get involved.

* Octet: a single eight-bit data element.  This is the network term
  for a "byte" (but since some architectures have non-eight-bit
  bytes, "octet" is used instead).

* Packet: A data block that can be transmitted intact over an
  underlying network protocol.  Depending on the context, this might
  be limited by physical constraints (e.g., the maximum size of an
  ethernet frame) or by logical constraints (e.g., the maximum
  chunk size in SCTP).

* PDU (Protocol Data Unit): A block of data representing a single
  actionable unit (e.g., command or response).  In some cases
  multiple PDUs might be encoded into a larger "Transport PDU".

* Routing: determining the location of a destination.


Protocol Overview
-----------------

Every PDU consists of three parts, two of which may be of zero length.
The first and third parts are visible to the routing layer.

The first portion is the Protocol Header.  This is binary encoded,
and intended to contain all the information that the Switching Layer
needs to route packets (assuming that layer has the necessary
routing information already cached).  It is designed to be small
and fast.

The second portion is the Payload.  This is intended to be encoded
in some industry-standard, platform agnostic encoding.  Popular
examples include ProtoBuf and Cap'n Proto.  The contents of the
Payload is message dependent.  It may be zero length.

The third portion is an optional Protocol Trailer (notably, containing
an HMAC).  This can be used for both integrity checking (e.g., in lieu
of a checksum) and as a security verification.  [[_Does this mean that
only some PDUs will have checksums?_]]

This alternative assumes that we are running over some Layer 4
protocol that gives us reliable, ordered transmission of arbitrarily
sized PDUs (for example, TCP).  That would make this a Layer 5
protocol.

For efficiency, it is possible to encode multiple commands (each
essentially an independent payload) in the PDU if the `MPAYLOAD`
flag is set.


### Protocol Header

Details are shown after the table.  The **Alternative** field
relates to the flags indicated in the **Flags** field.


| Offset 	| Len 	| Alternative 	| Detail								|
|-------:	|----:	|-------------	|-------------------					|
|      0	|   1	|				| Magic/Version [1]						|
|	   1	|   1	|				| Header Length	 [2]					|
|	   2	|   1	|				| Time to Live							|
|	   3	|   1	|				| Flags/Type of Service [3]				|
|      4    |   1   |               | Trailer Length [5]					|
|      5	|   1	|				| Reserved (MBZ)						|
|      6	|   2	|				| Payload Length [4]					|
|      8 	|   4   | IFLOWID      	| Initiator FlowID						|
|      - 	|   4   | RFLOWID      	| Return FlowID							|
|      - 	|  32  	| GDPADDR     	| Destination Addr						|
|      - 	|  32  	| GDPADDR     	| Source Addr							|
|      -	|   V	|				| Options								|


[1]	Magic and Version identify this PDU.  Must be 4.

[2] Size of header in units of 32 bits.  This starts at offset
	zero and includes the options.  This constrains the header to at
	most 1020 octets.

[3]	Flags indicate the format of the additional parts of the header,
	notably choosing between FlowIds and full GDP addresses.
	Type of Service is _for further study_.
	If the MPAYLOAD flag is set, this PDU includes multiple
	payloads, each starting with a Trailer Length and a Payload
	Length field.  The Payload Length field is the total size of
	the merged payloads, including initial counts.  This is
	_for further study_.
	[[_What are the semantics if none of IFLOWID, RFLOWID, or
	GDPADDR are not set?  Should probably make GDPADDR be the
	default.  But is there some reason that one might have both
	full 256-bit addresses and flow ids in the same PDU?_]]

[4] Size of Payload in units of 8 bits.  It is represented in network
	byte order (big endian).  This constrains the maximum size of a
	PDU payload to 2 ^ 16 - 1 = 65,535 octets.

[5]	Size of PDU Trailer (primarily HMAC) in units of 32 bits.
	If the MPAYLOAD flag is set, this must be zero.
	[[_The format of the Trailer still needs to be defined.  It is
	what Nitesh has called the HMAC portion, but this is a
	generalization of that concept._]]
	[[_Does this need to be in units of 32 bits?  It might be easier
	if it were in octet units._]]

The total size of the PDU is the sum of the Header Length, the
Payload Length, and the Trailer Length.

The source and destination address can be specified either explicitly
or by encoding into a FlowID.  Mechanisms for managing FlowIDs are
[[_being explored by Nitesh_]].

[[_Note to Nitesh: I'm using "Initiator FlowID" instead of just
"FlowID" to avoid confusion between the specific and the generic.
For example, consider the statement "the FlowID and the Return
FlowID are both FlowIDs."_]]


### Flag Bits

Flags are:

| Value		| Name		| Detail							|
|------:	|-----:		| -----------------					|
| 0x01		| GDPADDR	| Includes Dst & Src Addrs			|
| 0x02		| IFLOWID	| Includes Initiator FlowID			|
| 0x04		| RFLOWID	| Includes Return FlowID			|
| 0x08		| MPAYLOAD	| Multiple commands inside payload	|
| 0x10		| MULTICAST	| [[_??? Per Nitesh_]]				|

Note that a single PDU could contain both Flow IDs and GDP addresses.
It's unclear that this is well defined, and might change.


### Options

Options are used to convey additional information to the switching
layer, e.g., Quality of Service.  These are for future use.  Note
that options are included in the header size, so routers that do
not support options can skip this part without additional processing.

Each Option starts with a single octet of option id.  If the high
order bit of that option id is zero, the bottom four bits encode the
length of the option value (excluding the option id), otherwise the
following octet contains the length.
If the length is encoded in in the option id octet, that length is
part of the option id.  For example, option 0x10 and 0x14 are
different options, the former of length zero and the latter of length
four.  In comparison, the size of option 0x82 is contained in the
following octet, and the size is not part of the option id, so
0x82 0x00 and 0x82 0x04 are the same option, with values of length
zero and four respectively.

Unrecognized options must be ignored (but passed on).
[[_Perhaps there should be some encoding that indicates that an
option is essential, in the sense of the CoAP [RFC7252] distinction
between "Critical" and "Elective" options._]]

Options:

| Value		| Name		| Detail					|
|------:	|-----:		| -----------------			|
| 0x00		| EOOL		| End of Option List		|

[[_Need to define other options._]]

Note that some Options may be implied by a FlowID, in the same way
that a 256-bit address is implied by a FlowID.


Payload Encoding
----------------

[[_Move this into another document, or see the code._]]


Trailer Encoding
----------------

[[_Not specified at this time._]]


Things to Address
=================

* Do we need to allow for fields to handle physical networks?  That
  implies message fragmentation and reassembly, which means a few
  fields from both IP and TCP need to be added, e.g. window size,
  sequence number, ack number, and checksum.
  [[_Rick, you may want to address this.  In particular, are you
  encapsulating these (Layer 4 PDUs) into a more basic Layer 3 PDU
  that is not IP?_]]

* Should multiple commands be permitted in one PDU?  If so, the
  Payload and Trailer information needs to be in some sort of
  array.  Everything in these commands MUST have the same
  source and destination addresses, since this is specific to the
  routing/forwarding layer (i.e, forwarding of a partial PDU is not
  permitted).

* Do we need a trailer at all?  I think so, but Nitesh may be able
  to give more information.
