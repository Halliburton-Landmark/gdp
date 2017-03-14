/* vim: set ai sw=4 sts=4 ts=4 :*/

/*
**	This implements GDP Connection Log Cache
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
#include <ep/ep_thr.h>

#include "gdp.h"
#include "gdp_stat.h"
#include "gdp_priv.h"

#include <event2/event.h>

#include <errno.h>
#include <string.h>

static EP_DBG	Dbg = EP_DBG_INIT("gdp.gcl.cache",
						"GCL cache and reference counting");



/***********************************************************************
**
**	GCL Caching
**		Let's us find the internal representation of the GCL from
**		the name.  These are not really intended for public use,
**		but they are shared with gdplogd.
**
**		FIXME This is a very stupid implementation at the moment.
**
**		FIXME Makes no distinction between io modes (we cludge this
**			  by just opening everything for r/w for now)
**
***********************************************************************/

static EP_HASH			*OpenGCLCache;		// associative cache
LIST_HEAD(gcl_use_head, gdp_gcl)			// LRU cache
						GclsByUse		= LIST_HEAD_INITIALIZER(GclByUse);
static EP_THR_MUTEX		GclCacheMutex;


/*
**  Initialize the GCL cache
*/

EP_STAT
_gdp_gcl_cache_init(void)
{
	EP_STAT estat = EP_STAT_OK;
	int istat;
	const char *err;

	if (OpenGCLCache == NULL)
	{
		istat = ep_thr_mutex_init(&GclCacheMutex, EP_THR_MUTEX_RECURSIVE);
		if (istat != 0)
		{
			estat = ep_stat_from_errno(istat);
			err = "could not initialize GclCacheMutex";
			goto fail0;
		}

		OpenGCLCache = ep_hash_new("OpenGCLCache", NULL, 0);
		if (OpenGCLCache == NULL)
		{
			estat = ep_stat_from_errno(errno);
			err = "could not create OpenGCLCache";
			goto fail0;
		}
	}

	// Nothing to do for LRU cache

	if (false)
	{
fail0:
		ep_log(estat, "gdp_gcl_cache_init: %s", err);
		ep_app_fatal("gdp_gcl_cache_init: %s", err);
	}

	return estat;
}


/*
**  Add a GCL to both the associative and the LRU caches.
*/

void
_gdp_gcl_cache_add(gdp_gcl_t *gcl, gdp_iomode_t mode)
{
	// sanity checks
	EP_ASSERT_ELSE(GDP_GCL_ISGOOD(gcl), return);
	EP_ASSERT_ELSE(gdp_name_is_valid(gcl->name), return);
	EP_THR_MUTEX_ASSERT_ISLOCKED(&gcl->mutex, );

	ep_dbg_cprintf(Dbg, 49, "_gdp_gcl_cache_add(%p): adding\n", gcl);
	if (EP_UT_BITSET(GCLF_INCACHE, gcl->flags))
	{
		ep_dbg_cprintf(Dbg, 41, "_gdp_gcl_cache_add(%p): already cached\n",
				gcl);
		return;
	}

	// save it in the associative cache
	gdp_gcl_t *g2;
	ep_dbg_cprintf(Dbg, 49, "_gdp_gcl_cache_add(%p): insert into OpenGCLCache\n",
			gcl);
	g2 = ep_hash_insert(OpenGCLCache, sizeof (gdp_name_t), gcl->name, gcl);
	if (g2 != NULL)
	{
		EP_ASSERT_PRINT("duplicate GCL cache entry");
		fprintf(stderr, "New ");
		_gdp_gcl_dump(gcl, stderr, GDP_PR_DETAILED, 0);
		fprintf(stderr, "Existing ");
		_gdp_gcl_dump(g2, stderr, GDP_PR_DETAILED, 0);
	}

	// ... and the LRU list
	{
		struct timeval tv;

		gettimeofday(&tv, NULL);
		gcl->utime = tv.tv_sec;

		ep_dbg_cprintf(Dbg, 49, "_gdp_gcl_cache_add(%p): insert into LRU list\n",
				gcl);
		ep_thr_mutex_lock(&GclCacheMutex);
		IF_LIST_CHECK_OK(&GclsByUse, gcl, ulist, gdp_gcl_t)
		{
			LIST_INSERT_HEAD(&GclsByUse, gcl, ulist);
		}
		ep_thr_mutex_unlock(&GclCacheMutex);
	}

	gcl->flags |= GCLF_INCACHE;
	ep_dbg_cprintf(Dbg, 40, "_gdp_gcl_cache_add: %s => %p\n",
			gcl->pname, gcl);
}


