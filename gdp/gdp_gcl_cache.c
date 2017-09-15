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
#include <openssl/md4.h>

#include <errno.h>
#include <string.h>

static EP_DBG	Dbg = EP_DBG_INIT("gdp.gcl.cache", "GCL cache");



/***********************************************************************
**
**	GCL Caching
**		Return the internal representation of the GCL from
**		the name.  These are not really intended for public use,
**		but they are shared with gdplogd.
**
**		Getting lock ordering right here is a pain.
**
***********************************************************************/

static EP_THR_MUTEX		GclCacheMutex;		// locks _OpenGCLCache and GclsByUse
EP_HASH					*_OpenGCLCache;		// associative cache
LIST_HEAD(gcl_use_head, gdp_gcl)			// LRU cache
						GclsByUse		= LIST_HEAD_INITIALIZER(GclByUse);


/*
**  Initialize the GCL cache
*/

EP_STAT
_gdp_gcl_cache_init(void)
{
	EP_STAT estat = EP_STAT_OK;
	int istat;
	const char *err;

	if (_OpenGCLCache == NULL)
	{
		istat = ep_thr_mutex_init(&GclCacheMutex, EP_THR_MUTEX_DEFAULT);
		if (istat != 0)
		{
			estat = ep_stat_from_errno(istat);
			err = "could not initialize GclCacheMutex";
			goto fail0;
		}
		ep_thr_mutex_setorder(&GclCacheMutex, GDP_MUTEX_LORDER_GCLCACHE);

		_OpenGCLCache = ep_hash_new("_OpenGCLCache", NULL, 0);
		if (_OpenGCLCache == NULL)
		{
			estat = ep_stat_from_errno(errno);
			err = "could not create OpenGCLCache";
			goto fail0;
		}
	}

#if EP_OSCF_USE_VALGRIND
	{
		gdp_gcl_t *gcl;

		// establish lock ordering for valgrind
		ep_thr_mutex_lock(&GclCacheMutex);
		estat = _gdp_gcl_newhandle(NULL, &gcl);
		if (EP_STAT_ISOK(estat))
		{
			_gdp_gcl_lock(gcl);
			_gdp_gcl_freehandle(gcl);
		}
		ep_thr_mutex_unlock(&GclCacheMutex);
	}
#endif

	// Nothing to do for LRU cache

	if (false)
	{
fail0:
		if (EP_STAT_ISOK(estat))
			estat = EP_STAT_ERROR;
		ep_log(estat, "gdp_gcl_cache_init: %s", err);
		ep_app_fatal("gdp_gcl_cache_init: %s", err);
	}

	return estat;
}

#define LOG2_BLOOM_SIZE		10			// log base 2 of bloom filter size


/*
**  Helper routine checking that associative cache matches the LRU cache.
**
**		Unfortunately, this is O(n^2).  The bloom filter is less
**		useful here.
**		This is a helper routine called from ep_hash_forall.
*/

static void
check_cache_helper(size_t klen, const void *key, void *val, va_list av)
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


/*
** Check cache consistency.
**		Returns true if is OK, false if is not.
*/

