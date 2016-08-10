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


from MISC import *
from GDP_NAME import *
from GDP_DATUM import *
from GDP_GCLMD import *
from GDP_GCL_OPEN_INFO import *


class GDP_GCL:

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
    #   event for a gcl handle?

    # C pointer to python object mapping
    object_dir = {}

    class gdp_gcl_t(Structure):

        "Corresponds to gdp_gcl_t structure exported by C library"
        pass

    class gdp_event_t(Structure):

        "Corresponds to gdp_event_t structure exported by C library"
        pass


    def __init__(self, name, iomode, open_info={}):
        """
        Open a GCL with given name and io-mode
        Creating new GCL's is no longer supported.

        name=<name-of-gcl>, iomode=<mode>
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

        # self.ptr is just a C style pointer, that we will assign to something
        self.ptr = POINTER(self.gdp_gcl_t)()

        # we do need an internal represenation of the name.
        gcl_name_python = name.internal_name()
        # convert this to a string that ctypes understands. Some ctypes magic
        # ahead
        buf = create_string_buffer(gcl_name_python, 32+1)
        gcl_name_ctypes_ptr = cast(byref(buf), POINTER(GDP_NAME.name_t))
        gcl_name_ctypes = gcl_name_ctypes_ptr.contents

        # (optional) Do a quick sanity checking on open_info 
        #   use it to get a GDP_GCL_OPEN_INFO structure
        self.gdp_gcl_open_info = GDP_GCL_OPEN_INFO(open_info)

        # open an existing gcl
        __func = gdp.gdp_gcl_open
        __func.argtypes = [GDP_NAME.name_t, c_int,
                           POINTER(GDP_GCL_OPEN_INFO.gdp_gcl_open_info_t),
                           POINTER(POINTER(self.gdp_gcl_t))]
        __func.restype = EP_STAT

        estat = __func(gcl_name_ctypes, iomode,
                            self.gdp_gcl_open_info.gdp_gcl_open_info_ptr,
                            pointer(self.ptr))
        check_EP_STAT(estat)

        # Also add itself to the global list of objects
        # XXX: See if there is a cleaner way of dealing with this?
        self.object_dir[addressof(self.ptr.contents)] = self

        # A hack to make sure we can make get_next_event both a class method
        #   to listen for ALL events, as well as an instance method to listen
        #   to this particular instance's events
        self.get_next_event = self.__get_next_event

    def __del__(self):
        "close this GCL handle, and free the allocated resources"

        if bool(self.ptr) == False:   # null pointers have a false boolean value
            # Null pointer => no memory allocated => nothing to clean up
            #   this happens if __init__ did not finish
            return

        # remove the entry from object directory
        self.object_dir.pop(addressof(self.ptr.contents), None)

        # call the C function to free associated C memory block
        __func = gdp.gdp_gcl_close
        __func.argtypes = [POINTER(self.gdp_gcl_t)]
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
        gcl_name_python = name.internal_name()
        logd_name_python = logd_name.internal_name()

        # convert this to a string that ctypes understands. Some ctypes magic
        # ahead
        buf1 = create_string_buffer(gcl_name_python, 32+1)
        gcl_name_ctypes_ptr = cast(byref(buf1), POINTER(GDP_NAME.name_t))
        gcl_name_ctypes = gcl_name_ctypes_ptr.contents

        buf2 = create_string_buffer(logd_name_python, 32+1)
        logd_name_ctypes_ptr = cast(byref(buf2), POINTER(GDP_NAME.name_t))
        logd_name_ctypes = logd_name_ctypes_ptr.contents

        throwaway_ptr = POINTER(cls.gdp_gcl_t)()

        md = GDP_GCLMD()
        for k in metadata:
            md.add(k, metadata[k])

        __func = gdp.gdp_gcl_create
        __func.argtypes = [GDP_NAME.name_t, GDP_NAME.name_t,
                                POINTER(GDP_GCLMD.gdp_gclmd_t), 
                                POINTER(POINTER(cls.gdp_gcl_t))]
        __func.restype = EP_STAT

        estat = __func(gcl_name_ctypes, logd_name_ctypes,
                            md.gdp_gclmd_ptr, throwaway_ptr)
        check_EP_STAT(estat)
        return

    def getmetadata(self):
        """
        return the metadata included at creation time.
            Represented as a python dict
        """

        __func = gdp.gdp_gcl_getmetadata
        __func.argtypes = [POINTER(self.gdp_gcl_t),
                                POINTER(POINTER(GDP_GCLMD.gdp_gclmd_t))]
        __func.restype = EP_STAT

        gmd = POINTER(GDP_GCLMD.gdp_gclmd_t)()
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

            __func = gdp.gdp_gcl_read
            __func.argtypes = [POINTER(self.gdp_gcl_t), gdp_recno_t,
                                    POINTER(GDP_DATUM.gdp_datum_t)]
            __func.restype = EP_STAT

        elif isinstance(query_param, dict):

            __query_param = GDP_DATUM.EP_TIME_SPEC()
            __query_param.tv_sec = c_int64(query_param['tv_sec'])
            __query_param.tv_nsec = c_uint32(query_param['tv_nsec'])
            __query_param.tv_accuracy = c_float(query_param['tv_accuracy'])

            __func = gdp.gdp_gcl_read_ts
            __func.argtypes = [POINTER(self.gdp_gcl_t),
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


    def append(self, datum_dict):
        """
        Write a datum to the GCL. The datum should be a dictionary, with
            the only valid key being 'data'. The value is the actual 
            data that is to be written
        """

        datum = GDP_DATUM()

        if "data" in datum_dict.keys():
            datum.setbuf(datum_dict["data"])

        __func = gdp.gdp_gcl_append
        __func.argtypes = [
            POINTER(self.gdp_gcl_t), POINTER(GDP_DATUM.gdp_datum_t)]
        __func.restype = EP_STAT

        estat = __func(self.ptr, datum.gdp_datum)
        check_EP_STAT(estat)

        return

    def append_async(self, datum_dict):
        """
        Async version of append. A writer ought to check return status by
            invoking get_next_event, potentially in a different thread
        """
        datum = GDP_DATUM()

        if "data" in datum_dict.keys():
            datum.setbuf(datum_dict["data"])

        __func = gdp.gdp_gcl_append_async
        __func.argtypes = [ POINTER(self.gdp_gcl_t), 
                            POINTER(GDP_DATUM.gdp_datum_t),
                            c_void_p, c_void_p ]
        __func.restype = EP_STAT

        estat = __func(self.ptr, datum.gdp_datum, None, None)
        check_EP_STAT(estat)



    # XXX: Check if this works.
    # More details here: http://python.net/crew/theller/ctypes/tutorial.html#callback-functions
    # The first argument, I believe, is the return type, which I think is void*
    gdp_gcl_sub_cbfunc_t = CFUNCTYPE(c_void_p, POINTER(gdp_gcl_t),
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
            __func = gdp.gdp_gcl_subscribe

        elif isinstance(start, dict):
            __start = GDP_DATUM.EP_TIME_SPEC()
            __start.tv_sec = c_int64(start['tv_sec'])
            __start.tv_nsec = c_uint32(start['tv_nsec'])
            __start.tv_accuracy = c_float(start['tv_accuracy'])

            __start_type = POINTER(GDP_DATUM.EP_TIME_SPEC)
            __func = gdp.gdp_gcl_subscribe_ts

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
            __cbfunc = self.gdp_gcl_sub_cbfunc_t(cbfunc)

        if cbfunc == None:
            __func.argtypes = [POINTER(self.gdp_gcl_t), __start_type,
                                c_int32, POINTER(GDP_DATUM.EP_TIME_SPEC),
                                c_void_p, c_void_p]
        else:
            __func.argtypes = [POINTER(self.gdp_gcl_t), __start_type,
                                c_int32, POINTER(GDP_DATUM.EP_TIME_SPEC),
                                self.gdp_gcl_sub_cbfunc_t, c_void_p]

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
        return self.__subscribe(start, numrecs, timeout, None, None)


    def subscribe_ts(self, startdict, numrecs, timeout):
        """
        Same as subscribe, except that the starting point of subscription
            is a timestamp dictionary rather than a record number.
            (See also: 'read_ts')
        """
        return self.__subscribe(startdict, numrecs, timeout, None, None)


    def __multiread(self, start, numrecs, cbfunc, cbarg):
        """
        similar to multiread in the GDP C API
        """

        if isinstance(start, int):
            # casting start to ctypes
            __start = gdp_recno_t(start)

            __start_type = gdp_recno_t
            __func = gdp.gdp_gcl_multiread

        elif isinstance(start, dict):
            __start = GDP_DATUM.EP_TIME_SPEC()
            __start.tv_sec = c_int64(start['tv_sec'])
            __start.tv_nsec = c_uint32(start['tv_nsec'])
            __start.tv_accuracy = c_float(start['tv_accuracy'])

            __start_type = POINTER(GDP_DATUM.EP_TIME_SPEC)
            __func = gdp.gdp_gcl_multiread_ts

        else:
            assert False

        # casting numrecs to ctypes
        __numrecs = c_int32(numrecs)

        # casting the python function to the callback function
        if cbfunc == None:
            __cbfunc = None
        else:
            __cbfunc = self.gdp_gcl_sub_cbfunc_t(cbfunc)

        if cbfunc == None:
            __func.argtypes = [POINTER(self.gdp_gcl_t), __start_type,
                                c_int32, c_void_p, c_void_p]
        else:
            __func.argtypes = [POINTER(self.gdp_gcl_t), __start_type,
                                c_int32, self.gdp_gcl_sub_cbfunc_t, c_void_p]
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

        return self.__multiread(start, numrecs, None, None)


    def multiread_ts(self, startdict, numrecs):
        """
        Same as multiread, except that the starting point of multiread
            is a timestamp dictionary rather than a record number.
            (See also: 'read_ts')
        """

        return self.__multiread(startdict, numrecs, None, None)



    def print_to_file(self, fh, detail, indent):
        """
        Print this GDP object to a file. Could be sys.stdout
            The actual printing is done by the C library
        """

        __fh = PyFile_AsFile(fh)

        __func = gdp.gdp_gcl_print
        __func.argtypes = [POINTER(self.gdp_gcl_t), FILE_P, c_int, c_int]
        # ignore the return type

        __func(self.ptr, __fh, c_int(detail), c_int(indent))
        return

    def getname(self):
        "Get the name of this GCL, returns a GDP_NAME object"

        __func = gdp.gdp_gcl_getname
        __func.argtypes = [POINTER(self.gdp_gcl_t)]
        __func.restype = POINTER(GDP_NAME.name_t)

        gcl_name_pointer = __func(self.ptr)
        gcl_name = string_at(gcl_name_pointer, 32)
        return GDP_NAME(gcl_name)

    @classmethod
    def _helper_get_next_event(cls, __gcl_handle, timeout):
        """
        Get the events for GCL __gcl_handle. If __gcl_handle is None, then get
            events for any open GCL
        """

        __func1 = gdp.gdp_event_next

        # Find the 'type' we need to pass to argtypes
        if __gcl_handle == None:
            __func1_arg1_type = c_void_p
        else:
            __func1_arg1_type = POINTER(cls.gdp_gcl_t)

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

        event_ptr = __func1(__gcl_handle, __timeout)
        if bool(event_ptr) == False:  # Null pointers have false boolean value
            return None

        # now get the associated GCL handle
        __func2 = gdp.gdp_event_getgcl
        __func2.argtypes = [POINTER(cls.gdp_event_t)]
        __func2.restype = POINTER(cls.gdp_gcl_t)

        gcl_ptr = __func2(event_ptr)

        # now find this in the dictionary
        gcl_handle = cls.object_dir.get(addressof(gcl_ptr.contents), None)

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

        # also free the event
        __func6 = gdp.gdp_event_free
        __func6.argtypes = [POINTER(cls.gdp_event_t)]
        __func6.restype = EP_STAT

        estat = __func6(event_ptr)
        check_EP_STAT(estat)

        gdp_event = {}
        gdp_event["gcl_handle"] = gcl_handle
        gdp_event["datum"] = datum_dict
        gdp_event["type"] = event_type
        gdp_event["stat"] = event_ep_stat

        return gdp_event

    @classmethod
    def get_next_event(cls, timeout):
        """ Get events for ANY open gcl """
        return cls._helper_get_next_event(None, timeout)

    def __get_next_event(self, timeout):
        """ Get events for this particular GCL """
        event = self._helper_get_next_event(self.ptr, timeout)
        if event is not None: assert event["gcl_handle"] == self
        return event
