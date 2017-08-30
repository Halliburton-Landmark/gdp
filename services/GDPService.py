#!/usr/bin/env python

"""

A library to perform network related operations (setting up TCP connections,
parsing incoming PDUs, sending out replies, etc) for a GDP service.

To use GDPService for your own service, at the very minimum, you need to
subclass GDPService and override the method 'request_handler'. Then, you need
to create an instance of this service with the 256-bit GDP address that this
service should listen on and the address of a GDP router (as a
"IP-address:port" string).

Any GDP PDU destined to the GDP address specified for at the instantiation time
is parsed and passed as a dictionary to the 'request_handler'. This request
handler should act on the received data and return a python dictionary
containing field=>value pairs for a PDU to be sent as a reply. GDPService adds
some defaults (such as source address, destination address, etc) unless these
are overridden in the response dictionary.

"""

from twisted.internet.protocol import ClientFactory, Protocol
from twisted.internet import reactor
import threading
import struct
import logging


def pprint(d):
    """
    Just our own pprint function that behaves nicely with binary strings
    """

    def to_hex(data):
        hex_repr = "".join([hex(ord(t))[2:] for t in data])
        if len(data) > 0:
            hex_repr = '\\x' + hex_repr
        return hex_repr

    s = "{" + "\n"
    for k in d.keys():
        tmp = d[k]
        if isinstance(tmp, int):
            p = str(tmp)
        else: p = "'" + to_hex(tmp) + "'"
        s += " '" + k + "': " + p + "," + "\n"
    s += "}"
    return s