static bool
check_cache_consistency(const char *where)
{
	bool rval = true;
	gdp_gcl_t *gcl;
	uint32_t bloom[1 << (LOG2_BLOOM_SIZE - 1)];		// bloom filter
	union
	{
		uint8_t		md4out[MD4_DIGEST_LENGTH];		// output md4
		uint32_t	md4eq[MD4_DIGEST_LENGTH / 4];
	} md4buf;

	memset(&md4buf, 0, sizeof md4buf);

	EP_THR_MUTEX_ASSERT_ISLOCKED(&GclCacheMutex);
	LIST_FOREACH(gcl, &GclsByUse, ulist)
	{
		// check for loops, starting with quick bloom filter on address
		uint32_t bmask;
		int bindex;

		VALGRIND_HG_DISABLE_CHECKING(gcl, sizeof *gcl);

		MD4((unsigned char *) &gcl, sizeof gcl, md4buf.md4out);
		bindex = md4buf.md4eq[0];			// just use first 32-bit word
		bmask = 1 << (bindex & 0x1f);		// mask on single word
		bindex = (bindex >> 4) & ((1 << (LOG2_BLOOM_SIZE - 1)) - 1);
		if (EP_UT_BITSET(bmask, bloom[bindex]))
		{
			// may have a conflict --- do explicit check
			gdp_gcl_t *g2 = LIST_NEXT(gcl, ulist);
			while (g2 != NULL && g2 != gcl)
				g2 = LIST_NEXT(g2, ulist);
			if (g2 != NULL)
			{
				EP_ASSERT_PRINT("Loop in GclsByUse on %p at %s",
						gcl, where);
				_gdp_gcl_dump(gcl, ep_dbg_getfile(), GDP_PR_DETAILED, 0);
				rval = false;
				VALGRIND_HG_ENABLE_CHECKING(gcl, sizeof *gcl);
				break;
			}
		}
		bloom[bindex] |= bmask;
		VALGRIND_HG_ENABLE_CHECKING(gcl, sizeof *gcl);
	}

	ep_hash_forall(_OpenGCLCache, check_cache_helper, ep_dbg_getfile());
	return rval;
}


/*
**  Rebuild GCL LRU list from the associative hash table.
**		Which one is the definitive version?
**
**		Should "never happen", but if it does all hell breaks loose.
**		Yes, I know, this is O(n^2).  But it should never happen.
**		So sue me.
*/

// helper routine
static void
sorted_insert(size_t klen, const void *key, void *gcl_, va_list av)
{
	gdp_gcl_t *gcl = gcl_;

	// if this is being dropped, just skip this GCL
	if (EP_UT_BITSET(GCLF_DROPPING, gcl->flags))
		return;

	gdp_gcl_t *g2 = LIST_FIRST(&GclsByUse);
	if (g2 == NULL || gcl->utime > g2->utime)
	{
		// new entry is younger than first entry
		LIST_INSERT_HEAD(&GclsByUse, gcl, ulist);
		return;
	}
	LIST_FOREACH(g2, &GclsByUse, ulist)
	{
		gdp_gcl_t *g3 = LIST_NEXT(g2, ulist);
		if (g3 == NULL || gcl->utime > g3->utime)
		{
			LIST_INSERT_AFTER(g2, gcl, ulist);
			return;
		}
	}

	// shouldn't happen
	EP_ASSERT_PRINT("sorted_insert: ran off end of GclsByUse, gcl = %p", gcl);
}

static void
rebuild_lru_list(void)
{
	ep_dbg_cprintf(Dbg, 2, "rebuild_lru_list: rebuilding\n");
	ep_thr_mutex_lock(&GclCacheMutex);
	LIST_INIT(&GclsByUse);
	ep_hash_forall(_OpenGCLCache, sorted_insert);
	ep_thr_mutex_unlock(&GclCacheMutex);
}




/*
**  Add a GCL to both the associative and the LRU caches.
**  The "unlocked" refers to GclCacheMutex, which should already
**  be locked on entry.  The GCL is also expected to be locked.
*/

