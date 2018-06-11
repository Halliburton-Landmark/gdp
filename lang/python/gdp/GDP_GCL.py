#!/usr/bin/env python

# ----- BEGIN LICENSE BLOCK -----                                               
#	GDP: Global Data Plane
#	From the Ubiquitous Swarm Lab, 490 Cory Hall, U.C. Berkeley.
#
#	Copyright (c) 2015, Regents of the University of California.
#	All rights reserved.
#
#	Permission is hereby granted, without written agreement and without
#	license or royalty fees, to use, copy, modify, and distribute this
#	software and its documentation for any purpose, provided that the above
#	copyright notice and the following two paragraphs appear in all copies
#	of this software.
#
#	IN NO EVENT SHALL REGENTS BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT,
#	SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOST
#	PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION,
#	EVEN IF REGENTS HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
#	REGENTS SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT
#	LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
#	FOR A PARTICULAR PURPOSE. THE SOFTWARE AND ACCOMPANYING DOCUMENTATION,
#	IF ANY, PROVIDED HEREUNDER IS PROVIDED "AS IS". REGENTS HAS NO
#	OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS,
#	OR MODIFICATIONS.
# ----- END LICENSE BLOCK -----                                               


import weakref
import threading
import time
import random
import pprint
from MISC import *
from GDP_NAME import *
from GDP_DATUM import *
from GDP_GCLMD import *
from GDP_GCL_OPEN_INFO import *

### From https://stackoverflow.com/questions/19443440/
class WeakMethod(object):
    """A callable object. Takes one argument to init: 'object.method'.
    Once created, call this object -- MyWeakMethod() --
    and pass args/kwargs as you normally would.
    """
    def __init__(self, object_dot_method):
        self.target = weakref.proxy(object_dot_method.__self__)
        self.method = weakref.proxy(object_dot_method.__func__)
        ###Older versions of Python can use 'im_self' and 'im_func'
        ###in place of '__self__' and '__func__' respectively

    def __call__(self, *args, **kwargs):
        """Call the method with args and kwargs as needed."""
        return self.method(self.target, *args, **kwargs)
### End

