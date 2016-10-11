#!/usr/bin/env python

"""
Quick and dirty COAP => GDP bridge to get the TI i3mote data. 
- Highly specific to the setup from TI.
- Does not auto-create logs for the sensors
- Needs to be run on the same Beaglebone that runs the TI's stack
- Requires openwsn coap library: https://github.com/openwsn-berkeley/coap
"""

from coap import coap, coapException
import gdp
import threading    # we run one thread for each physical sensor
import urllib2      # Get the list of nodes from http://localhost/nodes
import json         # List of nodes is a JSON object
import struct       # Parsing COAP message
import sys
import time


def main(location):

    gdp.gdp_init()

    # Get the list of current nodes
    __raw_nodelist = urllib2.urlopen("http://localhost/nodes").read()
    nodelist = json.loads(__raw_nodelist)

    # Go through the nodelist and start threads
    threads = []
    for node in nodelist:
        if node['_id'] == 1: continue   # ignore the gateway
        th = threading.Thread(target=ReaderThread,
                                args= (node['address'], node['_id'],
                                            node['eui64'], location))
        threads.append(th)

    # Actually start the threads
    for th in threads:
        th.start()



def ReaderThread(ipv6, _id, eui64, location):
    """
    Gets COAP data for a sensor with 'eui64' and 'ipv6' and dumps it in
        a log named 'edu.berkeley.eecs.location.device.<eui64>'
    """

    _eui64 = eui64.encode('ascii', 'ignore').translate(None, "-")
    _ipv6 = ipv6.encode('ascii', 'ignore')

    # get coap setup
    c = coap.coap(udpPort=5000+_id)

    # get gdp setup
    logname = "edu.berkeley.eecs.%s.device.%s" % (location, _eui64)
    print logname

    time.sleep(2)
    g = gdp.GDP_GCL(gdp.GDP_NAME(logname), gdp.GDP_MODE_AO)

    # Just a counter to keep track of non-timeouts vs timeouts
    ctr, failCtr = 0, 0

    while True:

        try:
            data = c.GET("coap://[%s]/sensors" % _ipv6)
            json_string = parseCOAP(''.join([chr(b) for b in data]))
            ctr += 1

            # append to log
            g.append({"data": json_string})

        except coapException.coapTimeout as e:
            failCtr += 1

        # print _id, ctr, failCtr



def parseCOAP(buf):
    """
    Read a TI COAP message and convert it to JSON string
    """
    
    # Here's a message parsing code in JS
    # function obs_parse(id,data){
    #   obs_list[id].freshness=Date.now();
    #   var obj_data = {};
    #   var idx=0;
    #   obj_data.tamb=data.readInt16BE(idx)/100;idx+=2;
    #   obj_data.rhum=data.readUInt16BE(idx)/100;idx+=2;
    #   obj_data.lux=data.readUInt16BE(idx)/100;idx+=2;
    #   obj_data.press=data.readUInt32BE(idx)/100;idx+=4;
    #   obj_data.gyrox=data.readInt16BE(idx)/100;idx+=2;
    #   obj_data.gyroy=data.readInt16BE(idx)/100;idx+=2;
    #   obj_data.gyroz=data.readInt16BE(idx)/100;idx+=2;
    #   obj_data.accelx=data.readInt16BE(idx)/100;idx+=2;
    #   obj_data.accely=data.readInt16BE(idx)/100;idx+=2;
    #   obj_data.accelz=data.readInt16BE(idx)/100;idx+=2;
    #   obj_data.led=data.readUInt8(idx);idx+=1;
    #   obj_data.channel=data.readUInt8(idx);idx+=1;
    #   obj_data.bat=data.readUInt16BE(idx)/100;idx+=2;
    #   obj_data.eh=data.readUInt16BE(idx)/100;idx+=2;
    #   obj_data.cc2650_active=data.readUInt16BE(idx)/100;idx+=2;
    #   obj_data.cc2650_sleep=data.readUInt16BE(idx)/100;idx+=2;
    #   obj_data.rf_tx=data.readUInt16BE(idx)/100;idx+=2;
    #   obj_data.rf_rx=data.readUInt16BE(idx)/100;idx+=2;
    #   obj_data.ssm_active=data.readUInt16BE(idx)/100;idx+=2;
    #   obj_data.ssm_sleep=data.readUInt16BE(idx)/100;idx+=2;
    #   obj_data.gpsen_active=data.readUInt16BE(idx)/100;idx+=2;
    #   obj_data.gpsen_sleep=data.readUInt16BE(idx)/100;idx+=2;
    #   obj_data.msp432_active=data.readUInt16BE(idx)/100;idx+=2;
    #   obj_data.msp432_sleep=data.readUInt16BE(idx)/100;idx+=2;
    #   obj_data.others=data.readUInt16BE(idx)/100;idx+=2;
    #   var msg={_id:id,sensors:obj_data};
    #   console.log("observer data: "+id);
    #   ws.broadcast(JSON.stringify(msg));
    # }
    
    
    fmt =  ">hHHIhhhhhhBBHHHHHHHHHHHHH"
    coap_size = struct.calcsize(fmt)
    assert len(buf) == coap_size

    parsed = struct.unpack(fmt, buf)
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



if __name__=="__main__":
    if len(sys.argv)<2:
        print "Usage: %s <location>" % sys.argv[0]
        sys.exit(1)

    main(sys.argv[1])