static void
add_cache_unlocked(gdp_gcl_t *gcl)
{
	ep_dbg_cprintf(Dbg, 49, "_gdp_gcl_cache_add(%p): adding\n", gcl);

	// sanity checks
	EP_ASSERT_ELSE(GDP_GCL_ISGOOD(gcl), return);
	if (!GDP_GCL_ASSERT_ISLOCKED(gcl))
		return;
	EP_THR_MUTEX_ASSERT_ISLOCKED(&GclCacheMutex);

	check_cache_consistency("add_cache_unlocked");

	if (EP_UT_BITSET(GCLF_INCACHE, gcl->flags))
	{
		ep_dbg_cprintf(Dbg, 9,
				"_gdp_gcl_cache_add(%p): already cached\n",
				gcl);
		return;
	}

	// save it in the associative cache
	gdp_gcl_t *g2;
	ep_dbg_cprintf(Dbg, 49,
			"_gdp_gcl_cache_add(%p): insert into _OpenGCLCache\n",
			gcl);
	g2 = ep_hash_insert(_OpenGCLCache, sizeof (gdp_name_t), gcl->name, gcl);
	if (g2 != NULL)
	{
		EP_ASSERT_PRINT("duplicate GCL cache entry, gcl=%p (%s)",
				gcl, gcl->pname);
		fprintf(stderr, "New ");
		_gdp_gcl_dump(gcl, stderr, GDP_PR_DETAILED, 0);
		fprintf(stderr, "Existing ");
		_gdp_gcl_dump(g2, stderr, GDP_PR_DETAILED, 0);
		// we don't free g2 in case someone else has the pointer
	}

	// ... and the LRU list
	{
		struct timeval tv;

		gettimeofday(&tv, NULL);
		gcl->utime = tv.tv_sec;

		ep_dbg_cprintf(Dbg, 49, "_gdp_gcl_cache_add(%p): insert into LRU list\n",
				gcl);
		IF_LIST_CHECK_OK(&GclsByUse, gcl, ulist, gdp_gcl_t)
		{
			LIST_INSERT_HEAD(&GclsByUse, gcl, ulist);
		}
	}

	gcl->flags |= GCLF_INCACHE;
	ep_dbg_cprintf(Dbg, 40, "_gdp_gcl_cache_add: %s => %p\n",
			gcl->pname, gcl);
}


/*
**	Wrapper for add_cache_unlocked that takes care of locking
**	GclCacheMutex.  Since that *must* be locked before the GCL,
**	we have to unlock the GCL before we lock GclCache.
**	This should be OK since presumably the GCL is not in the
**	cache and hence not accessible to other threads.
*/

void
_gdp_gcl_cache_add(gdp_gcl_t *gcl)
{
	EP_THR_MUTEX_ASSERT_ISLOCKED(&gcl->mutex);
	_gdp_gcl_unlock(gcl);
	ep_thr_mutex_lock(&GclCacheMutex);
	_gdp_gcl_lock(gcl);
	add_cache_unlocked(gcl);
	ep_thr_mutex_unlock(&GclCacheMutex);
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
	check_cache_consistency("_gdp_gcl_cache_changename");
	(void) ep_hash_delete(_OpenGCLCache, sizeof (gdp_name_t), gcl->name);
	(void) memcpy(gcl->name, newname, sizeof (gdp_name_t));
	(void) ep_hash_insert(_OpenGCLCache, sizeof (gdp_name_t), newname, gcl);
	ep_thr_mutex_unlock(&GclCacheMutex);

	ep_dbg_cprintf(Dbg, 40, "_gdp_gcl_cache_changename: %s => %p\n",
					gcl->pname, gcl);
}


/*
**  _GDP_GCL_CACHE_GET --- get a GCL from the cache, if it exists
**
**		Searches for a specific GCL.  If found it is returned; if not,
**		it returns null unless the GGCF_CREATE flag is set, in which
**		case it is created and returned.  This allows the Cache Mutex
**		to be locked before the GCL is locked.  Newly created GCLs
**		are marked GCLF_PENDING unless the open routine clears that bit.
**		In particular, gdp_gcl_open needs to do additional opening
**		_after_ _gdp_gcl_cache_get returns so that the cache is
**		unlocked while sending protocol to the server.
**
**		The GCL is returned locked and with its reference count
**		incremented.  It is up to the caller to call _gdp_gcl_decref
**		on the GCL when it is done with it.
*/