class GDPProtocol(Protocol):

    """ An instance of this class is created for every connection """

    # this is the conventional GDP address of a router
    GDPROUTER_ADDRESS = (chr(255) + chr(0)) * 16

    def __init__(self, req_handler, GDPaddress):
        """ The GDP address for the service end point """

        self.GDPaddress = GDPaddress
        self.request_handler = req_handler
        self.buffer = ""

    def connectionMade(self):

        # Send the advertisement message
        advertisement = ('\x03' + '\x00' * 2 + '\x01' +
                            self.GDPROUTER_ADDRESS + self.GDPaddress +
                            '\x00' * 4 + '\x00' * 4 + '\x00'*4 )

        ## do network I/O in the reactor thread
        reactor.callFromThread(self.transport.write, advertisement)


    def terminateConnection(self, reason):
        """
        If we can't make sense out of any data on this connection
        anymore for any reason, terminate the connection.
        """

        # This just terminates the connection
        logging.info("Terminating connection, %r", reason)
        self.transport.abortConnection()


    def dataReceived(self, data):

        """
        parses PDUs and invokes the PDU processing code in a separate
        worker thread
        """

        assert threading.currentThread().getName() == "ReactorThr"

        # Make sure we have some flow control.

        l1 = len(self.buffer)   # what is left from previous calls
        l2 = len(data)          # what we got now
        l = 0                   # seek pointer.  0 <= l < l1+l2

        while reactor.running:

            def __get_byte_num(n):
                """ Reference to bit number 'l+n' in self.buffer + data,
                    0-indexed """
                if l + n < l1:
                    return self.buffer[l + n]
                else:
                    return data[l + n - l1]

            if (l + 1) > (l1 + l2):
                break       # can't figure out the version

            # check the version number
            version = __get_byte_num(0)

            if version != '\x03':   # bogus version number
                self.terminateConnection("bogus version number")
                break

            if (l + 80) > (l1 + l2):
                # incomplete header for the current message
                # we need at least 80 bytes to figure out the length of
                #   this PDU
                break

            # calculate lengths we need to find if we received full PDU
            sig_len = (ord(__get_byte_num(72)) & 0x0f) * 256 + \
                            ord(__get_byte_num(73))
            opt_len = 4 * ord(__get_byte_num(74))

            len_arr = [__get_byte_num(idx) for idx in xrange(76, 80)]
            data_len = 0
            for __t in len_arr:
                data_len = data_len * 256 + ord(__t)

            # pdu_len is the total length of the PDU
            pdu_len = 80 + opt_len + data_len + sig_len

            if (l + pdu_len) > (l1 + l2):
                break     # incomplete message

            # 3 cases:
            # - entire message in self.buffer
            # - message split between self.buffer and data
            # - entire message in data
            # case 2 can only happen once every call of this function

            message = None
            if (l + pdu_len) <= l1:  # case 1
                message = buffer(self.buffer, l, pdu_len)
            elif l >= l1:         # case 3
                message = buffer(data, l - l1, pdu_len)
            else:               # case 2
                # This is not a buffer anymore because of concatenation
                message = (buffer(self.buffer, l, l1 - l) +
                           buffer(data, 0, (pdu_len) - (l1 - l)))

            l = l + pdu_len       # update l

            # Work on this PDU, all the PDU processing logic goes here;
            # this gets called in a separate worker thread.
            reactor.callInThread(self.process_PDU,
                                    message, opt_len, sig_len, data_len)

            # End of while loop

        if reactor.running:

            # Now update the buffer with the remaining data
            # 2 cases: 1) Either the new buffer is a substring of just data
            # OR, 2) it is substring of old buffer + all data

            if l > l1:    # case 1
                self.buffer = buffer(
                    data, l - l1, (l1 + l2) - l)    # just a buffer
            else:       # case 2
                self.buffer = (buffer(self.buffer, l, l1 - l) +
                               buffer(data, 0, len(data)))  # Expensive string
            # if len(self.buffer)>0: assert ord(self.buffer[0])==2


    def process_PDU(self, message, opt_len, sig_len, data_len):
        """
        Act on a PDU that is already parsed. All the PDU processing
        logic goes here.

        Note that the message could be a string or a buffer. For now, it
        just parses it into a dictionary, calls the request request
        handler on the parsed dictionary and sends the returned python
        dictionary to the provided destination
        """

        # DEBUG-info:
        # 'message' is a PDU received by gdp_router from remote
        # address 'self.transport.getPeer()'

        # create a dictionary
        msg_dict = {}
        msg_dict['fmt'] = message[0]
        msg_dict['ttl'] = ord(message[1])
        msg_dict['res'] = message[2]
        msg_dict['cmd'] = ord(message[3])

        msg_dict['dst'] = message[4:36]
        msg_dict['src'] = message[36:68]

        msg_dict['rid'] = struct.unpack("!I", message[68:72])[0]

        msg_dict['sigalg'] = ord(message[72])>>4
        msg_dict['siglen'] = (ord(message[72]) & 0x0f) * 256 + ord(message[73])
        msg_dict['siginfo'] = message[72:74]
        assert msg_dict['siglen'] == sig_len

        msg_dict['olen'] = 4*ord(message[74])
        assert msg_dict['olen'] == opt_len

        msg_dict['flags'] = message[75]

        msg_dict['dlen'] = struct.unpack("!I", message[76:80])[0]
        assert msg_dict['dlen'] == data_len

        ctr = 80
        if msg_dict['olen'] != 0:
            msg_dict['options'] = message[ctr:ctr+msg_dict['olen']]
        ctr += msg_dict['olen']

        msg_dict['data'] = message[ctr:80+ctr+msg_dict['dlen']]
        ctr += msg_dict['dlen']

        msg_dict['sig'] = message[ctr:]

        # get the response dictionary
        # msg_dictuest_handler is to be supplied by GDPProtocolFactory, which
        #   in turn gets it from GDPService
        # XXX: What all information should be made available here?
        keys = set(msg_dict.keys()) & set(['cmd', 'dst', 'src', 'flags',
                                                'rid', 'data', 'options'])
        data = {k: msg_dict[k] for k in keys}
        logging.debug("Incoming:\n%s", pprint(data))
        resp = self.request_handler(data)
        logging.debug("Outgoing:\n%s", pprint(resp))

        if resp is not None:
            # if a destination is specified in the response, it will take
            #   precedence
            self.send_PDU(resp, dst=msg_dict['src'])


    def send_PDU(self, data, **kwargs):
        """
        Create a GDP message and write it to the network connection
        destination is a 256 bit address. This should *not* be called
        from the reactor thread.

        Qpen question: what all fields should be accepted from the caller?
        So far: cmd, dst, src, flags, rid, data, options
        """

        assert threading.currentThread().getName() != "ReactorThr"

        msg = {}
        msg['fmt'] = '\x03'
        msg['ttl'] = chr(0)                     # FIXME
        msg['res'] = '\x00'
        msg['cmd'] = chr(data['cmd'])           # mandatory

        # make sure we do have a destination
        assert 'dst' in data.keys() + kwargs.keys()
        msg['dst'] = str(data.get('dst', kwargs.get('dst', None)))
        msg['src'] = str(data.get('src', self.GDPaddress))

        # XXX: The following are from protocol ver 0x02
        # These are replaced by siginfo
        # msg['sigalg'] = '\x00'
        # msg['siglen'] = chr(0)
        msg['siginfo'] = '\x00\x00'

        assert len(data.get('options', ''))%4 == 0
        msg['olen'] = chr(len(data.get('options', ''))/4)
        msg['flags'] = str(data.get('flags', '\x00'))

        msg['rid'] = struct.pack("!I", data.get('rid', 0))

        msg['dlen'] = struct.pack("!I", len(data.get('data', '')))

        msg['options'] = str(data.get('options', ''))
        msg['data'] = str(data.get('data', ''))
        msg['sig'] = ''

        # some assertions (all may not be be necessary)
        valid_size = {  'fmt': 1, 'ttl': 1, 'res': 1, 'cmd': 1,
                        'dst': 32, 'src': 32,
                        'siginfo': 2, 'olen': 1, 'flags': 1,
                        'rid': 4, 'dlen': 4}
        for k in valid_size.keys():
            assert len(msg[k]) == valid_size[k]

        message = ( msg['fmt'] + msg['ttl'] + msg['res'] + msg['cmd'] +
                    msg['dst'] + msg['src'] +
                    msg['rid'] +
                    msg['siginfo'] + msg['olen'] + msg['flags'] +
                    msg['dlen'] +
                    msg['options'] + msg['data'] + msg['sig'] )

        ## the following gets called in the reactor thread
        reactor.callFromThread(self.transport.write, message)



