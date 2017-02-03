/* vim: set ai sw=8 sts=8 ts=8 :*/

/***********************************************************************
**  ----- BEGIN LICENSE BLOCK -----
**	LIBEP: Enhanced Portability Library (Reduced Edition)
**
**	Copyright (c) 2008-2017, Eric P. Allman.  All rights reserved.
**	Copyright (c) 2015-2017, Regents of the University of California.
**	All rights reserved.
**
**	Permission is hereby granted, without written agreement and without
**	license or royalty fees, to use, copy, modify, and distribute this
**	software and its documentation for any purpose, provided that the above
**	copyright notice and the following two paragraphs appear in all copies
**	of this software.
**
**	IN NO EVENT SHALL REGENTS BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT,
**	SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOST
**	PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION,
**	EVEN IF REGENTS HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**
**	REGENTS SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT
**	LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
**	FOR A PARTICULAR PURPOSE. THE SOFTWARE AND ACCOMPANYING DOCUMENTATION,
**	IF ANY, PROVIDED HEREUNDER IS PROVIDED "AS IS". REGENTS HAS NO
**	OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS,
**	OR MODIFICATIONS.
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
**	EP_OSCF_HAS_STRPTIME
**		Use strptime(3) for time/date parsing
**	EP_OSCF_USE_GETDATE
**		Use getdate(3) for time/date parsing
**	EP_OSCF_HAS_SD_NOTIFY
**		Use sd_notify(3) for system notification
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
# define EP_OSCF_HAS_STRPTIME		1	// does strptime(3) exist?

# ifdef __FreeBSD__
#  include <sys/param.h>
#  define EP_OSCF_HAS_UCHAR_T		0	// does uchar_t exist?
#  define EP_OSCF_HAS_SYS_CDEFS_H	1	// does <sys/cdefs.h> exist?
#  define EP_OSCF_HAS_STRLCPY		1	// does strlcat(3) exist?
#  define EP_OSCF_HAS_LSTAT		1	// does lstat(2) exist?
#  if __FreeBSD_version >= 440000
#   define EP_OSCF_HAS_GETPROGNAME	1	// does getprogname(3) exist?
#  endif
#  define EP_OSCF_NEED_OPTRESET		1	// optreset needed in getopt(3)
# endif // __FreeBSD__

# ifdef __APPLE__
#  define EP_OSCF_HAS_UCHAR_T		0	// does uchar_t exist?
#  define EP_OSCF_HAS_SYS_CDEFS_H	1	// does <sys/cdefs.h> exist?
#  define EP_OSCF_HAS_STRLCPY		1	// does strlcat exist?
#  define EP_OSCF_HAS_LSTAT		1	// does lstat(2) exist?
#  define EP_OSCF_HAS_GETPROGNAME	1	// does getprogname(3) exist?
#  define EP_OSCF_SYSTEM_VALLOC		valloc	// aligned memory allocator
#  ifndef EP_OSCF_USE_GETDATE
#   define EP_OSCF_USE_GETDATE		1	// does getdate(3) exist?
#  endif
#  define EP_OSCF_NEED_OPTRESET		1	// optreset needed in getopt(3)
# endif // __APPLE__

#ifdef __linux__
# define EP_TYPE_PRINTFLIKE(a, b)
# define EP_OSCF_HAS_STRLCPY		0	// no strlcpy on linux
# ifndef EP_OSCF_USE_GETDATE
#  if _XOPEN_SOURCE >= 500 || _XOPEN_SOURCE && _XOPEN_SOURCE_EXTENDED
#   define EP_OSCF_USE_GETDATE		1	// does getdate(3) exist?
#   if _GNU_SOURCE
#    define EP_OSCF_HAS_GETDATE_R	1	// getdate_r is available
#   endif
#  endif
# endif

# define _BSD_SOURCE			1	// needed to compile on Linux
# define _POSIX_C_SOURCE		200809L	// specify a modern environment
#endif

// this is a heuristic, but I don't know if there's a way to do it right
# if defined(__amd64) || defined(__x86_64)
#  define EP_OSCF_64BITPTR		1
# else
#  define EP_OSCF_64BITPTR		0
# endif

// defaults if not set above
#ifndef EP_TYPE_PRINTFLIKE
# define EP_TYPE_PRINTFLIKE(a, b)	__printflike(a, b)
#endif
#ifndef EP_OSCF_USE_GETDATE
# define EP_OSCF_USE_GETDATE		0	// does getdate(3) exist?
#endif
#ifndef EP_OSCF_HAS_SD_NOTIFY
# ifdef __has_include
#  if __has_include("systemd/sd-daemon.h")
#   define EP_OSCF_HAS_SD_NOTIFY	1	// have sd_notify(3) (systemd)
#  else
#   define EP_OSCF_HAS_SD_NOTIFY	0	// no sd_notify(3) (systemd)
#  endif
# endif
#endif


/*
** C Compiler configuration variables
**
**	EP_CCCF_USE_ATTRIBUTES
**		Use __attribute__ (at all; if 0, overrides specific attrs)
**	EP_CCCF_USE_ATTR_PACKED
**		Use __attribute__((packed))
*/

#ifndef EP_CCCF_USE_ATTRIBUTES
# define EP_CCCF_USE_ATTRIBUTES		1	// use __attribute__ decls
#endif

// wrap GCC attributes in macros to make code easier to port to non-gcc
#if EP_CCCF_USE_ATTRIBUTES
# define EP_ATTR(_attr)		__attribute__((_attr))
#else
# define EP_ATTR(_attr)
#endif
#define EP_ATTR_NORETURN	EP_ATTR(noreturn)
#if EP_CCCF_USE_ATTR_PACKED
# define EP_ATTR_PACKED		EP_ATTR(packed)
#else
# define EP_ATTR_PACKED
#endif

#endif // _EP_CONF_H_
