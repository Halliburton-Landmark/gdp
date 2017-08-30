#!/usr/bin/env python

"""
A log creation service that receives Log Creation commands from an
unmodified client and passes them to a log-server. The purpose of
such a log-creation service is to provide a layer of indirection
between the clients and log-servers (and ensure log-duplication
is handled cleanly, before we can come up with a better solution).


"""

from GDPService import GDPService
import gdp
import random
import cPickle
import argparse
import sys
import time
import sqlite3
import logging
import os

GDP_SERVICE_ADDR = gdp.GDP_NAME("logcreationservice")
GDP_LOG_NAME = "logcreationservicelog"
GDP_LOG_ADDR = gdp.GDP_NAME(GDP_LOG_NAME)

DEFAULT_ROUTER_PORT = 8007
DEFAULT_ROUTER_HOST = "172.30.0.1"

### these come from gdp/gdp_pdu.h from the GDP C library

# Acks/Naks
GDP_ACK_MIN = 128
GDP_ACK_MAX = 191
GDP_NAK_C_MIN = 192
GDP_NAK_C_MAX = 223
GDP_NAK_C_MIN = 223
GDP_NAC_C_MAX = 239
GDP_NAK_R_MIN = 240
GDP_NAK_R_MAX = 254

# specific commands
GDP_CMD_CREATE = 66
GDP_NAK_C_BADREQ = 192


class logCreationService(GDPService):

    def __init__(self, GDPaddress, router, logservers, dbname, **kwargs):
        """
        GDPaddress: the address of this particular service
        router: a 'host:port' string representing the GDP router
        logservers: a list of log-servers on the backend that we use
        dbname: sqlite database location
        """

        ## First call the __init__ of GDPService
        super(logCreationService, self).__init__(GDPaddress, router, **kwargs)

        ## Setup instance specific constants
        self.logservers = [gdp.GDP_NAME(x).internal_name() for x in logservers]
        self.dbname = dbname

        ## Setup a connection to the backend database
        if os.path.exists(self.dbname):
            logging.info("Loading existing database, %r", self.dbname)
            self.conn = sqlite3.connect(self.dbname, check_same_thread=False)
            self.cur = self.conn.cursor()
        else:
            logging.info("Creating new database, %r", self.dbname)
            self.conn = sqlite3.connect(self.dbname, check_same_thread=False)
            self.cur = self.conn.cursor()

            ## Make table for bookkeeping
            self.cur.execute("""CREATE TABLE logs(
                                    logname TEXT UNIQUE, srvname TEXT,
                                    ack_seen INTEGER DEFAULT 0, 
                                    ts DATETIME DEFAULT CURRENT_TIMESTAMP,
                                    creator TEXT, rid INTEGER)""")
            self.cur.execute("""CREATE UNIQUE INDEX logname_ndx
                                                ON logs(logname)""")
            self.cur.execute("CREATE INDEX srvname_ndx ON logs(srvname)")
            self.cur.execute("CREATE INDEX ack_seen_ndx ON logs(ack_seen)")
            self.conn.commit()


    def __del__(self):

        ## Close database connections
        logging.info("Closing database connection to %r", self.dbname)
        self.conn.close()


    def request_handler(self, req):
        """
        The routine that gets invoked when a PDU is received. In our case,
        it can be either a new request (from a client), or a response from
        a log-server.
        """

        # check if it's a request from a client or a response from a logd.

        if req['cmd']<128:      ## it's a command

            ## First check for any error conditions. If any of the
            ## following occur, we ought to send back a NAK

            if req['src'] in self.logservers:
                logging.info("error: received cmd %d from server", req['cmd'])
                return self.gen_bad_request_resp(req)

            if req['cmd'] != GDP_CMD_CREATE:
                logging.info("error: recieved unknown request")
                return self.gen_bad_request_resp(req)

            logging.info("Received Create request from a client")

            ## figure out the data we need to insert in the database
            logname = req['data'][:32]
            srvname = random.choice(self.logservers)
            ## private information that we will need later
            creator = req['src']
            rid = req['rid']

            ## log this to our backend database
            __logname = gdp.GDP_NAME(logname).printable_name()
            __srvname = gdp.GDP_NAME(srvname).printable_name()
            __creator = gdp.GDP_NAME(creator).printable_name()
            try:
                logging.debug("inserting to database %r, %r, %r, %d",
                                __logname, __srvname, __creator, rid)
                self.cur.execute("""INSERT INTO logs (logname, srvname,
                                    creator, rid) VALUES(?,?,?,?);""",
                                    (__logname, __srvname, __creator, rid))
                self.conn.commit()

            except sqlite3.IntegrityError:

                logging.info("Log already exists")
                return self.gen_bad_request_resp(req)

            ## Send a spoofed request to the logserver
            spoofed_req = req
            spoofed_req['src'] = self.GDPaddress
            spoofed_req['dst'] = srvname
            spoofed_req['rid'] = self.cur.lastrowid

            # now return this spoofed request back to the transport layer
            # Since we have overridden the destination, it will go
            # to a log server instead of the actual client
            return spoofed_req

        else: ## response.

            ## Sanity checking
            if req['src'] not in self.logservers:
                logging.info("error: received a non-response from logserver")
                return self.gen_bad_request_resp(req) 

            logging.info("Received response from a log-server")

            ## Fetch the original creator and rid from our database
            self.cur.execute("""SELECT creator, rid, ack_seen FROM logs
                                            WHERE rowid=?""", (req['rid'],))
            dbrows = self.cur.fetchall()

            good_resp = len(dbrows)==1
            if good_resp:
                (__creator, orig_rid, ack_seen) = dbrows[0]
                creator = gdp.GDP_NAME(__creator).internal_name()
                if ack_seen != 0:
                    good_resp = False

            if not good_resp:
                logging.info("error: bogus response")
                return self.gen_bad_request_resp(req)
            else:
                logging.info("Setting ack_seen to 1 for row %d", req['rid'])
                self.cur.execute("""UPDATE logs SET ack_seen=1
                                            WHERE rowid=?""", (req['rid'],))
                self.conn.commit()

            # create a spoofed reply and send it to the client
            spoofed_reply = req
            spoofed_reply['src'] = self.GDPaddress
            spoofed_reply['dst'] = creator
            spoofed_reply['rid'] = orig_rid

            # return this back to the transport layer
            return spoofed_reply


    def gen_bad_request_resp(self, req):
        resp = dict()
        resp['cmd'] = GDP_NAK_C_BADREQ
        resp['src'] = self.GDPaddress
        resp['dst'] = req['src']
        return resp


