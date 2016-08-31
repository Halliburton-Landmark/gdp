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
#include <ep_mem.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

static struct ep_malloc_functions	DefaultMallocFunctions =
{
	&malloc,
	&realloc,
	&valloc,
	&free,
};

static struct ep_malloc_functions	*SystemMallocFunctions =
						&DefaultMallocFunctions;

static void	(*MemoryRecoveryFunc)(void);


/*
**  EP_SET_MALLOC_FUNCTIONS --- set the malloc functions
**
*/


void
ep_mem_set_malloc_functions(struct ep_malloc_functions *mf)
{
	SystemMallocFunctions = mf;
}


/*
**  EP_MEM_SET_RECOVERY_FUNC --- set an "out of memory" recovery function
*/

void
ep_mem_set_recovery_func(void (*f)(void))
{
	MemoryRecoveryFunc = f;
}


/*
**  SYSTEM_MALLOC -- access system memory allocator
**
**	Parameters:
**		nbytes -- the number of bytes to allocate
**		curmem -- a current pointer (for realloc)
**		aligned -- set if we want page-aligned memory
**
**	Returns:
**		A pointer to that memory; NULL if not available
*/

static void *
system_malloc(
	size_t nbytes,
	void *curmem,
	bool aligned)
{
	void *p;

	if (curmem != NULL)
		p = (SystemMallocFunctions->m_realloc)(curmem, nbytes);
#ifdef EP_OSCF_SYSTEM_VALLOC
	else if (aligned)
		p = (SystemMallocFunctions->m_valloc)(nbytes);
#endif
	else
		p = (SystemMallocFunctions->m_malloc)(nbytes);
	return p;
}


/*
**  SYSTEM_MFREE -- access system memory deallocator
**
**	Parameters:
**		p -- the memory to free
**
**	Returns:
**		none.
*/

static void
system_mfree(void *p)
{
	// we assume system deallocator does its own mutex
	(*SystemMallocFunctions->m_free)(p);
}


/*
**  EP_MEM_IALLOC -- allocate memory from the heap (internal)
**
**	This is the routine that does all the real work.
**
**	This always succeeds, unless the EP_MEM_F_FAILOK flag bit
**	is set.  If memory cannot be allocated, it posts an error
**	condition.  The condition handler has four choices:
**	it can do some memory cleanup and return
**	EP_STAT_MEM_TRYAGAIN to retry the allocation.  It can
**	clean up and abort the thread (i.e., not return) -- for
**	example, by dropping the current connection and releasing
**	any memory associated with it, presumably also terminating
**	the associated task.  Or it can return another status to
**	cause us to abort the process.
**
**	Parameters:
**		nbytes -- how much memory to allocate
**		curmem -- a current memory pointer (for realloc)
**		flags -- flags to modify operation
**		file -- file name of caller (for debugging)
**		line -- line number of caller (for debugging)
**
**	Returns:
**		the memory
*/

void *
ep_mem_ialloc(
	size_t nbytes,
	void *curmem,
	uint32_t flags,
	const char *file,
	int line)
{
	void *p;

	// always require at least one pointer worth of data
	if (nbytes < sizeof (void *))
		nbytes = sizeof (void *);

	// see if this request wants page alignment
//	if ((nbytes & (EP_OSCF_MEM_PAGESIZE - 1)) == 0)
//		flags |= EP_MEM_F_ALIGN;

	// get the memory itself
	p = system_malloc(nbytes, curmem, EP_UT_BITSET(EP_MEM_F_ALIGN, flags));

	if (p == NULL)
	{
		char e1buf[80];

		if (EP_UT_BITSET(EP_MEM_F_FAILOK, flags))
			goto done;

		// attempt recovery
		if (MemoryRecoveryFunc != NULL)
		{
			(*MemoryRecoveryFunc)();
			p = system_malloc(nbytes, curmem,
					EP_UT_BITSET(EP_MEM_F_ALIGN, flags));
		}

		if (p == NULL)
		{
			// no luck ...  bail out
			strerror_r(errno, e1buf, sizeof e1buf);
			ep_assert_failure("memory", file, line,
					"Out of memory: %s", e1buf);
			/*NOTREACHED*/
		}
	}

	// zero or trash memory if requested
	if ( EP_UT_BITSET(EP_MEM_F_ZERO, flags))
		memset(p, 0, nbytes);
	else if (EP_UT_BITSET(EP_MEM_F_TRASH, flags))
		ep_mem_trash(p, nbytes);

	// return pointer to allocated memory
done:
	return p;
}