class GDP_GCL(object):

    """
    A class that represents a GCL handle. A GCL handle resembles an open
        file handle in various ways. However, it is still different in
        certain ways

    Note that get_next_event is both a class method (for events on any of
        the GCL's) as well as an instance method (for events on a particular
        GCL). The auto-generated documentation might not show this, but
        something to keep in mind.
    """

    # We need to keep a list of all the open GCL handles we have, and
    #   their associated gdp handles. I don't see any cleaner solution
    #   to how to differentiate between events associated for different
    #   GCLs. Also, we no longer can free the handles automatically.
    #   Why? How would python know that we are not planning to get an
    #   event for a gin handle?

    # C pointer to python object mapping
    object_dir = {}

    # Event Queues (events that we have extracted out of the C event
    # queue, but don't belong to us). Maps udata => list of events
    ev_queues = {}
    ev_queues_lock = threading.Lock()


    # Thread ID => userdata mapping. We need to keep this in someplace other
    # than a local variable, so that it doesn't get cleaned up by
    # Garbage collection. Otherwise, our references to the memory get
    # cleaned up over time, resulting in undefined behaviour
    tid_udata_map = {}

    # Old udata, so that they don't get garbage collected either
    __old_udata = []


    class gdp_gin_t(Structure):

        "Corresponds to gdp_gin_t structure exported by C library"
        pass

    class gdp_event_t(Structure):

        "Corresponds to gdp_event_t structure exported by C library"
        pass

    def __new__(cls, name, iomode, open_info={}):
        """
        Open a GCL with given name and io-mode. The reason to do custom
            initialization in __new__ (rather than __init__) is because
            we'd like a one-to-one correspondence between the underlying
            C pointer for an open GCL and Python objects representing
            a GCL. However, the C library can return an existing pointer
            when an already open GCL is opened again.

        name=<name-of-gin>, iomode=<mode>
        name is a GDP_NAME object
        mode is one of the following: GDP_MODE_ANY, GDP_MODE_RO, GDP_MODE_AO,
                                      GDP_MODE_RA
        open_info is a dictionary. It contains extra information, such as the
            private signature key, etc. The key may be optional, depending on
            how the log was created and if a log-server is set up to reject 
            unsigned appends, for example.
            Here is the (incomplete) list of keys and their description:
            'skey': an instance of EP_CRYPTO_KEY containing the signature key
        """

        # __ptr is just a C style pointer, that we will assign to something
        __ptr = POINTER(cls.gdp_gin_t)()

        # we do need an internal represenation of the name.
        gin_name_python = name.internal_name()
        # convert this to a string that ctypes understands. Some ctypes magic
        # ahead
        buf = create_string_buffer(gin_name_python, 32+1)
        gin_name_ctypes_ptr = cast(byref(buf), POINTER(GDP_NAME.name_t))
        gin_name_ctypes = gin_name_ctypes_ptr.contents

        # (optional) Do a quick sanity checking on open_info
        #   use it to get a GDP_GCL_OPEN_INFO structure
        __gdp_open_info = GDP_GCL_OPEN_INFO(open_info)

        # open an existing gin
        __func = gdp.gdp_gin_open
        __func.argtypes = [GDP_NAME.name_t, c_int,
                           POINTER(GDP_GCL_OPEN_INFO.gdp_open_info_t),
                           POINTER(POINTER(cls.gdp_gin_t))]
        __func.restype = EP_STAT

        estat = __func(gin_name_ctypes, iomode,
                            __gdp_open_info.gdp_open_info_ptr,
                            pointer(__ptr))
        check_EP_STAT(estat)

        if addressof(__ptr.contents) in cls.object_dir.keys():
            ## need the '()' because we store weakrefs in object_dir
            newobj = cls.object_dir[addressof(__ptr.contents)]()
            ## the following assertion should hold, because anytime a GDP_GCL
            ## object is freed, object_dir is also cleaned up. IF object_dir
            ## has a pointer, that means the object hasn't been claimed by GC.
            assert newobj is not None
            return newobj
        else:
            # create a new instance
            newobj = super(GDP_GCL, cls).__new__(cls)
            newobj.ptr = __ptr
            newobj.gdp_open_info = __gdp_open_info
            cls.object_dir[addressof(__ptr.contents)] = weakref.ref(newobj)
            return newobj

        assert False    # should never get here


    def __init__(self, *args):
        ## Create the instance method 'get_next_event', in addition to
        ## already existing class method
        self.get_next_event = WeakMethod(self.__get_next_event)


    def __del__(self):
        "close this GCL handle, and free the allocated resources"

        if bool(self.ptr) == False:   # null pointers have a false boolean value
            # Null pointer => no memory allocated => nothing to clean up
            #   this happens if __init__ did not finish
            return

        # remove the entry from object directory
        self.object_dir.pop(addressof(self.ptr.contents), None)

        # call the C function to free associated C memory block
        __func = gdp.gdp_gin_close
        __func.argtypes = [POINTER(self.gdp_gin_t)]
        __func.restype = EP_STAT

        estat = __func(self.ptr)
        check_EP_STAT(estat)
        return

    @classmethod
    def create(cls, name, logd_name, metadata):
        """
        create a new GCL with 'name' on 'logd_name'
        metadata is a dictionary with keys as 32-bit unsigned integers
        """

        # we do need an internal represenation of the names.
        gin_name_python = name.internal_name()
        logd_name_python = logd_name.internal_name()

        # convert this to a string that ctypes understands. Some ctypes magic
        # ahead
        buf1 = create_string_buffer(gin_name_python, 32+1)
        gin_name_ctypes_ptr = cast(byref(buf1), POINTER(GDP_NAME.name_t))
        gin_name_ctypes = gin_name_ctypes_ptr.contents

        buf2 = create_string_buffer(logd_name_python, 32+1)
        logd_name_ctypes_ptr = cast(byref(buf2), POINTER(GDP_NAME.name_t))
        logd_name_ctypes = logd_name_ctypes_ptr.contents

        throwaway_ptr = POINTER(cls.gdp_gin_t)()

        md = GDP_GCLMD()
        for k in metadata:
            md.add(k, metadata[k])

        __func = gdp.gdp_gin_create
        __func.argtypes = [GDP_NAME.name_t, GDP_NAME.name_t,
                                POINTER(GDP_GCLMD.gdp_md_t),
                                POINTER(POINTER(cls.gdp_gin_t))]
        __func.restype = EP_STAT

        estat = __func(gin_name_ctypes, logd_name_ctypes,
                            md.gdp_md_ptr, throwaway_ptr)
        check_EP_STAT(estat)
        return

    def getmetadata(self):
        """
        return the metadata included at creation time.
            Represented as a python dict
        """

        __func = gdp.gdp_gin_getmetadata
        __func.argtypes = [POINTER(self.gdp_gin_t),
                                POINTER(POINTER(GDP_GCLMD.gdp_md_t))]
        __func.restype = EP_STAT

        gmd = POINTER(GDP_GCLMD.gdp_md_t)()
        estat = __func(self.ptr, byref(gmd)) 
        check_EP_STAT(estat)

        md = GDP_GCLMD(ptr=gmd)

        idx = 0
        metadata = {}
        while True:
            try: 
                (i,v) = md.get(idx)
                metadata[i] = v
                idx += 1
            except EP_STAT_SEV_ERROR:
                break

        return metadata
            

    def __read(self, query_param):
        """
        An internal helper function for read. Either read by record number
            or read by timestamp. If query_param is 'int', we assume it is
            query by record number. If query_param is 'dict', we assume it
            is query by timestamp
        """

        datum = GDP_DATUM()

        if isinstance(query_param, int):
            __query_param = gdp_recno_t(query_param)

            __func = gdp.gdp_gin_read
            __func.argtypes = [POINTER(self.gdp_gin_t), gdp_recno_t,
                                    POINTER(GDP_DATUM.gdp_datum_t)]
            __func.restype = EP_STAT

        elif isinstance(query_param, dict):

            __query_param = GDP_DATUM.EP_TIME_SPEC()
            __query_param.tv_sec = c_int64(query_param['tv_sec'])
            __query_param.tv_nsec = c_uint32(query_param['tv_nsec'])
            __query_param.tv_accuracy = c_float(query_param['tv_accuracy'])

            __func = gdp.gdp_gin_read_ts
            __func.argtypes = [POINTER(self.gdp_gin_t),
                                    POINTER(GDP_DATUM.EP_TIME_SPEC),
                                    POINTER(GDP_DATUM.gdp_datum_t)]
            __func.restype = EP_STAT

        else:   # should never reach here
            assert False

        estat = __func(self.ptr, __query_param, datum.gdp_datum)
        check_EP_STAT(estat)

        datum_dict = {}
        datum_dict["recno"] = datum.getrecno()
        datum_dict["ts"] = datum.getts()
        datum_dict["data"] = datum.getbuf()
        datum_dict["sig"] = datum.getsig()
        datum_dict["sigalg"] = datum.getsigmdalg()

        return datum_dict


    def read(self, recno):
        """
        Returns a datum dictionary. The dictionary has the following keys:
            - recno: the record number for this GDP
            - ts   : the timestamp, which itself is a dictionary with the keys
                        being tv_sec, tv_nsec, tv_accuracy
            - data : the actual data associated with this datum.
        """

        return self.__read(recno)


    def read_ts(self, tsdict):
        """
        Same as 'read', but takes a time-stamp dictionary instead of
        a record number. The time-stamp dictionary has the following
        fields:
            - tv_sec: seconds since epoch (an integer)
            - tv_nsec: nano seconds (an integer)
            - tv_accuracy: accuracy (a float)
        """

        # the internal implementation is the same, we don't really
        #   need two different functions. The only reason is to make
        #   it clear to the user that reading by record number has
        #   a different meaning than reading by timestamp (especially
        #   when we don't trust the log-server to provide correct
        #   timestamps.
        return self.__read(tsdict)


    def read_async(self, recno):
        """
        Same as 'read', but aysnchoronous version. Returns events
        """

        __func = gdp.gdp_gin_read_async
        __func.argtypes = [POINTER(self.gdp_gin_t), gdp_recno_t,
                                c_void_p, c_void_p]
        __func.restype = EP_STAT

        estat = __func(self.ptr, gdp_recno_t(recno), None, self.__gen_udata())
        check_EP_STAT(estat)


    def append(self, datum_dict):
        """
        Write a datum to the GCL. The datum should be a dictionary, with
            the only valid key being 'data'. The value is the actual 
            data that is to be written
        """

        datum = GDP_DATUM()

        if "data" in datum_dict.keys():
            datum.setbuf(datum_dict["data"])

        __func = gdp.gdp_gin_append
        __func.argtypes = [
            POINTER(self.gdp_gin_t), POINTER(GDP_DATUM.gdp_datum_t)]
        __func.restype = EP_STAT

        estat = __func(self.ptr, datum.gdp_datum)
        check_EP_STAT(estat)


    def append_async(self, datum_dict):
        """
        Async version of append. A writer ought to check return status by
            invoking get_next_event, potentially in a different thread
        """
        datum = GDP_DATUM()

        if "data" in datum_dict.keys():
            datum.setbuf(datum_dict["data"])

        __func = gdp.gdp_gin_append_async
        __func.argtypes = [ POINTER(self.gdp_gin_t),
                            POINTER(GDP_DATUM.gdp_datum_t),
                            c_void_p, c_void_p ]
        __func.restype = EP_STAT

        estat = __func(self.ptr, datum.gdp_datum, None, self.__gen_udata())
        check_EP_STAT(estat)



    # XXX: Check if this works.
    # More details here: http://python.net/crew/theller/ctypes/tutorial.html#callback-functions
    # The first argument, I believe, is the return type, which I think is void*
    gdp_gin_sub_cbfunc_t = CFUNCTYPE(c_void_p, POINTER(gdp_gin_t),
                                     POINTER(GDP_DATUM.gdp_datum_t), c_void_p)

    def __subscribe(self, start, numrecs, timeout, cbfunc, cbarg):
        """
        This works somewhat similar to the subscribe in GDP C api.
        callback functions is experimental. Events are better for now.

        'start' could either be an 'int' (to represent record number),
            or a 'dict' (to represent a timestamp)
        """

        if isinstance(start, int):
            # casting start to ctypes
            __start = gdp_recno_t(start)

            __start_type = gdp_recno_t
            __func = gdp.gdp_gin_subscribe

        elif isinstance(start, dict):
            __start = GDP_DATUM.EP_TIME_SPEC()
            __start.tv_sec = c_int64(start['tv_sec'])
            __start.tv_nsec = c_uint32(start['tv_nsec'])
            __start.tv_accuracy = c_float(start['tv_accuracy'])

            __start_type = POINTER(GDP_DATUM.EP_TIME_SPEC)
            __func = gdp.gdp_gin_subscribe_ts

        else:   # should never reach here
            assert False

        # casting numrecs to ctypes
        __numrecs = c_int32(numrecs)

        # if timeout is None, then we just skip this
        if timeout == None:
            __timeout = None
        else:
            __timeout = GDP_DATUM.EP_TIME_SPEC()
            __timeout.tv_sec = c_int64(timeout['tv_sec'])
            __timeout.tv_nsec = c_uint32(timeout['tv_nsec'])
            __timeout.tv_accuracy = c_float(timeout['tv_accuracy'])

        # casting the python function to the callback function
        if cbfunc == None:
            __cbfunc = None
        else:
            __cbfunc = self.gdp_gin_sub_cbfunc_t(cbfunc)

        if cbfunc == None:
            __func.argtypes = [POINTER(self.gdp_gin_t), __start_type,
                                c_int32, POINTER(GDP_DATUM.EP_TIME_SPEC),
                                c_void_p, c_void_p]
        else:
            __func.argtypes = [POINTER(self.gdp_gin_t), __start_type,
                                c_int32, POINTER(GDP_DATUM.EP_TIME_SPEC),
                                self.gdp_gin_sub_cbfunc_t, c_void_p]

        __func.restype = EP_STAT

        estat = __func(
            self.ptr, __start, __numrecs, __timeout, __cbfunc, cbarg)
        check_EP_STAT(estat)
        return estat


    def subscribe(self, start, numrecs, timeout):
        """
        Subscriptions. Refer to the C-API for more details
        For now, callbacks are not exposed to end-user. Events are 
            generated instead.
        """
        return self.__subscribe(start, numrecs, timeout,
                                                    None, self.__gen_udata())


    def subscribe_ts(self, startdict, numrecs, timeout):
        """
        Same as subscribe, except that the starting point of subscription
            is a timestamp dictionary rather than a record number.
            (See also: 'read_ts')
        """
        return self.__subscribe(startdict, numrecs, timeout,
                                                    None, self.__gen_udata())

    def unsubscribe(self):
        """
        Terminate subscriptions (created by this current thread).
        """

        # FIXME: the behavior is unexpected, because of a potential
        # bug in the C implementation; it neither terminates the
        # specified subscription with non-null udata, nor terminates
        # 'all' subscriptions on null udata.
        # Fortunately, it does terminate the subscriptions by the 
        # current thread with null udata. It's a workaround that
        # *will* break soon.

        # return self.__unsubscribe(None, self.__get_udata())
        return self.__unsubscribe(None, None)


    def __unsubscribe(self, cbfunc, cbarg):
        """
        Terminate the subscription.
        """

        # casting the python function to the callback function
        if cbfunc == None:
            __cbfunc = None
        else:
            __cbfunc = self.gdp_gin_sub_cbfunc_t(cbfunc)

        __func = gdp.gdp_gin_unsubscribe
        if cbfunc is None:
            __func.argtypes = [POINTER(self.gdp_gin_t), c_void_p, c_void_p]
        else:
            __func.argtypes = [POINTER(self.gdp_gin_t),
                                self.gdp_gin_sub_cbfunc_t, c_void_p]
        __func.restype = EP_STAT

        estat = __func(self.ptr, __cbfunc, cbarg)
        check_EP_STAT(estat)
        return estat



    def __multiread(self, start, numrecs, cbfunc, cbarg):
        """
        similar to multiread in the GDP C API
        """

        if isinstance(start, int):
            # casting start to ctypes
            __start = gdp_recno_t(start)

            __start_type = gdp_recno_t
            __func = gdp.gdp_gin_multiread

        elif isinstance(start, dict):
            __start = GDP_DATUM.EP_TIME_SPEC()
            __start.tv_sec = c_int64(start['tv_sec'])
            __start.tv_nsec = c_uint32(start['tv_nsec'])
            __start.tv_accuracy = c_float(start['tv_accuracy'])

            __start_type = POINTER(GDP_DATUM.EP_TIME_SPEC)
            __func = gdp.gdp_gin_multiread_ts

        else:
            assert False

        # casting numrecs to ctypes
        __numrecs = c_int32(numrecs)

        # casting the python function to the callback function
        if cbfunc == None:
            __cbfunc = None
        else:
            __cbfunc = self.gdp_gin_sub_cbfunc_t(cbfunc)

        if cbfunc == None:
            __func.argtypes = [POINTER(self.gdp_gin_t), __start_type,
                                c_int32, c_void_p, c_void_p]
        else:
            __func.argtypes = [POINTER(self.gdp_gin_t), __start_type,
                                c_int32, self.gdp_gin_sub_cbfunc_t, c_void_p]
        __func.restype = EP_STAT

        estat = __func(self.ptr, __start, __numrecs, __cbfunc, cbarg)
        check_EP_STAT(estat)
        return estat


    def multiread(self, start, numrecs):
        """
        Multiread. Refer to the C-API for details.
        For now, callbacks are not exposed to end-user. Events are
            generated instead.
        """

        return self.__multiread(start, numrecs, None, self.__gen_udata())


    def multiread_ts(self, startdict, numrecs):
        """
        Same as multiread, except that the starting point of multiread
            is a timestamp dictionary rather than a record number.
            (See also: 'read_ts')
        """

        return self.__multiread(startdict, numrecs, None, self.__gen_udata())



    def print_to_file(self, fh, detail, indent):
        """
        Print this GDP object to a file. Could be sys.stdout
            The actual printing is done by the C library
        """

        __fh = PyFile_AsFile(fh)

        __func = gdp.gdp_gin_print
        __func.argtypes = [POINTER(self.gdp_gin_t), FILE_P, c_int, c_int]
        # ignore the return type

        __func(self.ptr, __fh, c_int(detail), c_int(indent))
        return

    def getname(self):
        "Get the name of this GCL, returns a GDP_NAME object"

        __func = gdp.gdp_gin_getname
        __func.argtypes = [POINTER(self.gdp_gin_t)]
        __func.restype = POINTER(GDP_NAME.name_t)

        gin_name_pointer = __func(self.ptr)
        gin_name = string_at(gin_name_pointer, 32)
        return GDP_NAME(gin_name)


    @classmethod
    def _helper_get_next_event(cls, __gin_handle, timeout, strict_threading):
        """ Get the events for GCL __gin_handle.  """

        if not strict_threading:
            ## this is the old library behavior, no checks on thread
            return cls._helper_get_next_event_call_clib(__gin_handle, timeout)

        ## Otherwise, do the threading check on Python side.
        ## use a mix of looking in local queue and calling the
        ## underlying C library.

        if timeout is None:
            # set an insanely high value for infinite timeout
            timeout = {'tv_sec':10**10, 'tv_nsec':0, 'tv_accuracy':0.0}

        timeout_s = timeout['tv_sec'] + (1.0*timeout['tv_nsec'])/(10**9)

        # 20 ms. seems like a good value for the moment.
        tiny_t = {'tv_sec':0, 'tv_nsec':20*(10**6), 'tv_accuracy':0.0}

        ev = None
        start_time = time.time()

        while ev is None and (time.time()-start_time)<timeout_s:
            ## phase 1: look in local queue
            ev = cls._helper_get_next_event_check_q(__gin_handle)

            if ev is not None:
                # print "############## From the queue"
                break

            ## phase 2: query from the C library (this blocks)
            ev = cls._helper_get_next_event_call_clib(__gin_handle, tiny_t)
            ## we insert it back to the library, and retrieve it again
            ## just to make sure nobody snuck something in the queue in the
            ## meantime
            if ev is not None:
                if ev["udata"] == cls.__get_udata():
                    # print ">>>>>>>>>>>>>> From the library"
                    pass

                with cls.ev_queues_lock:
                    evlist = cls.ev_queues.setdefault(ev["udata"], [])
                    evlist.append(ev)

            ## phase 3: look in local queue once again. If we fetched
            ## something from the library, it will be in the local queue
            ## this time, for sure.
            ev = cls._helper_get_next_event_check_q(__gin_handle)

        if ev is not None:
            assert ev["udata"] == cls.__get_udata()

        cls._helper_cleanup_event_q()

        return ev


    @classmethod
    def _helper_cleanup_event_q(cls):

        ## cleanup step (done separately):
        with cls.ev_queues_lock:
            for udata in cls.__old_udata:
                cls.ev_queues.pop(int(udata.value), None)


    @classmethod
    def _helper_get_next_event_check_q(cls, ginh):
        """
        Either extracts one event from the local queue that matches
        the requirements and returns that event, Or returns None if
        no event matches.
        """

        # pprint.pprint(cls.ev_queues)
        udata = cls.__get_udata()
        gin_handle = cls.object_dir.get(addressof(ginh.contents), None)()

        with cls.ev_queues_lock:
            evlist = cls.ev_queues.get(udata, [])
            if len(evlist)>0:
                ## sort the list, if it isn't for some reason
                evlist.sort(key=lambda ev:ev['datum']['recno'])
                if ginh is None or gin_handle == evlist[0]["gin_handle"]:
                    return evlist.pop(0)

        return None


    @classmethod
    def _helper_get_next_event_call_clib(cls, __gin_handle, timeout):
        """
        Get the events for GCL __gin_handle by calling the C library.
        If __gin_handle is None, then get events for any open GCL

        Returns at most one event.
        """

        __func1 = gdp.gdp_event_next

        # Find the 'type' we need to pass to argtypes
        if __gin_handle == None:
            __func1_arg1_type = c_void_p
        else:
            __func1_arg1_type = POINTER(cls.gdp_gin_t)

        # if timeout is None, then we just skip this
        if timeout == None:
            __timeout = None
            __func1_arg2_type = c_void_p
        else:
            __timeout = GDP_DATUM.EP_TIME_SPEC()
            __timeout.tv_sec = c_int64(timeout['tv_sec'])
            __timeout.tv_nsec = c_uint32(timeout['tv_nsec'])
            __timeout.tv_accuracy = c_float(timeout['tv_accuracy'])
            __func1_arg2_type = POINTER(GDP_DATUM.EP_TIME_SPEC)

        # Enable some type checking

        __func1.argtypes = [__func1_arg1_type, __func1_arg2_type]
        __func1.restype = POINTER(cls.gdp_event_t)

        event_ptr = __func1(__gin_handle, __timeout)
        if bool(event_ptr) == False:  # Null pointers have false boolean value
            return None

        # now get the associated GCL handle
        __func2 = gdp.gdp_event_getgin
        __func2.argtypes = [POINTER(cls.gdp_event_t)]
        __func2.restype = POINTER(cls.gdp_gin_t)

        gin_ptr = __func2(event_ptr)

        # now find this in the dictionary
        ## => need the '()' because we store weakrefs in object_dir
        gin_handle = cls.object_dir.get(addressof(gin_ptr.contents), None)()

        # also get the associated datum object
        __func3 = gdp.gdp_event_getdatum
        __func3.argtypes = [POINTER(cls.gdp_event_t)]
        __func3.restype = POINTER(GDP_DATUM.gdp_datum_t)

        datum_ptr = __func3(event_ptr)
        datum = GDP_DATUM(ptr=datum_ptr)
        datum_dict = {}
        datum_dict["recno"] = datum.getrecno()
        datum_dict["ts"] = datum.getts()
        datum_dict["data"] = datum.getbuf()
        datum_dict["sig"] = datum.getsig()
        datum_dict["sigalg"] = datum.getsigmdalg()

        # find the type of the event
        __func4 = gdp.gdp_event_gettype
        __func4.argtypes = [POINTER(cls.gdp_event_t)]
        __func4.restype = c_int

        event_type = __func4(event_ptr)

        # get the status code
        __func5 = gdp.gdp_event_getstat
        __func5.argtypes = [POINTER(cls.gdp_event_t)]
        __func5.restype = EP_STAT

        event_ep_stat = __func5(event_ptr)
        check_EP_STAT(event_ep_stat)

        # get user data
        __func6 = gdp.gdp_event_getudata
        __func6.argtypes = [POINTER(cls.gdp_event_t)]
        __func6.restype = c_void_p

        _udata = __func6(event_ptr)
        udata = int(cast(_udata, c_char_p).value)

        # also free the event
        __func7 = gdp.gdp_event_free
        __func7.argtypes = [POINTER(cls.gdp_event_t)]
        __func7.restype = EP_STAT

        estat = __func7(event_ptr)
        check_EP_STAT(estat)

        gdp_event = {}
        gdp_event["gin_handle"] = gin_handle
        gdp_event["datum"] = datum_dict
        gdp_event["type"] = event_type
        gdp_event["stat"] = event_ep_stat
        gdp_event["udata"] = udata

        return gdp_event

    @classmethod
    def get_next_event(cls, timeout, strict_threading=False):
        """ Get events for ANY open gin """
        return cls._helper_get_next_event(None, timeout, strict_threading)

    def __get_next_event(self, timeout, strict_threading=False):
        """ Get events for this particular GCL """
        event = self._helper_get_next_event(self.ptr, timeout, strict_threading)
        if event is not None:
            ## the crazy '__repr__.__self__' is needed, because there's no
            ## unproxy. See https://stackoverflow.com/questions/10246116
            assert event["gin_handle"] == self.__repr__.__self__
        return event

    @classmethod
    def __gen_udata(cls):
        """
        returns the current thread id as a c_void_p, that can be passed
        as user data to asynchronous calls.
        """
        tid = cls.__tid()
        str_buf = create_string_buffer(str(random.randint(0, 2**32)))

        ## Keep this, such that this does not garbage collected
        old_udata = cls.tid_udata_map.get(tid, None)
        if old_udata is not None:
            cls.__old_udata.append(old_udata)

        ## update new udata
        cls.tid_udata_map[tid] = str_buf
        udata = cast(str_buf, c_void_p).value
        return udata

    @classmethod
    def __get_udata(cls):
        """ returns the current thread id, just as an integer """
        tid = cls.__tid()
        assert tid in cls.tid_udata_map
        return int(cls.tid_udata_map[tid].value)

    @staticmethod
    def __tid():
        return threading.current_thread().ident
