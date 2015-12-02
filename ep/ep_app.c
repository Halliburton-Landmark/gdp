/* vim: set ai sw=8 sts=8 ts=8 :*/

/***********************************************************************
**  ----- BEGIN LICENSE BLOCK -----
**	LIBEP: Enhanced Portability Library (Reduced Edition)
**
**	Copyright (c) 2008-2015, Eric P. Allman.  All rights reserved.
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
**  ----- END LICENSE BLOCK -----
***********************************************************************/

#include <ep.h>
#include <ep_string.h>
#include <ep_app.h>
#include <stdlib.h>
#include <sys/errno.h>


////////////////////////////////////////////////////////////////////////
//
//  Application support
//
//	Just to make life easier for app writers
//
////////////////////////////////////////////////////////////////////////

const char *
ep_app_getprogname(void)
{
#if EP_OSCF_HAS_GETPROGNAME
	return getprogname();
#elif __linux__
	extern char *program_invocation_short_name;
	return program_invocation_short_name;
#else
	return NULL;
# endif
}



////////////////////////////////////////////////////////////////////////
//
//  PRINTMESSAGE -- helper routine for message printing
//

static void
printmessage(const char *tag, const char *fmt, va_list av)
{
	const char *progname;

	if ((progname = ep_app_getprogname()) != NULL)
		fprintf(stderr, "%s: ", progname);
	fprintf(stderr, "%s%s%s: ", EpVid->vidinv, tag, EpVid->vidnorm);
	if (fmt != NULL)
		vfprintf(stderr, fmt, av);
	else
		fprintf(stderr, "unknown %s", tag);
	if (errno != 0)
	{
		char nbuf[40];

		strerror_r(errno, nbuf, sizeof nbuf);
		fprintf(stderr, "\n\t(%s)", nbuf);
	}
	fprintf(stderr, "\n");
}


////////////////////////////////////////////////////////////////////////
//
//  EP_APP_WARN -- print a warning message
//
//	Parameters:
//		fmt -- format for a message
//		... -- arguments
//
//	Returns:
//		never
//

void
ep_app_warn(
	const char *fmt,
	...)
{
	va_list av;

	va_start(av, fmt);
	printmessage("WARNING", fmt, av);
	va_end(av);
}


////////////////////////////////////////////////////////////////////////
//
//  EP_APP_ERROR -- print an error message
//
//	Parameters:
//		fmt -- format for a message
//			If NULL it prints errno.
//		... -- arguments
//
//	Returns:
//		never
//

void
ep_app_error(
	const char *fmt,
	...)
{
	va_list av;

	va_start(av, fmt);
	printmessage("ERROR", fmt, av);
	va_end(av);
}


////////////////////////////////////////////////////////////////////////
//
//  EP_APP_FATAL -- print a fatal error message and exit
//
//	Just uses a generic exit status.
//
//	Parameters:
//		fmt -- format for a message
//		... -- arguments
//
//	Returns:
//		never
//

void
ep_app_fatal(
	const char *fmt,
	...)
{
	va_list av;

	va_start(av, fmt);
	printmessage("FATAL", fmt, av);
	va_end(av);

	fprintf(stderr, "\t(exiting)\n");
	exit(1);
	/*NOTREACHED*/
}


////////////////////////////////////////////////////////////////////////
//
//  EP_APP_ABORT -- print an error message and abort
//
//	Parameters:
//		fmt -- format for a message
//		... -- arguments
//
//	Returns:
//		never
//

void
ep_app_abort(
	const char *fmt,
	...)
{
	va_list av;

	va_start(av, fmt);
	printmessage("ABORT", fmt, av);
	va_end(av);

	fprintf(stderr, "\n\t(exiting)\n");
	abort();
	/*NOTREACHED*/
}
