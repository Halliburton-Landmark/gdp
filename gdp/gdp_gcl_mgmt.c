/* vim: set ai sw=4 sts=4 ts=4 :*/

/*
**	This implements GDP resource management (primarily locking and memory).
**
**	----- BEGIN LICENSE BLOCK -----
**	GDP: Global Data Plane Support Library
**	From the Ubiquitous Swarm Lab, 490 Cory Hall, U.C. Berkeley.
**
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
**	----- END LICENSE BLOCK -----
*/

#include <ep/ep.h>
#include <ep/ep_app.h>
#include <ep/ep_dbg.h>
#include <ep/ep_hash.h>
#include <ep/ep_log.h>
#include <ep/ep_prflags.h>
#include <ep/ep_thr.h>

#include "gdp.h"
#include "gdp_priv.h"

#include <event2/event.h>

#include <errno.h>
#include <string.h>

static EP_DBG	Dbg = EP_DBG_INIT("gdp.gcl.mgmt", "GCL resource management");



/***********************************************************************
**
**	GCL Management
**
***********************************************************************/

extern EP_THR_MUTEX		_GclCacheMutex;
static LIST_HEAD(gcl_free_head, gdp_gcl)
						GclFreeList = LIST_HEAD_INITIALIZER(GclFreeList);

static int				NGclsAllocated = 0;


/*
**	_GDP_GCL_NEWHANDLE --- create a new gcl_handle & initialize
**
**		Only initialization done is the mutex and the name.
**
**	Parameters:
**		gcl_name --- internal (256-bit) name of the GCL
**		pgcl --- location to store the resulting GCL handle
**
**		gcl is returned unlocked.
*/

EP_STAT
_gdp_gcl_newhandle(gdp_name_t gcl_name, gdp_gcl_t **pgcl)
{
	EP_STAT estat = EP_STAT_OK;
	gdp_gcl_t *gcl = NULL;

	ep_thr_mutex_lock(&_GclCacheMutex);
	if (!LIST_EMPTY(&GclFreeList))
	{
		gcl = LIST_FIRST(&GclFreeList);
		LIST_REMOVE(gcl, ulist);
	}
	ep_thr_mutex_unlock(&_GclCacheMutex);

	if (gcl == NULL)
	{
		// allocate the memory to hold the gcl_handle
		gcl = ep_mem_zalloc(sizeof *gcl);
		if (gcl == NULL)
			goto fail1;

		if (ep_thr_mutex_init(&gcl->mutex, EP_THR_MUTEX_DEFAULT) != 0)
			goto fail1;
		ep_thr_mutex_setorder(&gcl->mutex, GDP_MUTEX_LORDER_GCL);
	}
	VALGRIND_HG_CLEAN_MEMORY(gcl, sizeof *gcl);

	LIST_INIT(&gcl->reqs);
	gcl->refcnt = 1;
	gcl->nrecs = 0;
	NGclsAllocated++;

	// create a name if we don't have one passed in
	if (gcl_name == NULL || !gdp_name_is_valid(gcl_name))
		_gdp_newname(gcl->name, gcl->gclmd);	//XXX bogus: gcl->gclmd isn't set yet
	else
		memcpy(gcl->name, gcl_name, sizeof gcl->name);
	gdp_printable_name(gcl->name, gcl->pname);

	// success
	gcl->flags |= GCLF_INUSE;
	*pgcl = gcl;
	ep_dbg_cprintf(Dbg, 28, "_gdp_gcl_newhandle => %p (%s)\n",
			gcl, gcl->pname);
	return estat;

fail1:
	estat = ep_stat_from_errno(errno);
	ep_mem_free(gcl);

	char ebuf[100];
	ep_dbg_cprintf(Dbg, 4, "_gdp_gcl_newhandle failed: %s\n",
				ep_stat_tostr(estat, ebuf, sizeof ebuf));
	return estat;
}


/*
**  _GDP_GCL_FREEHANDLE --- drop an existing handle
*/

