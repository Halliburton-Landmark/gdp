#!/usr/bin/env python

from twisted.internet import reactor
from twisted.web.resource import Resource
from twisted.web.server import Site

import urlparse
import argparse
import json
import gdp


class GDPWriterResource(Resource):

    isLeaf = True

    def __init__(self, logname, keyfile, nolist, allow_nop):

        Resource.__init__(self)

        gdp.gdp_init()
        self.logname = logname
        self.nolist = nolist
        self.allow_nop = allow_nop
        open_info = {}

        if keyfile is not None:
            skey = gdp.EP_CRYPTO_KEY(filename=keyfile,
                                        keyform=gdp.EP_CRYPTO_KEYFORM_PEM,
                                        flags=gdp.EP_CRYPTO_F_SECRET)
            open_info['skey'] = skey

        self.gcl = gdp.GDP_GCL(gdp.GDP_NAME(logname), gdp.GDP_MODE_AO,
                                                open_info=open_info)

    def __process(self, uri, d):

        if self.allow_nop == False and len(d.keys())==0:
            return ""

        if self.nolist == True:
            for k in d.keys():
                d[k] = d[k][0]
        try:
            d["_meta"] = uri
            self.gcl.append({"data": json.dumps(d)})
            return "Ok"
        except gdp.MISC.EP_STAT_Exception as e:
            return str(e)


    def render_GET(self, request):
        print "Received GET request:", request.uri, request.args
        return self.__process(request.uri, request.args)

    def render_PUT(self, request):
        print "Received PUT request:", request.uri, request.args
        return self.__process(request.uri, request.args)


if __name__ == "__main__":

    # Set up argument parsing code
    parser = argparse.ArgumentParser()
    parser.add_argument("-p", "--port", type=int, default=8080,
                            help="TCP port to serve requests on, default=8080")
    parser.add_argument("--nolist", action="store_true", default=False,
                            help="Attempt to write a JSON object as a " + \
                            "key=>value format, as opposed to a " + \
                            "key=>[value] format. If an applicaiton " + \
                            "sends multiple values for the same key, " + \
                            "a random value will be picked. Default: False")
    parser.add_argument("--allow-nop", action="store_true", default=False,
                            help="Do not ignore requests that don't look " +\
                            "like a key-value pair. By default, any " + \
                            "requests that don't have a key=value pair " + \
                            "are ignored")
    parser.add_argument("-k", "--keyfile",
                        help="Path to a signature key file for the log")
    parser.add_argument("logname", help="Name of log")

    # Get the actual arguments
    args = parser.parse_args()

    # Create a 'site' object to serve requests on 
    site = Site(GDPWriterResource(args.logname, args.keyfile,
                        args.nolist, args.allow_nop))
    # Setup the reactor with the port
    reactor.listenTCP(args.port, site)
    print "Starting REST interface on port %d, appending data to log %s" % \
                (args.port, args.logname)
    reactor.run()
