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


class GDP_GCLMD:

    """
    GCL Metadata equivalent -- for internal use only.
    The C equivalent exposed to python users is of a dictionary.

    """

    class gdp_md_t(Structure):
        pass


    def __init__(self, **kwargs):
        """
        Constructor: calls the C-function to allocate memory.
        """

        if len(kwargs)==0:
            __func = gdp.gdp_md_new
            __func.argtypes = [c_int]
            __func.restype = POINTER(self.gdp_md_t)

            self.gdp_md_ptr = __func(c_int(0))
            self.did_i_create_it = True

        else:
            if "ptr" in kwargs:
                self.gdp_md_ptr = kwargs["ptr"]
                self.did_i_create_it = False
            else:
                raise Exception


    def __del__(self):
        """
        Destructor: Frees the C structure
        """

        if self.did_i_create_it:
            __func = gdp.gdp_md_free
            __func.argtypes = [POINTER(self.gdp_md_t)]

            __func(self.gdp_md_ptr)

        return


    def print_to_file(self, fh, detail):
        """
        Print the gdp_md C memory location contents to a file handle fh.
            fh could be sys.stdout, or any other open file handle.
        Note: This just calls the corresponding C library function which
              handles the printing
        """

        # need to convert this file handle to a C FILE*
        __fh = PyFile_AsFile(fh)
        __func = gdp.gdp_md_print
        __func.argtypes = [POINTER(self.gdp_md_t), FILE_P, c_int]
        # ignore the return value

        __func(self.gdp_md_ptr, __fh, detail)


    def add(self, md_id, data):
        """
        Add a new entry to the metadata set
        """

        __func = gdp.gdp_md_add
        __func.argtypes = [POINTER(self.gdp_md_t), c_uint32,
                                c_size_t, c_void_p]
        __func.restype = EP_STAT

        size = c_size_t(len(data))
        tmp_buf = create_string_buffer(data, len(data))        
        
        estat = __func(self.gdp_md_ptr, c_uint32(md_id),
                                size, byref(tmp_buf))
        check_EP_STAT(estat)

        return


    def get(self, index):
        """
        Get a new entry from the metadata set by index
        """

        __func = gdp.gdp_md_get
        __func.argtypes = [POINTER(self.gdp_md_t), c_int, POINTER(c_uint32),
                                POINTER(c_size_t), POINTER(c_void_p)]
        __func.restype = EP_STAT

        md_id = c_uint32()
        dlen = c_size_t()
        data_ptr = c_void_p()
        _index = c_int(index)

        estat = __func(self.gdp_md_ptr, _index, byref(md_id),
                                byref(dlen), byref(data_ptr))
        check_EP_STAT(estat)

        data = string_at(data_ptr, dlen.value)

        return (md_id.value, data)


    def find(self, md_id):
        """
        Get a new entry from the metadata set by id
        """

        __func = gdp.gdp_md_find
        __func.argtypes = [POINTER(self.gdp_md_t), c_uint32,
                                POINTER(c_size_t), POINTER(c_void_p)]
        __func.restype = EP_STAT

        dlen = c_size_t()
        data_ptr = c_void_p()

        estat = __func(self.gdp_md_ptr, c_uint32(md_id),
                                byref(dlen), byref(data_ptr))
        check_EP_STAT(estat)

        data = string_at(data_ptr, dlen.value)

        return data