/*
**  EP_MEM_[MZR]ALLOC_F -- malloc to plug into function pointer
**
**	Used for other libraries that require a pointer to a routine
**	taking one size argument.  Essentially this is plain old malloc.
*/

void *
ep_mem_malloc_f(size_t nbytes)
{
	return ep_mem_ialloc(nbytes, NULL, 0, NULL, 0);
}

void *
ep_mem_zalloc_f(size_t nbytes)
{
	return ep_mem_ialloc(nbytes, NULL, EP_MEM_F_ZERO, NULL, 0);
}

void *
ep_mem_ralloc_f(size_t nbytes)
{
	return ep_mem_ialloc(nbytes, NULL, EP_MEM_F_TRASH, NULL, 0);
}

void *
ep_mem_realloc_f(size_t nbytes,
	void *curmem)
{
	return ep_mem_ialloc(nbytes, curmem, 0, NULL, 0);
}

void *
ep_mem_zrealloc_f(size_t nbytes,
	void *curmem)
{
	return ep_mem_ialloc(nbytes, curmem, EP_MEM_F_ZERO, NULL, 0);
}


/*
**  EP_MEM_[MZR]ALLOC -- same thing, but just in the non-debugging case
*/

#undef ep_mem_malloc

void *
ep_mem_malloc(size_t nbytes)
{
	return ep_mem_ialloc(nbytes, NULL, 0, NULL, 0);
}

#undef ep_mem_zalloc

void *
ep_mem_zalloc(size_t nbytes)
{
	void *p;

	p = ep_mem_ialloc(nbytes, NULL, EP_MEM_F_ZERO, NULL, 0);
	return p;
}

#undef ep_mem_ralloc

void *
ep_mem_ralloc(size_t nbytes)
{
	void *p;

	p = ep_mem_ialloc(nbytes, NULL, EP_MEM_F_TRASH, NULL, 0);
	return p;
}

#undef ep_mem_realloc

void *
ep_mem_realloc(
	void *curmem,
	size_t nbytes)
{
	return ep_mem_ialloc(nbytes, curmem, 0, NULL, 0);
}

#undef ep_mem_zrealloc

void *
ep_mem_zrealloc(
	void *curmem,
	size_t nbytes)
{
	return ep_mem_ialloc(nbytes, curmem, EP_MEM_F_ZERO, NULL, 0);
}


/*
**  EP_MEM_TSTRDUP -- save a string onto a heap (tagged)
**
**	paramters:
**		s -- the string to duplicate
**		slen -- the maximum length of s to copy -- -1 => all
**		flags -- as usual
**		grp -- an arbitrary group number (for debugging)
**		file -- the file name (for debugging)
**		line -- the line number (for debugging)
*/

char *
ep_mem_istrdup(
	const char *s,
	ssize_t slen,
	uint32_t flags,
	const char *file,
	int line)
{
	ssize_t l;
	char *p;

	if (s == NULL)
		return NULL;
	l = strlen(s);
	if (slen >= 0 && l > slen)
		l = slen;
	EP_ASSERT_REQUIRE(l + 1 > l);
	p = ep_mem_ialloc(l + 1, NULL, flags, file, line);
	if (p != NULL)
	{
		memcpy(p, s, l);
		p[l] = '\0';
	}
	return p;
}

/*
**  EP_MEM_STRDUP -- only used if memory debugging is not compiled in
*/

#undef ep_mem_strdup

char *
ep_mem_strdup(const char *s)
{
	return ep_mem_istrdup(s, -1, 0, NULL, 0);
}

/*
**  EP_MEM_TRASH -- set memory to random values
**
**	No gurantees here: the memory might be set to pseudo-random
**	bytes, it might be set to 0xDEADBEEF, or might be untouched.
**	If you require specific semantics, take care of it yourself.
**
**	Parameters:
**		p -- pointer to the memory
**		nbytes -- size of memory
**
**	Returns:
**		nothing
*/

void
ep_mem_trash(void *p,
	size_t nbytes)
{
	if (nbytes > 0)
		memset(p, 0xA5, nbytes);
}



/*
**  EP_MEM_FREE -- free allocated memory
**
**	Parameters:
**		p -- the memory to free
**
**	Returns:
**		void
*/

void
ep_mem_free(void *p)
{
	system_mfree(p);
}