EP_STAT
_gdp_gcl_cache_get(
			gdp_name_t gcl_name,
			gdp_iomode_t iomode,
			uint32_t flags,
			gdp_gcl_t **pgcl)
{
	gdp_gcl_t *gcl;
	EP_STAT estat = EP_STAT_OK;

	ep_thr_mutex_lock(&GclCacheMutex);
	if (!check_cache_consistency("_gdp_gcl_cache_get"))
		rebuild_lru_list();

	// see if we have a pointer to this GCL in the cache
	gcl = ep_hash_search(_OpenGCLCache, sizeof (gdp_name_t), (void *) gcl_name);
	if (gcl != NULL)
	{
		_gdp_gcl_lock(gcl);

		// sanity checking:
		// someone may have snuck in before we acquired the lock
		if (!EP_UT_BITSET(GCLF_INUSE, gcl->flags) ||
				!EP_UT_BITSET(GCLF_INCACHE, gcl->flags) ||
				EP_UT_BITSET(GCLF_DROPPING, gcl->flags) ||
				(EP_UT_BITSET(GCLF_PENDING, gcl->flags) &&
				 !EP_UT_BITSET(GGCF_GET_PENDING, flags)))
		{
			// someone deallocated this in the brief window above
			if (ep_dbg_test(Dbg, 10))
			{
				ep_dbg_printf("_gdp_gcl_cache_get: abandoning ");
				_gdp_gcl_dump(gcl, ep_dbg_getfile(), GDP_PR_BASIC, 0);
			}
			_gdp_gcl_unlock(gcl);
			gcl = NULL;
		}
		else if (!EP_UT_BITSET(_GDP_MODE_PEEK, iomode))
		{
			_gdp_gcl_incref(gcl);
			_gdp_gcl_touch(gcl);
			gcl->iomode |= iomode & GDP_MODE_MASK;
		}
	}
	if (gcl == NULL)
	{
		if (!EP_UT_BITSET(GGCF_CREATE, flags))
			goto done;

		// create a new one
		estat = _gdp_gcl_newhandle(gcl_name, &gcl);
		EP_STAT_CHECK(estat, goto fail0);
		_gdp_gcl_lock(gcl);
		add_cache_unlocked(gcl);
	}

done:
	if (ep_dbg_test(Dbg, 42))
	{
		gdp_pname_t pname;

		gdp_printable_name(gcl_name, pname);
		if (gcl == NULL)
			ep_dbg_printf("_gdp_gcl_cache_get: %s => NULL\n", pname);
		else
			ep_dbg_printf("_gdp_gcl_cache_get: %s =>\n\t%p refcnt %d\n",
						pname, gcl, gcl->refcnt);
	}
fail0:
	if (EP_STAT_ISOK(estat))
		*pgcl = gcl;
	ep_thr_mutex_unlock(&GclCacheMutex);
	return estat;
}


/*
** Drop a GCL from both the associative and the LRU caches
**
*/

