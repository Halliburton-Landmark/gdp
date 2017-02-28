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


"""
A Python API for libgdp. Uses ctypes and shared library versions of
libgdp and libep.

This package exports two classes, and a few utility functions.
- GDP_NAME:     represents names in GDP
- GDP_GCL :     represents a GCL file handle

The first thing that you will need to do is call `gdp_init`. `gdp_init` 
sets up the connection to a remote `gdp_router`. Example:

```
gdp_init("127.0.0.1:8007")
```

Next, you would like to have a `GDP_NAME` object that represents the 
names in GDP. The reason for a `GDP_NAME` object is that each name can
have multiple representations.

To create a `GDP_NAME`, use the following:
```
gcl_name = GDP_NAME(some_name)
```
`some_name` could be either a human readable name that is then hashed
using SHA256, or it could be an internal name represented as a python
binary string, or it could also be a printable GDP name. (For more
details on this, look at the C API documentation)

Once you have the GDP_NAME, you can use it to open `GDP_GCL` objects,
which behave like a file handle in some certain sense.

The unit of reading/writing from a `GDP_GCL` is a datum, which is a
python dictionary. `GDP_GCL` supports append, reading by record number,
or subscriptions. For more details, look at the sample programs
provided.

"""
from __future__ import absolute_import

from .MISC import GDP_MODE_ANY, GDP_MODE_RO, GDP_MODE_AO, GDP_MODE_RA, \
    GDP_EVENT_DATA, GDP_EVENT_EOS, GDP_EVENT_SHUTDOWN, \
    GDP_EVENT_CREATED, GDP_EVENT_SUCCESS, GDP_EVENT_FAILURE, \
    GDP_GCLMD_XID, GDP_GCLMD_PUBKEY, GDP_GCLMD_CTIME, GDP_GCLMD_CID, \
    gdp_init, gdp_run_accept_event_loop, dbg_set, check_EP_STAT, ep_stat_tostr
from .GDP_NAME import GDP_NAME
from .GDP_GCL import GDP_GCL
from .EP_CRYPTO import EP_CRYPTO_KEY, \
    EP_CRYPTO_KEYFORM_UNKNOWN, EP_CRYPTO_KEYFORM_PEM, EP_CRYPTO_KEYFORM_DER, \
    EP_CRYPTO_F_PUBLIC, EP_CRYPTO_F_SECRET    

__all__ = [GDP_MODE_ANY, GDP_MODE_RO, GDP_MODE_AO, GDP_MODE_RA,
           GDP_EVENT_DATA, GDP_EVENT_EOS, GDP_EVENT_SHUTDOWN,
           GDP_EVENT_CREATED, GDP_EVENT_SUCCESS, GDP_EVENT_FAILURE,
           GDP_GCLMD_XID, GDP_GCLMD_PUBKEY, GDP_GCLMD_CTIME, GDP_GCLMD_CID,
           gdp_init, gdp_run_accept_event_loop, dbg_set,
           check_EP_STAT, ep_stat_tostr,
           GDP_NAME, GDP_GCL,
           EP_CRYPTO_KEY,
           EP_CRYPTO_KEYFORM_UNKNOWN, EP_CRYPTO_KEYFORM_PEM,
           EP_CRYPTO_KEYFORM_DER,
           EP_CRYPTO_F_PUBLIC, EP_CRYPTO_F_SECRET ]