if __name__ == "__main__":

    ## argument parsing
    parser = argparse.ArgumentParser(description="Log creation service")

    parser.add_argument("-D", "--debug", action='store_true',
                                help="Turn on debugging")
    parser.add_argument("-i", "--host", type=str, default=DEFAULT_ROUTER_HOST,
                                help="host of gdp_router instance. "
                                     "default = %s" % DEFAULT_ROUTER_HOST)
    parser.add_argument("-p", "--port", type=int, default=DEFAULT_ROUTER_PORT,
                                help="port for gdp_router instance. "
                                     "default = %d" % DEFAULT_ROUTER_PORT)
    parser.add_argument("dbname", type=str, 
                                help="filename for sqlite database")
    parser.add_argument("logservers", type=str, nargs="+",
                                help="log daemons that this instance of "
                                     "log creation service should use")

    args = parser.parse_args()

    ## done argument parsing, instantiate the service
    logging.basicConfig(level=logging.DEBUG)
    service = logCreationService(GDP_SERVICE_ADDR.internal_name(),
                                    "%s:%d" % (args.host, args.port),
                                    args.logservers, args.dbname,
                                    debug=args.debug)

    ## all done, start the service (and sleep indefinitely)
    logging.info("Starting logcreationservice")
    service.start()

    try:
        while True:
            time.sleep(1)
    except (KeyboardInterrupt, SystemExit):
        service.stop()
