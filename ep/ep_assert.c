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
#include <ep_assert.h>
#include <ep_stat.h>
#include <ep_string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>


void	(*EpAssertInfo)(void) = NULL;	// can be used to dump state
void	(*EpAssertAbort)(void) = NULL;	// alternate abort() function

#if !_EP_CCCF_ASSERT_NONE

/***********************************************************************
**
**  ASSERT_ABORT -- abort process because of an assertion failure
*/

static void EP_ATTR_NORETURN
assert_abort(void)
{
	// give the application an opportunity to do something else
	if (EpAssertAbort != NULL)
	{
		// avoid assertion failure loops
		void (*abortfunc)(void) = EpAssertAbort;
		EpAssertAbort = NULL;
		(*abortfunc)();
	}

	// we're still here....   but not for long
	abort();
}


/***********************************************************************
**
**  EP_ASSERT_PRINT[V] -- print assertion failure message (but do not abort)
*/

void
ep_assert_printv(
	const char *file,
	int line,
	const char *msg,
	va_list av)
{
	// log something here?

	flockfile(stderr);
	fprintf(stderr, "%s%sAssertion failed at %s:%d:\n\t",
			EpVid->vidfgcyan, EpVid->vidbgred,
			file, line);
	vfprintf(stderr, msg, av);
	fprintf(stderr, "%s\n", EpVid->vidnorm);

	// give the application a chance to print state
	if (EpAssertInfo != NULL)
		(*EpAssertInfo)();

	funlockfile(stderr);

#if _EP_CCCF_ASSERT_ALL_ABORT	//DEBUG: abort on all exceptions
	assert_abort();
#endif
}

void
ep_assert_print(
	const char *file,
	int line,
	const char *msg,
	...)
{
	va_list av;

	va_start(av, msg);
	ep_assert_printv(file, line, msg, av);
	va_end(av);
}

/***********************************************************************
**
**  EP_ASSERT_FAILURE -- internal routine to raise an assertion failure
**
**	ep_assert_nofail variant prints message but returns
**
**	Parameters:
**		file -- which file contained the assertion
**		line -- which line was it on
**		msg -- the message to print (printf format)
**		... -- arguments
**
**	Returns:
**		never
*/

void
ep_assert_failure(
	const char *file,
	int line,
	const char *msg,
	...)
{
	va_list av;

	va_start(av, msg);
	ep_assert_printv(file, line, msg, av);
	va_end(av);

	assert_abort();
	/*NOTREACHED*/
}

#endif // _EP_CCCF_ASSERT_NONE