/*
**  Update the name of a GCL in the cache
**		This is used when creating a new GCL.  The original GCL
**		is a dummy that uses the name of the log server.  After
**		the log actually exists it has to be updated with the
**		real name.
*/

void
_gdp_gcl_cache_changename(gdp_gcl_t *gcl, gdp_name_t newname)
{
	// sanity checks
	EP_ASSERT_ELSE(GDP_GCL_ISGOOD(gcl), return);
	EP_ASSERT_ELSE(gdp_name_is_valid(gcl->name), return);
	EP_ASSERT_ELSE(gdp_name_is_valid(newname), return);
	EP_ASSERT_ELSE(EP_UT_BITSET(GCLF_INCACHE, gcl->flags), return);

	ep_thr_mutex_lock(&GclCacheMutex);
	(void) ep_hash_delete(OpenGCLCache, sizeof (gdp_name_t), gcl->name);
	(void) memcpy(gcl->name, newname, sizeof (gdp_name_t));
	(void) ep_hash_insert(OpenGCLCache, sizeof (gdp_name_t), newname, gcl);
	ep_thr_mutex_unlock(&GclCacheMutex);

	ep_dbg_cprintf(Dbg, 40, "_gdp_gcl_cache_changename: %s => %p\n",
					gcl->pname, gcl);
}


/*
**  _GDP_GCL_CACHE_GET --- get a GCL from the cache, if it exists
**
**		To avoid deadlock due to improper lock ordering, you can't
**		hold GclCacheMutex while locking or unlocking a GCL, which 
**		can cause problems.
**
**		If found, the refcnt is bumped for the returned GCL,
**		i.e., the caller is responsible for calling
**		_gdp_gcl_decref(&gcl) when it is finished with it.
**
**		gcl is returned locked.
*/

gdp_gcl_t *
_gdp_gcl_cache_get(gdp_name_t gcl_name, gdp_iomode_t mode)
{
	gdp_gcl_t *gcl;

	// see if we have a pointer to this GCL in the cache
	// don't need to lock GclCacheMutex since OpenGCLCache is protected
	gcl = ep_hash_search(OpenGCLCache, sizeof (gdp_name_t), (void *) gcl_name);
	if (gcl == NULL)
		goto done;
	_gdp_gcl_lock(gcl);

	// sanity checking --- someone may have snuck in before we acquired the lock
	if (!EP_UT_BITSET(GCLF_INUSE, gcl->flags) ||
			!EP_UT_BITSET(GCLF_INCACHE, gcl->flags) ||
			EP_UT_BITSET(GCLF_DROPPING, gcl->flags))
	{
		// someone deallocated this in the brief window above
		_gdp_gcl_unlock(gcl);
		gcl = NULL;
	}
	else
	{
		// we're good to go
		if (mode != _GDP_MODE_PEEK)
			_gdp_gcl_incref(gcl);
	}

done:
	if (gcl == NULL)
	{
		if (ep_dbg_test(Dbg, 42))
		{
			gdp_pname_t pname;

			gdp_printable_name(gcl_name, pname);
			ep_dbg_printf("gdp_gcl_cache_get: %s => NULL\n", pname);
		}
	}
	else
	{
		ep_dbg_cprintf(Dbg, 42, "gdp_gcl_cache_get: %s =>\n"
					"\t%p refcnt %d\n",
					gcl->pname, gcl, gcl->refcnt);
	}
	return gcl;
}


/*
** Drop a GCL from both the associative and the LRU caches
**
**		GclCacheMutex should already be acquired, since this is
**		only called when freeing resources.
*/

void
_gdp_gcl_cache_drop(gdp_gcl_t *gcl)
{
	bool gcl_was_locked = false;

	EP_ASSERT_ELSE(gcl != NULL, return);
	if (EP_ASSERT_TEST(GDP_GCL_ISGOOD(gcl)))
	{
		// GCL is in some random state --- we need the name at least
		EP_ASSERT_ELSE(gdp_name_is_valid(gcl->name), return);
	}

	// make sure it is locked
	if (ep_thr_mutex_trylock(&gcl->mutex) != 0)
		gcl_was_locked = true;

	if (!EP_UT_BITSET(GCLF_INCACHE, gcl->flags))
	{
		ep_dbg_cprintf(Dbg, 8, "_gdp_gcl_cache_drop(%p): uncached\n", gcl);
		goto fail0;
	}

	// error if we're dropping something that's referenced from the cache
	if (gcl->refcnt != 0)
	{
		ep_log(GDP_STAT_BAD_REFCNT, "_gdp_gcl_cache_drop: ref count %d != 0",
				gcl->refcnt);
		if (ep_dbg_test(Dbg, 1))
			_gdp_gcl_dump(gcl, ep_dbg_getfile(), GDP_PR_BASIC, 0);
	}

	// remove it from the associative cache
	(void) ep_hash_delete(OpenGCLCache, sizeof (gdp_name_t), gcl->name);

	// ... and the LRU list
	LIST_REMOVE(gcl, ulist);
	gcl->flags &= ~GCLF_INCACHE;

	ep_dbg_cprintf(Dbg, 40, "_gdp_gcl_cache_drop: %s => %p\n",
			gcl->pname, gcl);
fail0:
	if (!gcl_was_locked)
		ep_thr_mutex_unlock(&gcl->mutex);
}


