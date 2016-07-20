/* vim: set ai sw=4 sts=4 ts=4 :*/

/*
**	GDP_BUF --- data buffers for the GDP
**
**	----- BEGIN LICENSE BLOCK -----
**	GDP: Global Data Plane Support Library
**	From the Ubiquitous Swarm Lab, 490 Cory Hall, U.C. Berkeley.
**
**	Copyright (c) 2015, Regents of the University of California.
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
**	----- END LICENSE BLOCK -----
*/

#include "gdp.h"
#include "gdp_buf.h"

#include <stdarg.h>
#include <string.h>
#include <arpa/inet.h>

/*
**  Create a new buffer
*/

gdp_buf_t *
gdp_buf_new(void)
{
	return evbuffer_new();
}

/*
**  Reset the contents of a buffer.
**		Returns 0 on success, -1 on failure.
*/

int
gdp_buf_reset(gdp_buf_t *b)
{
	return evbuffer_drain(b, evbuffer_get_length(b));
}

/*
**  Free a buffer.
*/

void
gdp_buf_free(gdp_buf_t *b)
{
	evbuffer_free(b);
}

/*
**  Set a mutex for a buffer.
*/

void
gdp_buf_setlock(gdp_buf_t *buf, EP_THR_MUTEX *m)
{
	evbuffer_enable_locking(buf, m);
}

/*
**  Get the amount of data in a buffer.
*/

size_t
gdp_buf_getlength(gdp_buf_t *buf)
{
	return evbuffer_get_length(buf);
}

/*
**  Read data from buffer.
**		Returns number of bytes actually read.
*/

size_t
gdp_buf_read(gdp_buf_t *buf, void *out, size_t sz)
{
	return evbuffer_remove(buf, out, sz);
}

/*
**  "Peek" at data in a buffer.
**		Like read, but leaves the buffer intact.
*/

size_t
gdp_buf_peek(gdp_buf_t *buf, void *out, size_t sz)
{
	ssize_t s = evbuffer_copyout(buf, out, sz);
	if (s < 0)
		return 0;
	return s;
}

/*
**  Remove data from a buffer (and discard it).
**		Returns 0 on success, -1 on failure.
*/

int
gdp_buf_drain(gdp_buf_t *buf, size_t sz)
{
	return evbuffer_drain(buf, sz);
}

/*
**  Return a pointer to the data in a buffer (kind of like peek w/o copy).
*/

unsigned char *
gdp_buf_getptr(gdp_buf_t *buf, size_t sz)
{
	return evbuffer_pullup(buf, sz);
}

/*
**  Write data to a buffer.
**		Returns 0 on success, -1 on failure.
*/

int
gdp_buf_write(gdp_buf_t *buf, const void *in, size_t sz)
{
	return evbuffer_add(buf, in, sz);
}

/*
**  Do a "printf" on to the end of a buffer.
**		Returns the number of bytes added.
*/

int
gdp_buf_printf(gdp_buf_t *buf, const char *fmt, ...)
{
	va_list ap;
	int res;

	va_start(ap, fmt);
	res = evbuffer_add_vprintf(buf, fmt, ap);
	va_end(ap);
	return res;
}

/*
**  Append the contents of one buffer onto another.
**		Returns 0 on success, -1 on failure.
**		This removes the data from ibuf and appends it to obuf.
*/

int
gdp_buf_move(gdp_buf_t *ibuf, gdp_buf_t *obuf)
{
	return evbuffer_add_buffer(obuf, ibuf);
}

/*
**  Dump buffer to a file (for debugging).
*/

void
gdp_buf_dump(gdp_buf_t *buf, FILE *fp)
{
	fprintf(fp, "gdp_buf @ %p: len=%zu\n",
			buf, gdp_buf_getlength(buf));
}

/*
**  Get a 32 bit unsigned int in network byte order from a buffer.
*/

uint32_t
gdp_buf_get_uint32(gdp_buf_t *buf)
{
	uint32_t t;

	evbuffer_remove(buf, &t, sizeof t);
	return ntohl(t);
}

/*
**  Get a 48 bit unsigned int in network byte order from a buffer.
*/

uint64_t
gdp_buf_get_uint48(gdp_buf_t *buf)
{
	uint16_t h;
	uint32_t l;

	evbuffer_remove(buf, &h, sizeof h);
	evbuffer_remove(buf, &l, sizeof l);
	return ((uint64_t) ntohs(h) << 32) | ((uint64_t) ntohl(l));
}

/*
**  Get a 64 bit unsigned int in network byte order from a buffer.
*/

uint64_t
gdp_buf_get_uint64(gdp_buf_t *buf)
{
	uint64_t t;
	static const int32_t num = 42;

	evbuffer_remove(buf, &t, sizeof t);
	if (ntohl(num) == num)
	{
		return t;
	}
	else
	{
		uint32_t h = htonl((uint32_t) (t >> 32));
		uint32_t l = htonl((uint32_t) (t & UINT64_C(0xffffffff)));

		return ((uint64_t) l) << 32 | h;
	}
}

/*
**  Get a time stamp in network byte order from a buffer.
*/

void
gdp_buf_get_timespec(gdp_buf_t *buf, EP_TIME_SPEC *ts)
{
	uint32_t t;

	ts->tv_sec = gdp_buf_get_uint64(buf);
	ts->tv_nsec = gdp_buf_get_uint32(buf);
	t = gdp_buf_get_uint32(buf);
	memcpy(&ts->tv_accuracy, &t, sizeof ts->tv_accuracy);
}


/*
**  Put a 32 bit unsigned integer to a buffer in network byte order.
*/

void
gdp_buf_put_uint32(gdp_buf_t *buf, const uint32_t v)
{
	uint32_t t = htonl(v);
	evbuffer_add(buf, &t, sizeof t);
}


/*
**  Put a 48 bit unsigned integer to a buffer in network byte order.
*/

void
gdp_buf_put_uint48(gdp_buf_t *buf, const uint64_t v)
{
	uint16_t h = htons((v >> 32) & 0xffff);
	uint32_t l = htonl((uint32_t) (v & UINT64_C(0xffffffff)));
	evbuffer_add(buf, &h, sizeof h);
	evbuffer_add(buf, &l, sizeof l);
}


/*
**  Put a 64 bit unsigned integer to a buffer in network byte order.
*/

void
gdp_buf_put_uint64(gdp_buf_t *buf, const uint64_t v)
{
	static const int32_t num = 42;

	if (htonl(num) == num)
	{
		evbuffer_add(buf, &v, sizeof v);
	}
	else
	{
		uint64_t t = htonl(((uint32_t) v & UINT64_C(0xffffffff)));
		t <<= 32;
		t |= (uint64_t) htonl((uint32_t) (v >> 32));
		evbuffer_add(buf, &t, sizeof t);
	}
}


/*
**  Put a time stamp to a buffer in network byte order.
*/

void
gdp_buf_put_timespec(gdp_buf_t *buf, EP_TIME_SPEC *ts)
{
	gdp_buf_put_uint64(buf, ts->tv_sec);
	gdp_buf_put_uint32(buf, ts->tv_nsec);
	gdp_buf_put_uint32(buf, *((uint32_t *) &ts->tv_accuracy));
}
