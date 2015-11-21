/* vim: set ai sw=8 sts=8 ts=8 :*/

/***********************************************************************
**  ----- BEGIN LICENSE BLOCK -----
**	LIBEP: Enhanced Portability Library (Reduced Edition)
**
**	Copyright (c) 2008-2015, Eric P. Allman.  All rights reserved.
**	Copyright (c) 2015, Regents of the University of California.
**
**	Redistribution and use in source and binary forms, with or without
**	modification, are permitted provided that the following conditions
**	are met:
**
**	1. Redistributions of source code must retain the above copyright
**	notice, this list of conditions and the following disclaimer.
**
**	2. Redistributions in binary form must reproduce the above copyright
**	notice, this list of conditions and the following disclaimer in the
**	documentation and/or other materials provided with the distribution.
**
**	3. Neither the name of the copyright holder nor the names of its
**	contributors may be used to endorse or promote products derived
**	from this software without specific prior written permission.
**
**	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
**	"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
**	LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
**	FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
**	COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
**	INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
**	BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
**	LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
**	CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
**	LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
**	ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
**	POSSIBILITY OF SUCH DAMAGE.
**  ----- END LICENSE BLOCK -----
***********************************************************************/

#ifndef _EP_CONF_H_
# define _EP_CONF_H_

/*
**  Local configuration.  There will be one of these for any platform
**  that needs tweaks.
**
**  Possible options:
**	EP_OSCF_HAS_INTTYPES_H
**		Set if <inttypes.h> exists
**	EP_OSCF_HAS_STDBOOL_H
**		Set if <stdbool.h> exists
**	EP_OSCF_HAS_STDINT_H
**		Set if <stdint.h> exists
**	EP_OSCF_HAS_STDLIB_H
**		Set if <stdlib.h> exists
**	EP_OSCF_HAS_STRING_H
**		Set if <string.h> exists
**	EP_OSCF_HAS_SYS_TYPES_H
**		Set if <sys/types.h> exists
**	EP_OSCF_HAS_UNISTD_H
**		Set if <unistd.h> exists
**	EP_OSCF_HAS_UCHAR_T
**		Set if uchar_t is defined
**	EP_OSCF_HAS_SYS_CDEFS_H
**		Set if <sys/cdefs.h> exists
**	EP_OSCF_HAS_STRLCPY
**		Set if strlcpy(3) is available
**	EP_OSCF_HAS_GETPROGNAME
**		Set if getprogname(3) is available
**	EP_OSCF_MEM_PAGESIZE
**		Memory page size; defaults to getpagesize()
**	EP_OSCF_MEM_ALIGNMENT
**		Best generic memory alignment; defaults to 4
**	EP_OSCF_SYSTEM_MALLOC
**		System memory allocation routine; defaults to "malloc"
**	EP_OSCF_SYSTEM_VALLOC
**		System aligned memory allocation routine; defaults to "malloc"
**	EP_OSCF_SYSTEM_REALLOC
**		System memory reallocation routine; defaults to "realloc"
**	EP_OSCF_SYSTEM_MFREE
**		System memory free routine; defaults to "free"
**	EP_OSCF_USE_PTHREADS
**		Compile in pthreads support
**
**  Configuration is probably better done using autoconf
**
**	This is the fallback version (if none other found)
**	(Actually, currently hacked to work on my development platforms)
*/

# ifndef EP_OSCF_USE_PTHREADS
#  define EP_OSCF_USE_PTHREADS		1
# endif

// these should be defined on all POSIX platforms
# define EP_OSCF_HAS_INTTYPES_H		1	// does <inttypes.h> exist?
# define EP_OSCF_HAS_STDBOOL_H		1	// does <stdbool.h> exist?
# define EP_OSCF_HAS_STDINT_H		1	// does <stdint.h> exist?
# define EP_OSCF_HAS_STDLIB_H		1	// does <stdlib.h> exist?
# define EP_OSCF_HAS_STRING_H		1	// does <string.h> exist?
# define EP_OSCF_HAS_SYS_TYPES_H	1	// does <sys/types.h> exist?
# define EP_OSCF_HAS_UNISTD_H		1	// does <unistd.h> exist?

# ifdef __FreeBSD__
#  define EP_OSCF_HAS_UCHAR_T		0	// does uchar_t exist?
#  define EP_OSCF_HAS_SYS_CDEFS_H	1	// does <sys/cdefs.h> exist?
#  define EP_OSCF_HAS_STRLCPY		1	// does strlcat(3) exist?
#  define EP_OSCF_HAS_LSTAT		1	// does lstat(2) exist?
#  if __FreeBSD_version >= 440000
#   define EP_OSCF_HAS_GETPROGNAME	1	// does getprogname(3) exist?
#  endif
# endif // __FreeBSD__

# ifdef __APPLE__
#  define EP_OSCF_HAS_UCHAR_T		0	// does uchar_t exist?
#  define EP_OSCF_HAS_SYS_CDEFS_H	1	// does <sys/cdefs.h> exist?
#  define EP_OSCF_HAS_STRLCPY		1	// does strlcat exist?
#  define EP_OSCF_HAS_LSTAT		1	// does lstat(2) exist?
#  define EP_OSCF_HAS_GETPROGNAME	1	// does getprogname(3) exist?
#  define EP_OSCF_SYSTEM_VALLOC		valloc	// aligned memory allocator
# endif // __APPLE__

#ifdef __linux__
# define EP_TYPE_PRINTFLIKE(a, b)
# define EP_OSCF_HAS_STRLCPY		0	// no strlcpy on linux
#endif

// this is a heuristic, but I don't know if there's a way to do it right
# if defined(__amd64) || defined(__x86_64)
#  define EP_OSCF_64BITPTR		1
# else
#  define EP_OSCF_64BITPTR		0
# endif

#ifndef EP_TYPE_PRINTFLIKE
# define EP_TYPE_PRINTFLIKE(a, b)	__printflike(a, b)
#endif

#endif // _EP_CONF_H_
