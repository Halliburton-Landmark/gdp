#!/usr/bin/env python

"""
A visualization tool for time-series data stored in GDP logs
"""

from twisted.internet import reactor
from twisted.web.resource import Resource, NoResource
from twisted.web.server import Site
from twisted.web import static
from threading import Lock

import gdp
import argparse
import json
import gviz_api
from datetime import datetime
import time

class GDPcache:
    """ A caching + query-by-time layer for GDP. We don't need a lock here,
        since all our opreations are atomic (at least for now)"""

    def __init__(self, logname, limit=1000):

        self.logname = logname
        self.lh = gdp.GDP_GCL(gdp.GDP_NAME(logname), gdp.GDP_MODE_RO)
        self.cache = {}     # a recno => record cache   (limited size)

        self.__read(1)
        self.__read(-1)

    
    def __time(self, recno):        # cache for tMap
        """ give us the time function. A way to switch between the log-server
            timestamps and the timestamps in data """
        datum = self.__read(recno)
        return datum['ts']['tv_sec'] + (datum['ts']['tv_nsec']*1.0/10**9)


    def __read(self, recno):        # cache for, of course, cache
        """ read a single record by recno, but add to cache """
        if recno in self.cache.keys():
            return self.cache[recno]
        datum = self.lh.read(recno)
        pos_recno = datum['recno']
        self.cache[pos_recno] = datum
        return datum


    def __multiread(self, start, num):
        """ same as read, but efficient for a range. Use carefully, because
        I don't check for already-cached entries """
        self.lh.multiread(start, num)
        ret = []
        while True:
            event = self.lh.get_next_event(None)
            if event['type'] == gdp.GDP_EVENT_EOS: break
            datum = event['datum']
            recno = datum['recno']
            self.cache[recno] = datum
            ret.append(datum)
        return ret


    def __findRecNo(self, t):
        """ find the most recent record num before t, i.e. a binary search"""

        self.__read(-1)     # just to refresh the cache
        _startR, _endR = min(self.cache.keys()), max(self.cache.keys())

        # first check the obvious out of range condition
        if t<self.__time(_startR): return _startR
        if t>=self.__time(_endR): return _endR

        # t lies in range [_startR, _endR)
        while _startR < _endR-1:
            p = (_startR + _endR)/2
            if t<self.__time(p): _endR = p
            else: _startR = p

        return _startR


    def get(self, tStart, tEnd, numPoints=1000):
        """ return a list of records, *roughly* numPoints long """
        _startR = self.__findRecNo(tStart)+1
        _endR = self.__findRecNo(tEnd)

        # can we use multiread?
        if _endR+1-_startR<4*numPoints:
            return self.__multiread(_startR, (_endR+1)-_startR)

        # if not, let's read one by one
        ret = []
        step = max((_endR+1-_startR)/numPoints,1)
        for r in xrange(_startR, _endR+1, step):
            ret.append(self.__read(r))

        return ret

    def mostRecent(self):
        return self.__read(-1)


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
                    'temperature_celcius', 'light_lux']
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
        for datum in gdpcache.get(startTime, endTime):

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
    gdp.gdp_init()
    site = Site(MainResource(), logPath=args.logfile)
    reactor.listenTCP(args.port, site)

    print "Starting web-server on port", args.port
    reactor.run()

