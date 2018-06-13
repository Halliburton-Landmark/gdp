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


class GDP_SIG(object):
    """
    A class only for internal use. It is only a partially
    implemented interface for the moment.
    """

    class gdp_sig_t(Structure):
        pass

    def __init__(self, alg):
        """ Creates a new sig structure by calling C functions """
        __func = gdp.gdp_sig_new
        __func.argtypes = [c_int]
        __func.restype = POINTER(self.gdp_sig_t)
        self.sig_ = __func(c_int(alg))


    def __del__(self):
        """Free the memory"""
        __func = gdp.gdp_sig_free
        __func.argtypes = [POINTER(self.gdp_sig_t)]
        __func(self.sig_)


    def reset(self):
        """Reset the sig structure"""
        __func = gdp.gdp_sig_reset
        __func.argtypes = [POINTER(self.gdp_sig_t)]
        __func.restype = c_int

        ret = __func(self.sig_)
        return int(ret)


    def getlength(self):
        "Returns the length of the data associated with this sig structure"

        __func = gdp.gdp_sig_getlength
        __func.argtypes = [POINTER(self.gdp_sig_t)]
        __func.restype = c_size_t

        ret = __func(self.sig_)
        return ret


    def set(self, sigbytes):
        "write sigbytes to sig structure"

        __func = gdp.gdp_sig_set
        __func.argtypes = [POINTER(self.gdp_sig_t), c_void_p, c_size_t]

        size = c_size_t(len(sigbytes))
        tmp_buf = create_string_buffer(sigbytes, len(sigbytes))
        written_bytes = __func(self.sig_, byref(tmp_buf), size)


    def get(self):
        "Read data from sig structure"
        ## This returns the actual data (and not just a pointer).
        ## Pointers aren't very useful as it is in Python.

        __func = gdp.gdp_sig_getptr
        __func.argtypes = [POINTER(self.gdp_sig_t), POINTER(c_size_t)]
        __func.restype = c_void_p

        siglen = c_size_t()
        sigptr = __func(self.sig_, byref(siglen))
        sigbytes = string_at(sigptr, siglen.value)

        return sigbytes