void
_gdp_gcl_cache_drop(gdp_gcl_t *gcl, bool cleanup)
{
	EP_ASSERT_ELSE(gcl != NULL, return);
	if (!EP_ASSERT(GDP_GCL_ISGOOD(gcl)))
	{
		// GCL is in some random state --- we need the name at least
		// (this may crash)
		EP_ASSERT_ELSE(gdp_name_is_valid(gcl->name), return);
	}

	GDP_GCL_ASSERT_ISLOCKED(gcl);
	if (!EP_ASSERT(EP_UT_BITSET(GCLF_INCACHE, gcl->flags)))
		return;
	if (cleanup)
	{
		EP_THR_MUTEX_ASSERT_ISLOCKED(&GclCacheMutex);
		check_cache_consistency("_gdp_gcl_cache_drop");
	}
	else
	{
		EP_THR_MUTEX_ASSERT_ISUNLOCKED(&GclCacheMutex);
	}

	// error if we're dropping something that's referenced from the cache
	if (gcl->refcnt != 0)
	{
		ep_log(GDP_STAT_BAD_REFCNT, "_gdp_gcl_cache_drop: ref count %d != 0",
				gcl->refcnt);
		if (ep_dbg_test(Dbg, 1))
			_gdp_gcl_dump(gcl, ep_dbg_getfile(), GDP_PR_BASIC, 0);
	}

	// mark it as being dropped to detect race condition
	gcl->flags |= GCLF_DROPPING;

	// if we're not cleanup up (GclCacheMutex unlocked) we have to
	// get the lock ordering right
	if (!cleanup)
	{
		// now lock the cache and then re-lock the GCL
		_gdp_gcl_unlock(gcl);
		ep_thr_mutex_lock(&GclCacheMutex);
		check_cache_consistency("_gdp_gcl_cache_drop");
		_gdp_gcl_lock(gcl);

		// sanity checks (XXX should these be assertions? XXX)
		//XXX need to check that nothing bad happened while GCL was unlocked
		EP_ASSERT(EP_UT_BITSET(GCLF_INCACHE, gcl->flags));
	}

	// remove it from the associative cache
	(void) ep_hash_delete(_OpenGCLCache, sizeof (gdp_name_t), gcl->name);

	// ... and the LRU list
	LIST_REMOVE(gcl, ulist);
	gcl->flags &= ~GCLF_INCACHE;

	if (!cleanup)
	{
		// now we can unlock the cache, but leave the GCL locked
		ep_thr_mutex_unlock(&GclCacheMutex);
	}

	ep_dbg_cprintf(Dbg, 40, "_gdp_gcl_cache_drop: %s => %p\n",
			gcl->pname, gcl);
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

	ep_dbg_cprintf(Dbg, 46, "_gdp_gcl_touch(%p)\n", gcl);

	gettimeofday(&tv, NULL);
	gcl->utime = tv.tv_sec;

	if (!GDP_GCL_ASSERT_ISLOCKED(gcl))
	{
		// GCL isn't locked: do nothing
	}
	else if (!EP_UT_BITSET(GCLF_INCACHE, gcl->flags))
	{
		// GCL isn't in cache: do nothing
	}
	else
	{
		// both locked and in cache
		EP_THR_MUTEX_ASSERT_ISLOCKED(&GclCacheMutex);
		check_cache_consistency("_gdp_gcl_touch");
		LIST_REMOVE(gcl, ulist);
		IF_LIST_CHECK_OK(&GclsByUse, gcl, ulist, gdp_gcl_t)
			LIST_INSERT_HEAD(&GclsByUse, gcl, ulist);
	}
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

#define CACHE_BLOOM_SIZE		(16 * 1024)
void
_gdp_gcl_cache_reclaim(time_t maxage)
{
	static int headroom = 0;
	static long maxgcls;				// maximum number of GCLs in one pass

	ep_dbg_cprintf(Dbg, 68, "_gdp_gcl_cache_reclaim(maxage = %ld)\n", maxage);

	// collect some parameters (once only)
	if (headroom == 0)
	{
		headroom = ep_adm_getintparam("swarm.gdp.cache.fd.headroom", 0);
		if (headroom <= 0)
		{
			int maxfds;
			(void) ep_app_numfds(&maxfds);
			headroom = maxfds - ((maxfds * 2) / 3);
			if (headroom == 0)
				headroom = 8;
		}
	}

	if (maxgcls <= 0)
	{
		maxgcls = ep_adm_getlongparam("swarm.gdp.cache.reclaim.maxgcls", 100000);
	}

	for (;;)
	{
		struct timeval tv;
		gdp_gcl_t *g1, *g2;
		time_t mintime;
		long loopcount = 0;

		gettimeofday(&tv, NULL);
		mintime = tv.tv_sec - maxage;

		ep_thr_mutex_lock(&GclCacheMutex);
		if (!check_cache_consistency("_gdp_gcl_cache_reclaim"))
			rebuild_lru_list();
		for (g1 = LIST_FIRST(&GclsByUse); g1 != NULL; g1 = g2)
		{
			if (loopcount++ > maxgcls)
			{
				EP_ASSERT_PRINT("_gdp_gcl_cache_reclaim: processed %ld "
								"GCLS (giving up)",
							maxgcls);
				break;
			}
			g2 = LIST_NEXT(g1, ulist);		// do as early as possible

			if (ep_thr_mutex_trylock(&g1->mutex) != 0)
			{
				// couldn't get the lock: previous g2 setting may be wrong
				continue;
			}
			g1->flags |= GCLF_ISLOCKED;
			g2 = LIST_NEXT(g1, ulist);		// get g2 again with the lock
			if (g1->utime > mintime)
			{
				_gdp_gcl_unlock(g1);
				continue;
			}
			if (EP_UT_BITSET(GCLF_DROPPING, g1->flags) || g1->refcnt > 0)
			{
				if (ep_dbg_test(Dbg, 19))
				{
					ep_dbg_printf("_gdp_gcl_cache_reclaim: skipping %s:\n   ",
							EP_UT_BITSET(GCLF_DROPPING, g1->flags) ?
								"dropping" : "referenced");
					_gdp_gcl_dump(g1, ep_dbg_getfile(), GDP_PR_DETAILED, 0);
				}
				_gdp_gcl_unlock(g1);
				continue;
			}

			// OK, we really want to drop this GCL
			if (ep_dbg_test(Dbg, 32))
			{
				ep_dbg_printf("_gdp_gcl_cache_reclaim: reclaiming:\n   ");
				_gdp_gcl_dump(g1, ep_dbg_getfile(), GDP_PR_DETAILED, 0);
			}

			// remove from the LRU list and the name->handle cache
			_gdp_gcl_cache_drop(g1, true);

			// release memory (this will also unlock the corpse)
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

	// free all GCLs and all reqs linked to them
	// can give locking errors in some circumstances
	for (g1 = LIST_FIRST(&GclsByUse); g1 != NULL; g1 = g2)
	{
		ep_thr_mutex_trylock(&g1->mutex);
		g1->flags |= GCLF_ISLOCKED;
		g2 = LIST_NEXT(g1, ulist);
		_gdp_req_freeall(g1, shutdownfunc);
		_gdp_gcl_freehandle(g1);	// also removes from cache and usage list
	}
}


void
_gdp_gcl_cache_dump(int plev, FILE *fp)
{
	gdp_gcl_t *gcl;
	int ngcls = 0;
	int maxgcls = 40;		//XXX should be a parameter

	if (fp == NULL)
		fp = ep_dbg_getfile();

	fprintf(fp, "\n<<< Showing cached GCLs by usage >>>\n");
	LIST_FOREACH(gcl, &GclsByUse, ulist)
	{
		if (++ngcls > maxgcls)
			break;
		VALGRIND_HG_DISABLE_CHECKING(gcl, sizeof *gcl);

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
		if (ep_hash_search(_OpenGCLCache, sizeof gcl->name, (void *) gcl->name) == NULL)
			fprintf(fp, "    ===> WARNING: %s not in primary cache\n",
					gcl->pname);
		VALGRIND_HG_ENABLE_CHECKING(gcl, sizeof *gcl);
	}
	if (gcl == NULL)
		fprintf(fp, "\n<<< End of cached GCL list >>>\n");
	else
		fprintf(fp, "\n<<< End of cached GCL list (only %d of %d printed >>>\n",
				ngcls, maxgcls);
}


/*
**  Do a pass over all known GCLs.  Used for reclamation.
*/

void
_gdp_gcl_cache_foreach(void (*f)(gdp_gcl_t *))
{
	gdp_gcl_t *g1;
	gdp_gcl_t *g2;

	ep_thr_mutex_lock(&GclCacheMutex);
	for (g1 = LIST_FIRST(&GclsByUse); g1 != NULL; g1 = g2)
	{
		g2 = LIST_NEXT(g1, ulist);
		(*f)(g1);
	}
	ep_thr_mutex_unlock(&GclCacheMutex);
}
