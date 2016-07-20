#!/usr/bin/env python

"""
A visualization tool for time-series data stored in GDP logs
"""

from twisted.internet import reactor
from twisted.web.resource import Resource, NoResource
from twisted.web.server import Site
from twisted.web import static
from threading import Lock

from GDPcache import GDPcache
import argparse
import json
import gviz_api
from datetime import datetime
import time

class DataResource(Resource):

    isLeaf = True
    def __init__(self):
        Resource.__init__(self)
        self.GDPcaches = {}
        self.lock = Lock()


    def __handleQuery(self, request):

        args = request.args

        # get query parameters
        logname = args['logname'][0]
        startTime = float(args['startTime'][0])
        endTime = float(args['endTime'][0])

        # reqId processing
        reqId = 0
        tqx = args['tqx'][0].split(';')
        for t in tqx:
            _t = t.split(':')
            if _t[0] == "reqId":
                reqId = _t[1]
                break

        self.lock.acquire()
        gdpcache = self.GDPcaches.get(logname, None)
        if gdpcache is None:
            gdpcache = GDPcache(logname)
            self.GDPcaches[logname] = gdpcache
        self.lock.release()

        sampleRecord = gdpcache.mostRecent()
        sampleData = json.loads(sampleRecord['data'])
        if sampleData['device'] == "BLEES":
            keys = ['pressure_pascals', 'humidity_percent',
                    'temperature_celcius', 'light_lux',
                    'acceleration_advertisement', 'acceleration_interval']
        elif sampleData['device'] == "Blink":
            keys = ['current_motion', 'motion_since_last_adv',
                    'motion_last_minute']
        elif sampleData['device'] == 'PowerBlade':
            keys = ['rms_voltage', 'power', 'apparent_power', 'energy',
                    'power_factor']
        else:
            keys = []

        # create data
        data = []
        for datum in gdpcache.getRange(startTime, endTime):

            _time = datetime.fromtimestamp(datum['ts']['tv_sec'] + \
                                (datum['ts']['tv_nsec']*1.0/10**9))
            __xxx = [_time]
            for key in keys:
                _raw_data = json.loads(datum['data'])[key]
                if _raw_data == "true":
                    _data = 1.0
                elif _raw_data == "false":
                    _data = 0.0
                else:
                    _data = float(_raw_data)
                __xxx.append(_data)
            data.append(tuple(__xxx))

        desc = [('time', 'datetime')]
        for key in keys:
            desc.append((key, 'number'))
        data_table = gviz_api.DataTable(desc)
        data_table.LoadData(data)

        response = data_table.ToJSonResponse(order_by='time', req_id = reqId)

        return response


    def render_GET(self, request):

        resp = ""
        respCode = 200

        try:
            resp = self.__handleQuery(request)
        except Exception as e:
            print e
            request.setResponseCode(500)
            respCode = 500
            resp = "500: internal server error"

        return resp


class MainResource(Resource):

    def __init__(self):
        Resource.__init__(self)
        self.staticresource = static.File("./static")
        self.dataresource = DataResource()
        self.putChild('static', self.staticresource)
        self.putChild('datasource', self.dataresource)

    def getChild(self, name, request):
        if name == "":
            return self
        else:
            return NoResource()

    def render_GET(self, request):
        with open('index.html') as fh:
            return fh.read()

if __name__ == '__main__': 

    parser = argparse.ArgumentParser()
    parser.add_argument("-p", "--port", type=int, default=8888,
                        help="TCP port to serve requests on")
    parser.add_argument("-l", "--logfile", default="visServer.log",
                        help="Logfile to log requests to")

    args = parser.parse_args()
    site = Site(MainResource(), logPath=args.logfile)
    reactor.listenTCP(args.port, site)

    print "Starting web-server on port", args.port
    reactor.run()