void
_gdp_gcl_freehandle(gdp_gcl_t *gcl)
{
	ep_dbg_cprintf(Dbg, 28, "_gdp_gcl_freehandle(%p)\n", gcl);
	if (gcl == NULL)
		return;

	// this is a forced free, so ignore existing refcnts, etc.
	gcl->refcnt = 0;

	GDP_GCL_ASSERT_ISLOCKED(gcl);
	gcl->flags |= GCLF_DROPPING | GCLF_ISLOCKED;

	// drop it from the name -> handle cache
	_gdp_gcl_cache_drop(gcl);

	// release any remaining requests
	_gdp_req_freeall(&gcl->reqs, NULL);

	// should be inacessible now
	_gdp_gcl_unlock(gcl);

	// free any additional per-GCL resources
	if (gcl->freefunc != NULL)
		(*gcl->freefunc)(gcl);
	gcl->freefunc = NULL;
	if (gcl->gclmd != NULL)
		gdp_gclmd_free(gcl->gclmd);
	gcl->gclmd = NULL;
	if (gcl->digest != NULL)
		ep_crypto_md_free(gcl->digest);
	gcl->digest = NULL;

	// if there is any "extra" data, drop that
	//		(redundant; should be done by the freefunc)
	if (gcl->x != NULL)
	{
		ep_mem_free(gcl->x);
		gcl->x = NULL;
	}

	// drop this (now empty) GCL handle on the free list
	gcl->flags = 0;
	ep_thr_mutex_lock(&_GclCacheMutex);
	LIST_INSERT_HEAD(&GclFreeList, gcl, ulist);
	ep_thr_mutex_unlock(&_GclCacheMutex);
	NGclsAllocated--;
}


/*
**  _GDP_GCL_DUMP --- print a GCL (for debugging)
*/

EP_PRFLAGS_DESC	GclFlags[] =
{
	{ GCLF_DROPPING,		GCLF_DROPPING,			"DROPPING"			},
	{ GCLF_INCACHE,			GCLF_INCACHE,			"INCACHE"			},
	{ GCLF_ISLOCKED,		GCLF_ISLOCKED,			"ISLOCKED"			},
	{ GCLF_INUSE,			GCLF_INUSE,				"INUSE"				},
	{ GCLF_DEFER_FREE,		GCLF_DEFER_FREE,		"DEFER_FREE"		},
	{ GCLF_KEEPLOCKED,		GCLF_KEEPLOCKED,		"KEEPLOCKED"		},
	{ 0, 0, NULL }
};

void
_gdp_gcl_dump(
		const gdp_gcl_t *gcl,
		FILE *fp,
		int detail,
		int indent)
{
	if (detail >= GDP_PR_BASIC)
		fprintf(fp, "GCL@%p: ", gcl);
	VALGRIND_HG_DISABLE_CHECKING(gcl, sizeof gcl);
	if (gcl == NULL)
	{
		fprintf(fp, "NULL\n");
	}
	else
	{
		if (!gdp_name_is_valid(gcl->name))
		{
			fprintf(fp, "no name\n");
		}
		else
		{
			fprintf(fp, "%s\n", gcl->pname);
		}

		if (detail >= GDP_PR_BASIC)
		{
			fprintf(fp, "\tiomode = %d, refcnt = %d, reqs = %p, nrecs = %"
					PRIgdp_recno "\n"
					"\tflags = ",
					gcl->iomode, gcl->refcnt, LIST_FIRST(&gcl->reqs),
					gcl->nrecs);
			ep_prflags(gcl->flags, GclFlags, fp);
			fprintf(fp, "\n");
			if (detail >= GDP_PR_DETAILED)
			{
				char tbuf[40];
				struct tm tm;

				fprintf(fp, "\tfreefunc = %p, gclmd = %p, digest = %p\n",
						gcl->freefunc, gcl->gclmd, gcl->digest);
				gmtime_r(&gcl->utime, &tm);
				strftime(tbuf, sizeof tbuf, "%Y-%m-%d %H:%M:%S", &tm);
				fprintf(fp, "\tutime = %s, x = %p\n", tbuf, gcl->x);
			}
		}
	}
	VALGRIND_HG_ENABLE_CHECKING(gcl, sizeof gcl);
}


/*
**  _GDP_GCL_LOCK --- lock a GCL
*/

void
_gdp_gcl_lock_trace(
		gdp_gcl_t *gcl,
		const char *file,
		int line,
		const char *id)
{
	//XXX cheat: _ep_thr_mutex_lock is a libep-private interface
	if (_ep_thr_mutex_lock(&gcl->mutex, file, line, id) == 0)
	{
		EP_ASSERT(!EP_UT_BITSET(GCLF_ISLOCKED, gcl->flags));
		gcl->flags |= GCLF_ISLOCKED;
	}
}


/*
**  _GDP_GCL_UNLOCK --- unlock a GCL
*/