class GDPProtocolFactory(ClientFactory):

    def __init__(self, req_handler, GDPaddress):
        self.GDPaddress = GDPaddress
        self.request_handler = req_handler

    def buildProtocol(self, remoteaddr):
        protocol = GDPProtocol(self.request_handler, self.GDPaddress)
        return protocol


class GDPService(object):
    """
    A generic GDP Service. In a perfect world, all GDP services should
    be subclasses of this class.

    To implement your own GDP service, override the 'request_handler' method
    and put your service's request processing logic.

    One can also override 'setup' to do service specific startup functions.
    These functions are called *before* establishing connection to the
    GDP router.
    """


    def __init__(self, GDPaddress, router):
        """
        address: 256 bit GDP address
        router: ip:port for a GDP router
        """

        ## parse the GDP router host:port
        t = router.split(":")
        self.router_host = t[0]
        self.router_port = int(t[1])
        self.GDPaddress = GDPaddress

        ProtocolFactory = GDPProtocolFactory(self.request_handler, GDPaddress)

        ## Call service specific setup code
        self.setup()

        ## Establish connection to the router (the reactor isn't running
        ## yet, so this will only get established when 'start()' is called.
        logging.debug("Connecting to host:%s, port:%d",
                            self.router_host, self.router_port)
        reactor.connectTCP(self.router_host, self.router_port, ProtocolFactory)


    def setup(self):
        "Initial setup for this service. Default: do nothing"
        pass


    def request_handler(self,req):
        """
        Receive a request (dictionary), with the following keys:
            cmd = Command field in GDP PDU
            src = Source GDP address (256 bit)
            dst = Destination GDP address,
            rid = Request ID
            data = Actual payload
        Ideally, a GDP service will override this method
        """
        pass


    def start(self):
        """
        Start the reactor (in a separate thread) and returns immediately. One
        should put appropriate logic after calling 'start' to keep main thread
        alive as long as possible.
        """
        reactor_thr = threading.Thread(name="ReactorThr",
                                 target=reactor.run,
                                 kwargs={'installSignalHandlers':False})
        reactor_thr.start()


    def stop(self):
        """ Stop serving requests, (ideally) called from main-thread """
        reactor.callFromThread(reactor.stop)
