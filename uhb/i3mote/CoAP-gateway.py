#!/usr/bin/env python

## A script that:
# - finds the physical sensors and their corresponding IPv6 addresses
#   connected to the current mesh.
# - Sets up named pipes (to be used as output files) for each of the
#   physical sensors
# - Starts up off-the-shelf `coap-client` binary with appropriate
#   arguments (especially important to include observe)
# - Starts up translator script that takes the raw COAP messages and
#   converts them to JSON string (based on the data structure

import urllib2
import json
import os
import tempfile
import time
import struct
import sys
import gdp
import subprocess
import threading

COAP_BINARY="NOT_SET"
FORMAT = ">hHHIhhhhhhBBHHHHHHHHHHHHH"
BUFSIZE = struct.calcsize(FORMAT)


class Node:
    """
    Represents a physical node.
    """

    def __init__(self, _eui64, _ipv6, prefix):

        self.eui64 = _eui64.encode('ascii', 'ignore').translate(None, '-')
        self.ipv6 = _ipv6.encode('ascii', 'ignore')
        self.prefix = prefix
        self.logname = "%s.%s" % (self.prefix, self.eui64)

        # Make a temp file where data from the coap-client will go. It's
        #    a cleaner approach than redirecting STDOUT, since restarting
        #    the writer or reader individually is possible.
        tmpdir = tempfile.gettempdir()
        _fifo = "i3mesh-" + self.eui64
        self.fifo = os.path.join(tmpdir, _fifo)
        # Make sure there's no old file that a reader will get confused with
        try:
            os.unlink(self.fifo)
        except OSError as e:
            pass

        print ">>> Node (%s), \n\tIPV6 (%s), \n\ttmpfile (%s), \n\tlog(%s)" %\
                         (self.eui64, self.ipv6, self.fifo, self.logname)

        # Setup threads
        self.coap_thr = None
        self.reader_thr = None
        self.exit_lock = threading.Lock()
        self.exit_reader_thr = False
        self.POLLING_INTERVAL = 0.5   # polling interval for read thread

        self.g = gdp.GDP_GCL(gdp.GDP_NAME(self.logname), gdp.GDP_MODE_AO)


    def poke_threads(self):
        """
        If threads haven't been started, or died for some reason,
            fix them
        """

        self.exit_lock.acquire()

        if (self.coap_thr is None) or (self.coap_thr.is_alive()==False):
            self.coap_thr = threading.Thread(target=self.CoAPThread)
            self.coap_thr.start()

        if (self.reader_thr is None) or (self.reader_thr.is_alive()==False):
            self.exit_reader_thr = False
            self.reader_thr = threading.Thread(target=self.ReaderThread)
            self.reader_thr.start()

        self.exit_lock.release()

    def CoAPThread(self):
        """
        Get COAP data from the specified ipv6 address, which represents
            a sensor with EUI64. Write this to a temp file at the
            specified location.
        """

        coap_args = [COAP_BINARY, '-v', '7', '-B', '120', '-s', '60',
                            '-o', self.fifo, '-m', 'get',
                            "coap://[%s]/sensors" % self.ipv6]
        subprocess.call(coap_args)

        # signal the reader thread to terminate when this exits
        # The reader thread will exit at most in polling interval
        self.exit_lock.acquire()
        try:
           os.unlink(self.fifo)
        except OSError:
            pass
        self.exit_reader_thr = True
        self.exit_lock.release()


    def ReaderThread(self):
        """
        Read data from the temp file written to by CoAP, parse it to JSON
            and append it to a GDP log (name derived from parameters)
        Write everything that hasn't been written yet
        """

        time.sleep(2)

        try:
            with open(self.fifo) as fh:

                seekptr = 0
                while True:

                    # there may be unread data, but we need to move on
                    if self.exit_reader_thr: break

                    assert seekptr%BUFSIZE == 0
                    fh.seek(seekptr)

                    # ugly
                    data = fh.read(BUFSIZE)
                    if len(data)<BUFSIZE:
                        time.sleep(self.POLLING_INTERVAL)
                        continue

                    json_string = self.parseCOAP(data[:50])
                    print "## %s>> %s" % (self.eui64, json_string)
                    self.g.append({'data': json_string})
                    seekptr += BUFSIZE

        except IOError as e:
            print "## %s >>" % self.eui64, e,
            print "\tIt's ok; writer hasn't wrote anything since last restart"

    @staticmethod
    def parseCOAP(buf):
        """
        Read a TI COAP message and convert it to JSON string
        """

        # This is from `app.js` provided by TI
        assert len(buf) == BUFSIZE

        parsed = struct.unpack(FORMAT, buf)
        message = { "tamb"              : parsed[0]/100.0,
                    "rhum"              : parsed[1]/100.0,
                    "lux"               : parsed[2]/100.0,
                    "press"             : parsed[3]/100.0,
                    "gyrox"             : parsed[4]/100.0,
                    "gyroy"             : parsed[5]/100.0,
                    "gyroz"             : parsed[6]/100.0,
                    "accelx"            : parsed[7]/100.0,
                    "accely"            : parsed[8]/100.0,
                    "accelz"            : parsed[9]/100.0,
                    "led"               : parsed[10],
                    "channel"           : parsed[11],
                    "bat"               : parsed[12]/100.0,
                    "eh"                : parsed[13]/100.0,
                    "cc2650_active"     : parsed[14]/100.0,
                    "cc2650_sleep"      : parsed[15]/100.0,
                    "rf_tx"             : parsed[16]/100.0,
                    "rf_rx"             : parsed[17]/100.0,
                    "ssm_active"        : parsed[18]/100.0,
                    "ssm_sleep"         : parsed[19]/100.0,
                    "gpsen_active"      : parsed[20]/100.0,
                    "gpsen_sleep"       : parsed[21]/100.0,
                    "msp432_active"     : parsed[22]/100.0,
                    "msp432_sleep"      : parsed[23]/100.0,
                    "others"            : parsed[24]/100.0 }
        return json.dumps(message)




def fetch_nodes():
    """ Fetches the list of nodes from http://localhost/nodes """
    response = urllib2.urlopen("http://localhost/nodes")
    json_str = response.read()
    json_data = json.loads(json_str)
    # Note that the resulting data has no strings, it only has unicodes
    return json_data


def main(coap, prefix):

    global COAP_BINARY
    COAP_BINARY = coap

    gdp.gdp_init()
    _nodes = fetch_nodes()

    nodes = []
    for n in _nodes:
        if n['_id'] == 1: continue   # ignore the gateway
        node = Node(n['eui64'], n['address'], prefix)
        nodes.append(node)

    while True:
        for node in nodes: node.poke_threads()
        time.sleep(1)

if __name__ == "__main__":

    if len(sys.argv)<3:
        print "Usage: %s <full-path-to-coap-client> <log-prefix>" % sys.argv[0]
        print "    coap-client is the libcoap binary for your platform"
        print "    log-prefix could be 'edu.berkeley.eecs.swarmlab.device'"
        sys.exit(1)

    main(sys.argv[1], sys.argv[2])