/*
**  _GDP_GCL_TOUCH --- move GCL to the front of the LRU list
**
**		GCL must be locked when we enter.
*/

void
_gdp_gcl_touch(gdp_gcl_t *gcl)
{
	struct timeval tv;

	EP_ASSERT_ELSE(GDP_GCL_ISGOOD(gcl), return);
	EP_THR_MUTEX_ASSERT_ISLOCKED(&gcl->mutex, return);

	if (!EP_UT_BITSET(GCLF_INCACHE, gcl->flags))
	{
		ep_dbg_cprintf(Dbg, 8, "_gcl_gcl_touch(%p): uncached!\n", gcl);
		return;
	}

	ep_dbg_cprintf(Dbg, 46, "_gdp_gcl_touch(%p)\n", gcl);

	gettimeofday(&tv, NULL);
	gcl->utime = tv.tv_sec;

	if (!EP_UT_BITSET(GCLF_INCACHE, gcl->flags))
		ep_thr_mutex_lock(&GclCacheMutex);
	LIST_REMOVE(gcl, ulist);
	IF_LIST_CHECK_OK(&GclsByUse, gcl, ulist, gdp_gcl_t)
	{
		LIST_INSERT_HEAD(&GclsByUse, gcl, ulist);
	}
	if (!EP_UT_BITSET(GCLF_INCACHE, gcl->flags))
		ep_thr_mutex_unlock(&GclCacheMutex);
}


/*
**  Reclaim cache entries older than a specified age
**
**		If we can't get the number of file descriptors down far enough
**		we keep trying with increasingly stringent constraints, so maxage
**		is really more advice than a requirement.
**
**		XXX	Currently the GCL is not reclaimed if the reference count
**		XXX	is greater than zero, i.e., someone is subscribed to it.
**		XXX But the file descriptors associated with it _could_ be
**		XXX	reclaimed, especially since they are a scarce resource.
*/

void
_gdp_gcl_cache_reclaim(time_t maxage)
{
	static int headroom = 0;

	ep_dbg_cprintf(Dbg, 68, "_gdp_gcl_cache_reclaim(maxage = %ld)\n", maxage);

	// collect some parameters (once only)
	if (headroom == 0)
	{
		headroom = ep_adm_getintparam("swarm.gdp.cache.fd.headroom", 0);
		if (headroom == 0)
		{
			int maxfds;
			(void) ep_app_numfds(&maxfds);
			headroom = maxfds - ((maxfds * 2) / 3);
			if (headroom == 0)
				headroom = 8;
		}
	}

	for (;;)
	{
		struct timeval tv;
		gdp_gcl_t *g1, *g2;
		time_t mintime;

		gettimeofday(&tv, NULL);
		mintime = tv.tv_sec - maxage;

		ep_thr_mutex_lock(&GclCacheMutex);
		for (g1 = LIST_FIRST(&GclsByUse); g1 != NULL; g1 = g2)
		{
			_gdp_gcl_lock(g1);
			g2 = LIST_NEXT(g1, ulist);
			if (g1->utime > mintime)
			{
				_gdp_gcl_unlock(g1);
				break;
			}
			if (EP_UT_BITSET(GCLF_DROPPING, g1->flags) || g1->refcnt > 0)
			{
				if (ep_dbg_test(Dbg, 19))
				{
					ep_dbg_printf("_gdp_gcl_cache_reclaim: skipping:\n   ");
					_gdp_gcl_dump(g1, ep_dbg_getfile(), GDP_PR_DETAILED, 0);
				}
				_gdp_gcl_unlock(g1);
				continue;
			}

			if (ep_dbg_test(Dbg, 32))
			{
				ep_dbg_printf("_gdp_gcl_cache_reclaim: reclaiming:\n   ");
				_gdp_gcl_dump(g1, ep_dbg_getfile(), GDP_PR_DETAILED, 0);
			}
			LIST_REMOVE(g1, ulist);
			_gdp_gcl_freehandle(g1);
		}
		ep_thr_mutex_unlock(&GclCacheMutex);

		// check to see if we have enough headroom
		int maxfds;
		int nfds = ep_app_numfds(&maxfds);

		if (nfds < maxfds - headroom)
			return;

		// try again, shortening timeout
		maxage -= maxage < 4 ? 1 : maxage / 4;
		if (maxage <= 0)
		{
			ep_log(EP_STAT_WARN,
					"_gdp_gcl_cache_reclaim: cannot reach headroom %d, nfds %d",
					headroom, nfds);
			return;
		}
	}
}


