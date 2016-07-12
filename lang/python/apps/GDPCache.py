#!/usr/bin/env python

# ----- BEGIN LICENSE BLOCK -----                                               
#   GDP: Global Data Plane                                                      
#   From the Ubiquitous Swarm Lab, 490 Cory Hall, U.C. Berkeley.                
#                                                                               
#   Copyright (c) 2015, Regents of the University of California.                
#   All rights reserved.                                                        
#                                                                               
#   Permission is hereby granted, without written agreement and without         
#   license or royalty fees, to use, copy, modify, and distribute this          
#   software and its documentation for any purpose, provided that the above     
#   copyright notice and the following two paragraphs appear in all copies      
#   of this software.                                                           
#                                                                               
#   IN NO EVENT SHALL REGENTS BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT,      
#   SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOST               
#   PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION,     
#   EVEN IF REGENTS HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.         
#                                                                               
#   REGENTS SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT           
#   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS           
#   FOR A PARTICULAR PURPOSE. THE SOFTWARE AND ACCOMPANYING DOCUMENTATION,      
#   IF ANY, PROVIDED HEREUNDER IS PROVIDED "AS IS". REGENTS HAS NO              
#   OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS,          
#   OR MODIFICATIONS.                                                           
# ----- END LICENSE BLOCK -----      

import sys
sys.path.append("../")
import gdp

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