void
_gdp_gcl_unlock_trace(
		gdp_gcl_t *gcl,
		const char *file,
		int line,
		const char *id)
{
	EP_ASSERT(EP_UT_BITSET(GCLF_ISLOCKED, gcl->flags));
	gcl->flags &= ~GCLF_ISLOCKED;

	//XXX cheat: _ep_thr_mutex_unlock is a libep-private interface
	_ep_thr_mutex_unlock(&gcl->mutex, file, line, id);
}


/*
**  Check to make sure a mutex is locked / unlocked.
*/

static int
get_lock_type(void)
{
	static int locktype;
	static bool initialized = false;

	if (initialized)
		return locktype;

	const char *p = ep_adm_getstrparam("libep.thr.mutex.type", "default");
	if (strcasecmp(p, "normal") == 0)
		locktype = EP_THR_MUTEX_NORMAL;
	else if (strcasecmp(p, "errorcheck") == 0)
		locktype = EP_THR_MUTEX_ERRORCHECK;
	else if (strcasecmp(p, "recursive") == 0)
		locktype = EP_THR_MUTEX_RECURSIVE;
	else
		locktype = EP_THR_MUTEX_DEFAULT;

	initialized = true;
	return locktype;
}

bool
_gdp_mutex_check_islocked(
		EP_THR_MUTEX *m,
		const char *mstr,
		const char *file,
		int line)
{
	int istat;

	// if we are using recursive locks, this won't tell much
	if (get_lock_type() == EP_THR_MUTEX_RECURSIVE)
		return true;

	// trylock should fail if the mutex is already locked
	istat = ep_thr_mutex_trylock(m);
	if (istat != 0)
		return true;

	// oops, must have been unlocked
	ep_thr_mutex_unlock(m);
	ep_assert_print(file, line, "mutex %s is not locked", mstr);
	return false;
}


bool
_gdp_mutex_check_isunlocked(
		EP_THR_MUTEX *m,
		const char *mstr,
		const char *file,
		int line)
{
	int istat;

	// anything but error checking locks?  tryunlock doesn't work
	if (get_lock_type() != EP_THR_MUTEX_ERRORCHECK)
		return true;

	// tryunlock should fail if the mutex is not already locked
	istat = ep_thr_mutex_tryunlock(m);
	if (istat == EPERM)
		return true;

	// oops, must have been locked
	ep_thr_mutex_lock(m);
	ep_assert_print(file, line, "mutex %s is already locked", mstr);
	return false;
}


/*
**  _GDP_GCL_INCREF --- increment the reference count on a GCL
**
**		Must be called with GCL locked.
*/

void
_gdp_gcl_incref(gdp_gcl_t *gcl)
{
	EP_ASSERT_ELSE(GDP_GCL_ISGOOD(gcl), return);
	GDP_GCL_ASSERT_ISLOCKED(gcl);

	gcl->refcnt++;
	_gdp_gcl_touch(gcl);
	ep_dbg_cprintf(Dbg, 51, "_gdp_gcl_incref(%p): %d\n", gcl, gcl->refcnt);
}


/*
**  _GDP_GCL_DECREF --- decrement the reference count on a GCL
**
**		The GCL must be locked on entry.  Upon return it will
**		be unlocked (and possibly deallocated if the refcnt has
**		dropped to zero.
*/

#undef _gdp_gcl_decref

void
_gdp_gcl_decref(gdp_gcl_t **gclp)
{
	_gdp_gcl_decref_trace(gclp, __FILE__, __LINE__, "gclp");
}

void
_gdp_gcl_decref_trace(
		gdp_gcl_t **gclp,
		const char *file,
		int line,
		const char *id)
{
	gdp_gcl_t *gcl = *gclp;

	EP_ASSERT_ELSE(GDP_GCL_ISGOOD(gcl), return);
	(void) ep_thr_mutex_assert_islocked(&gcl->mutex, id, file, line);

	EP_ASSERT(gcl->refcnt > 0);
	if (gcl->refcnt > 0)
		gcl->refcnt--;
	*gclp = NULL;

	ep_dbg_cprintf(Dbg, 51, "_gdp_gcl_decref(%p): %d\n",
			gcl, gcl->refcnt);
	if (gcl->refcnt == 0 && !EP_UT_BITSET(GCLF_DEFER_FREE, gcl->flags))
		_gdp_gcl_freehandle(gcl);
	else if (!EP_UT_BITSET(GCLF_KEEPLOCKED, gcl->flags))
		_gdp_gcl_unlock_trace(gcl, file, line, id);
}


/*
**  Print statistics (for debugging)
*/

void
_gdp_gcl_pr_stats(FILE *fp)
{
	fprintf(fp, "GCLs Allocated: %d\n", NGclsAllocated);
}