/*
**  Shut down GCL cache --- immediately!
**
**		Informs subscribers of imminent shutdown.
**		Only used when the entire daemon is shutting down.
*/

void
_gdp_gcl_cache_shutdown(void (*shutdownfunc)(gdp_req_t *))
{
	gdp_gcl_t *g1, *g2;

	ep_dbg_cprintf(Dbg, 30, "\n_gdp_gcl_shutdown\n");

	// don't bother with mutexes --- we need to shut down now!

	// free all GCLs and all reqs linked to them
	for (g1 = LIST_FIRST(&GclsByUse); g1 != NULL; g1 = g2)
	{
		g2 = LIST_NEXT(g1, ulist);
		LIST_REMOVE(g1, ulist);
		_gdp_req_freeall(&g1->reqs, shutdownfunc);
		_gdp_gcl_freehandle(g1);
	}
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
	_ep_thr_mutex_lock(&gcl->mutex, file, line, id);
	gcl->flags |= GCLF_ISLOCKED;
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
	//XXX cheat: _ep_thr_mutex_unlock is a libep-private interface
	gcl->flags &= ~GCLF_ISLOCKED;
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
	EP_THR_MUTEX_ASSERT_ISLOCKED(&gcl->mutex, );

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

	if (gcl->refcnt > 0)
		gcl->refcnt--;
	else
		ep_log(GDP_STAT_BAD_REFCNT, "_gdp_gcl_decref: %p: zero refcnt", gcl);
	*gclp = NULL;

	ep_dbg_cprintf(Dbg, 51, "_gdp_gcl_decref(%p): %d\n",
			gcl, gcl->refcnt);
	if (gcl->refcnt == 0 && !EP_UT_BITSET(GCLF_DEFER_FREE, gcl->flags))
		_gdp_gcl_freehandle(gcl);
	else if (!EP_UT_BITSET(GCLF_KEEPLOCKED, gcl->flags))
		_gdp_gcl_unlock_trace(gcl, file, line, id);
}


/*
**  Show contents of LRU cache (for debugging)
*/

static void
check_cache(size_t klen, const void *key, void *val, va_list av)
{
	const gdp_gcl_t *gcl;
	const gdp_gcl_t *g2;

	if (val == NULL)
		return;

	gcl = val;
	LIST_FOREACH(g2, &GclsByUse, ulist)
	{
		if (g2 == gcl)
			return;
	}

	FILE *fp = va_arg(av, FILE *);
	fprintf(fp, "    ===> WARNING: %s not in usage list\n", gcl->pname);
}

void
_gdp_gcl_cache_dump(int plev, FILE *fp)
{
	gdp_gcl_t *gcl;
	gdp_gcl_t *prev_gcl = NULL;

	fprintf(fp, "\n<<< Showing cached GCLs by usage >>>\n");
	LIST_FOREACH(gcl, &GclsByUse, ulist)
	{
		// do minor sanity check on list
		// won't help if the loop is more than one item
		EP_ASSERT_ELSE(gcl != prev_gcl, break);
		prev_gcl = gcl;

		if (plev > GDP_PR_PRETTY)
		{
			_gdp_gcl_dump(gcl, fp, plev, 0);
		}
		else
		{
			struct tm *tm;
			char tbuf[40];

			if ((tm = localtime(&gcl->utime)) != NULL)
				strftime(tbuf, sizeof tbuf, "%Y%m%d-%H%M%S", tm);
			else
				snprintf(tbuf, sizeof tbuf, "%"PRIu64, (int64_t) gcl->utime);
			fprintf(fp, "%s %p %s %d\n", tbuf, gcl, gcl->pname, gcl->refcnt);
		}
		if (ep_hash_search(OpenGCLCache, sizeof gcl->name, (void *) gcl->name) == NULL)
			fprintf(fp, "    ===> WARNING: %s not in primary cache\n",
					gcl->pname);
	}

	ep_hash_forall(OpenGCLCache, check_cache);
	fprintf(fp, "\n<<< End of cached GCL list >>>\n");
}
