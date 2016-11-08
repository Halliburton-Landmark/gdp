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
    
        # Make a named pipe where data from the coap-client will go. It's
        #    a cleaner approach than redirecting STDOUT, since restarting
        #    the writer or reader individually is possible.
        tmpdir = tempfile.gettempdir()
        _fifo = "i3mesh-" + self.eui64
        self.fifo = os.path.join(tmpdir, _fifo)
        try:
            os.mkfifo(self.fifo)
        except OSError as e:
            print e

        # Setup threads
        self.coap_thr = None
        self.reader_thr = None

    def poke_threads(self):
        """
        If threads haven't been started, or died for some reason,
            fix them
        """

        if (self.coap_thr is None) or (self.coap_thr.is_alive()==False):
            self.coap_thr = threading.Thread(target=self.CoAPThread)
            self.coap_thr.start()

        if (self.reader_thr is None) or (self.reader_thr.is_alive()==False):
            self.reader_thr = threading.Thread(target=self.ReaderThread)
            self.reader_thr.start()

    
    def CoAPThread(self):
        """
        Get COAP data from the specified ipv6 address, which represents
            a sensor with EUI64. Write this to a fifo at the
            specified location.
        """
    
        coap_args = [COAP_BINARY, '-v', '9', '-B', '3600', '-s', '60',
                            '-o', self.fifo, '-m', 'get',
                            "coap://[%s]/sensors" % self.ipv6]
        subprocess.call(coap_args)


    def ReaderThread(self):
        """
        Read data from the fifo written to by CoAP, parse it to JSON
            and append it to a GDP log (name derived from parameters)
        """
    
        logname = "%s.%s" % (self.prefix, self.eui64)
        print logname
        time.sleep(2)
    
        g = gdp.GDP_GCL(gdp.GDP_NAME(logname), gdp.GDP_MODE_AO)
    
        with open(self.fifo) as fh:
    
            data = ""
            while True:
                data += fh.read(BUFSIZE)
                if len(data)<BUFSIZE:
                    time.sleep(0.1)
                    continue
                json_string = self.parseCOAP(data[:50])
                data = data[50:]
                g.append({'data': json_string})
        
 
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
        time.sleep(10)

if __name__ == "__main__":

    if len(sys.argv)<3:
        print "Usage: %s <full-path-to-coap-client> <log-prefix>" % sys.argv[0] 
        print "    coap-client is the libcoap binary for your platform"
        print "    log-prefix could be 'edu.berkeley.eecs.swarmlab.device'"
        sys.exit(1)

    main(sys.argv[1], sys.argv[2]) 
