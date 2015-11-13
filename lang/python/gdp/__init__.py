#!/usr/bin/env python

"""
A Python API for libgdp. Uses ctypes and shared library versions of
libgdp and libep.

This package exports two classes, and a few utility functions.
- GDP_NAME:     represents names in GDP
- GDP_GCL :     represents a GCL file handle

The first thing that you will need to do is call `gdp_init`. `gdp_init` 
sets up the connection to a remote `gdp_router`. Example:

```
gdp_init("127.0.0.1", 8007)
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

from MISC import GDP_MODE_ANY, GDP_MODE_RO, GDP_MODE_AO, GDP_MODE_RA, \
    GDP_EVENT_DATA, GDP_EVENT_EOS, GDP_EVENT_SHUTDOWN, GDP_EVENT_ASTAT, \
    GDP_GCLMD_XID, GDP_GCLMD_PUBKEY, GDP_GCLMD_CTIME, GDP_GCLMD_CID, \
    gdp_init, gdp_run_accept_event_loop, dbg_set
from GDP_NAME import GDP_NAME
from GDP_GCL import GDP_GCL
from EP_CRYPTO import EP_CRYPTO_KEY, \
    EP_CRYPTO_KEYFORM_UNKNOWN, EP_CRYPTO_KEYFORM_PEM, EP_CRYPTO_KEYFORM_DER, \
    EP_CRYPTO_F_PUBLIC, EP_CRYPTO_F_SECRET    

__all__ = [GDP_MODE_ANY, GDP_MODE_RO, GDP_MODE_AO, GDP_MODE_RA,
           GDP_EVENT_DATA, GDP_EVENT_EOS, GDP_EVENT_SHUTDOWN, GDP_EVENT_ASTAT,
           GDP_GCLMD_XID, GDP_GCLMD_PUBKEY, GDP_GCLMD_CTIME, GDP_GCLMD_CID,
           gdp_init, gdp_run_accept_event_loop, dbg_set, GDP_NAME, GDP_GCL,
           EP_CRYPTO_KEY,
           EP_CRYPTO_KEYFORM_UNKNOWN, EP_CRYPTO_KEYFORM_PEM,
           EP_CRYPTO_KEYFORM_DER,
           EP_CRYPTO_F_PUBLIC, EP_CRYPTO_F_SECRET ]
