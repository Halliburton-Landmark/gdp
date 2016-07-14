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
import time

class GDPcache:
    """
    A caching + query-by-time layer for GDP. We don't need a lock here,
        since all our opreations are atomic (at least for now)
    limit is the number of records to keep in the cache. This is a soft
        limit, which means that we will go over the limit on various
        occasions, but we will try to be within a certain factor of the
        specified limit (by default, 2). This enables us to minimize the
        cleanup overhead.
    """

    def __init__(self, logname, limit=10000):
        """ Initialize with just the log name, and optionally cache size """

        gdp.gdp_init()      # No side-effects of calling this multiple times
        self.logname = logname
        self.lh = gdp.GDP_GCL(gdp.GDP_NAME(logname), gdp.GDP_MODE_RO)
        self.limit = limit
        self.cache = {}     # recno => record cache   (limited size)
        self.atime = {}     # recno => time of access (same size as cache)

        self.__read(1)
        self.__read(-1)

    def __cleanup(self):
        """
        Make sure that we adhere to the size limit on cache. However, as
            an optimization, we never delete the smallest and the largest
            record number we know of. In addition, the limit is a *soft*
            limit, meaning that we avoid doing cleanup as soon as we are
            just one above the limit, for example.
        """

        # A quick return for the case we don't need cleanup for
        if len(self.cache)<=2*self.limit: return

        print "Length before cleanup", len(self.cache)
        min_recno = min(self.cache.keys())
        max_recno = max(self.cache.keys())

        # get an ordered list of keys based on inverse LRU
        iLRUorder = sorted(self.cache.keys(), key=lambda x:self.atime[x],
                                             reverse=True )
        if min_recno in iLRUorder: iLRUorder.remove(min_recno)
        if max_recno in iLRUorder: iLRUorder.remove(max_recno)

        while len(self.cache)>self.limit:
            # the last item in the reverse sorted list (least recently used)
            lru = iLRUorder.pop()
            self.cache.pop(lru)
            self.atime.pop(lru)
        print "Length after cleanup", len(self.cache)

    
    def __time(self, recno):        # cache for tMap
        """ give us the time function. A way to switch between the log-server
            timestamps and the timestamps in data """
        datum = self.__read(recno)
        return datum['ts']['tv_sec'] + (datum['ts']['tv_nsec']*1.0/10**9)


    def __read(self, recno):        # cache for, of course, records
        """ read a single record by recno, but add to cache """
        if recno in self.cache.keys():
            self.atime[recno] = time.time()
            return self.cache[recno]
        datum = self.lh.read(recno)
        pos_recno = datum['recno']
        self.cache[pos_recno] = datum
        self.atime[pos_recno] = time.time()
        self.__cleanup()
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
            self.atime[recno] = time.time()
            ret.append(datum)
        self.__cleanup()
        return ret


    def __findRec(self, t):
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

    def get(self, t):
        """ Get the record just before time t (using a binary search)"""
        recno = self.__findRec(t)
        return self.__read(recno)


    def getRange(self, tStart, tEnd, numPoints=1000):
        """ return a sampled list of records, *roughly* numPoints long """
        _startR = self.__findRec(tStart)+1
        _endR = self.__findRec(tEnd)

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


