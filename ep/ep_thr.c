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

#include <ep.h>
#include <ep_dbg.h>
#include <ep_thr.h>
#include <stdio.h>
#include <string.h>
#include <sys/errno.h>

#if EP_OSCF_USE_PTHREADS

static EP_DBG	Dbg = EP_DBG_INIT("libep.thr", "Threading support");

bool	_EpThrUsePthreads = false;	// also used by ep_dbg_*

#if EP_OPT_EXTENDED_MUTEX_CHECK
# include <ep_string.h>
# include <sys/syscall.h>
# define gettid()		((int) syscall(SYS_gettid))
# if EP_OPT_EXTENDED_MUTEX_CHECK > 1
#  define CHECKMTX(m, e) \
    do	\
    {								\
	if (ep_dbg_test(Dbg, 98) &&				\
	    ((m)->__data.__lock > 1 ||				\
	     (m)->__data.__nusers > 1))				\
	{							\
		fprintf(stderr,					\
		    "%smutex_%s(%p): __lock=%x, __owner=%d, __nusers=%u%s\n", \
		    EpVid->vidfgred, e, m,			\
		    (m)->__data.__lock, (m)->__data.__owner,	\
		    (m)->__data.__nusers, EpVid->vidnorm);	\
	}							\
    } while (false)
# endif
#endif

#ifndef CHECKMTX
# define CHECKMTX(m, e)
#endif

#ifndef CHECKCOND
# define CHECKCOND(c, e)
#endif

/*
**  Helper routines
*/

static void
diagnose_thr_err(int err,
		const char *where,
		const char *file,
		int line,
		const char *name,
		void *p)
{
	// timed out is not unexpected, so put it at a high debug level
	if (ep_dbg_test(Dbg, err == ETIMEDOUT ? 90 : 4))
	{
		char nbuf[40];

		strerror_r(err, nbuf, sizeof nbuf);
		if (name == NULL)
			name = "???";
		ep_dbg_printf("ep_thr_%-13s: %s:%d %s (%p): %s\n",
				where, file, line, name, p, nbuf);
		ep_dbg_backtrace();
	}
	if (ep_dbg_test(Dbg, 101))
		EP_ASSERT_FAILURE("exiting on thread error");
}

# if EP_OPT_EXTENDED_MUTEX_CHECK
static void
mtx_printtrace(pthread_mutex_t *m, const char *where,
		const char *file, int line, const char *name)
{
	int my_tid = gettid();

	ep_dbg_printf("ep_thr_%-13s %s:%d %p (%s) [%d] __lock=%x __owner=%d%s\n",
			where, file, line, m, name, my_tid,
			m->__data.__lock, m->__data.__owner,
			_EpThrUsePthreads ? "" : " (ignored)");
}

static void
lock_printtrace(void *lock, const char *where,
		const char *file, int line, const char *name)
{
	int my_tid = gettid();

	ep_dbg_printf("ep_thr_%-13s %s:%d %p (%s) [%d]%s\n",
			where, file, line, lock, name, my_tid,
			_EpThrUsePthreads ? "" : " (ignored)");
}

#define TRACEMTX(m, where)	\
		if (ep_dbg_test(Dbg, 99))	\
			mtx_printtrace(m, where, file, line, name)

#else

static void
lock_printtrace(void *lock, const char *where,
		const char *file, int line, const char *name)
{
	pthread_t self = pthread_self();

	ep_dbg_printf("ep_thr_%-13s %s:%d %p (%s) [%p]%s\n",
			where, file, line, lock, name, (void *) self,
			_EpThrUsePthreads ? "" : " (ignored)");
}
#define TRACEMTX	TRACE

#endif

#define TRACE(lock, where)	\
		if (ep_dbg_test(Dbg, 99))	\
			lock_printtrace(lock, where, file, line, name)

void
_ep_thr_init(void)
{
	_EpThrUsePthreads = true;
}


/*
**  Basics
*/

int
_ep_thr_spawn(EP_THR *thidp,
		void *(*thfunc)(void *),
		void *arg,
		const char *file,
		int line)
{
	int r;

	// to make the TRACE call compile
	const char *name = NULL;

	TRACE(NULL, "spawn");
	if (!_EpThrUsePthreads)
		return EPERM;
	r = pthread_create(thidp, NULL, thfunc, arg);
	if (r != 0)
		diagnose_thr_err(errno, "spawn", file, line, NULL, NULL);
	return r;
}


void
_ep_thr_yield(const char *file, int line)
{
	// to make the TRACE call compile
	const char *name = NULL;

	TRACE(NULL, "yield");
	if (!_EpThrUsePthreads)
		return;
	if (sched_yield() < 0)
		diagnose_thr_err(errno, "yield", file, line, NULL, NULL);
}


EP_THR
ep_thr_gettid(void)
{
	return pthread_self();
}


/*
**  Mutex implementation
*/

int
_ep_thr_mutex_init(EP_THR_MUTEX *mtx, int type,
		const char *file, int line, const char *name)
{
	int err;
	pthread_mutexattr_t attr;

	if (!_EpThrUsePthreads)
		return 0;
	pthread_mutexattr_init(&attr);
	if (type == EP_THR_MUTEX_DEFAULT)
	{
		const char *mtype;

		mtype = ep_adm_getstrparam("libep.thr.mutex.type", "default");
		if (strcasecmp(mtype, "normal") == 0)
			type = PTHREAD_MUTEX_NORMAL;
		else if (strcasecmp(mtype, "errorcheck") == 0)
			type = PTHREAD_MUTEX_ERRORCHECK;
		else if (strcasecmp(mtype, "recursive") == 0)
			type = PTHREAD_MUTEX_RECURSIVE;
		else
			type = PTHREAD_MUTEX_DEFAULT;
	}
	pthread_mutexattr_settype(&attr, type);
	if ((err = pthread_mutex_init(mtx, &attr)) != 0)
		diagnose_thr_err(err, "mutex_init", file, line, name, mtx);
	pthread_mutexattr_destroy(&attr);
	TRACEMTX(mtx, "mutex_init");
	CHECKMTX(mtx, "init <<<");
	return err;
}

int
_ep_thr_mutex_destroy(EP_THR_MUTEX *mtx,
		const char *file, int line, const char *name)
{
	int err;

	TRACEMTX(mtx, "mutex_destroy");
	if (!_EpThrUsePthreads)
		return 0;
	CHECKMTX(mtx, "destroy >>>");
	if ((err = pthread_mutex_destroy(mtx)) != 0)
		diagnose_thr_err(err, "mutex_destroy", file, line, name, mtx);
	return err;
}

int
_ep_thr_mutex_lock(EP_THR_MUTEX *mtx,
		const char *file, int line, const char *name)
{
	int err;

	TRACEMTX(mtx, "mutex_lock");
	if (!_EpThrUsePthreads)
		return 0;
	CHECKMTX(mtx, "lock >>>");
#if EP_OPT_EXTENDED_MUTEX_CHECK
	if (mtx->__data.__owner == gettid() /* && !recursive */)
		ep_assert_print(file, line, "mutex %p (%s) already self-locked",
				mtx, name);
#endif
	if ((err = pthread_mutex_lock(mtx)) != 0)
		diagnose_thr_err(err, "mutex_lock", file, line, name, mtx);
	CHECKMTX(mtx, "lock <<<");
	return err;
}

int
_ep_thr_mutex_trylock(EP_THR_MUTEX *mtx,
		const char *file, int line, const char *name)
{
	int err;

	TRACEMTX(mtx, "mutex_trylock");
	if (!_EpThrUsePthreads)
		return 0;
	CHECKMTX(mtx, "trylock >>>");
	// EBUSY => mutex was already locked
	if ((err = pthread_mutex_trylock(mtx)) != 0 && err != EBUSY)
		diagnose_thr_err(err, "mutex_trylock", file, line, name, mtx);
	CHECKMTX(mtx, "trylock <<<");
	return err;
}

int
_ep_thr_mutex_unlock(EP_THR_MUTEX *mtx,
		const char *file, int line, const char *name)
{
	int err;

	TRACEMTX(mtx, "mutex_unlock");
	if (!_EpThrUsePthreads)
		return 0;
	CHECKMTX(mtx, "unlock >>>");
#if EP_OPT_EXTENDED_MUTEX_CHECK
	if (mtx->__data.__owner != gettid())
		ep_assert_print(file, line,
				"_ep_thr_mutex_unlock: mtx owner = %d, I am %d",
				mtx->__data.__owner, gettid());
#endif
	if ((err = pthread_mutex_unlock(mtx)) != 0)
		diagnose_thr_err(err, "mutex_unlock", file, line, name, mtx);
	CHECKMTX(mtx, "unlock <<<");
	return err;
}

int
_ep_thr_mutex_tryunlock(EP_THR_MUTEX *mtx,
		const char *file, int line, const char *name)
{
	int err;

	TRACE(mtx, "mutex_tryunlock");
	if (!_EpThrUsePthreads)
		return 0;
	CHECKMTX(mtx, "tryunlock >>>");
#if EP_OPT_EXTENDED_MUTEX_CHECK
	if (mtx->__data.__owner != gettid())
		ep_assert_print(file, line,
				"_ep_thr_mutex_unlock: mtx owner = %d, I am %d",
				mtx->__data.__owner, gettid());
#endif
	// EAGAIN => mutex was not locked
	// EPERM  => mutex held by a different thread
	if ((err = pthread_mutex_unlock(mtx)) != 0 &&
			err != EAGAIN && err != EPERM)
		diagnose_thr_err(err, "mutex_unlock", file, line, name, mtx);
	CHECKMTX(mtx, "tryunlock <<<");
	return err;
}


int
_ep_thr_mutex_check(EP_THR_MUTEX *mtx)
{
	CHECKMTX(mtx, "check ===");
	return 0;
}


bool
ep_thr_mutex_assert_islocked(
			EP_THR_MUTEX *m,
			const char *mstr,
			const char *file,
			int line)
{
#if ! EP_OPT_EXTENDED_MUTEX_CHECK
	return true;
#else
	if (m->__data.__lock != 0 && m->__data.__owner == gettid())
	{
		// OK, this is locked (by me)
		return true;
	}

	// oops, not locked or not locked by me
	if (m->__data.__lock == 0)
		ep_assert_print(file, line, "mutex %s (%p) is not locked (should be %d)",
				mstr, m, gettid());
	else
		ep_assert_print(file, line, "mutex %s (%p) locked by %d (should be %d)",
				mstr, m, m->__data.__owner, gettid());
	return false;
#endif
}


bool
ep_thr_mutex_assert_isunlocked(
			EP_THR_MUTEX *m,
			const char *mstr,
			const char *file,
			int line)
{
#if ! EP_OPT_EXTENDED_MUTEX_CHECK
	return true;
#else
	if (m->__data.__lock == 0)
		return true;
	ep_assert_print(file, line,
			"mutex %s (%p) is locked by %d (should be unlocked)",
			mstr, m, m->__data.__owner);
	return false;
#endif
}


bool
ep_thr_mutex_assert_i_own(
			EP_THR_MUTEX *m,
			const char *mstr,
			const char *file,
			int line)
{
#if ! EP_OPT_EXTENDED_MUTEX_CHECK
	return true;
#else
	if (m->__data.__owner == gettid())
		return true;
	ep_assert_print(file, line,
			"mutex %s (%p) is locked by %d (should be %d)",
			mstr, m, m->__data.__owner, gettid());
	return false;
#endif
}


/*
**  Condition Variable implementation
*/

int
_ep_thr_cond_init(EP_THR_COND *cv,
		const char *file, int line, const char *name)
{
	int err;

	TRACE(cv, "cond_init");
	if (!_EpThrUsePthreads)
		return 0;
	if ((err = pthread_cond_init(cv, NULL)) != 0)
		diagnose_thr_err(err, "cond_init", file, line, name, cv);
	CHECKCOND(cv, "init <<<");
	return err;
}

int
_ep_thr_cond_destroy(EP_THR_COND *cv,
		const char *file, int line, const char *name)
{
	int err;

	TRACE(cv, "cond_destroy");
	if (!_EpThrUsePthreads)
		return 0;
	CHECKCOND(cv, "destroy >>>");
	if ((err = pthread_cond_destroy(cv)) != 0)
		diagnose_thr_err(err, "cond_destroy", file, line, name, cv);
	return err;
}

int
_ep_thr_cond_signal(EP_THR_COND *cv,
		const char *file, int line, const char *name)
{
	int err;

	TRACE(cv, "cond_signal");
	if (!_EpThrUsePthreads)
		return 0;
	CHECKCOND(cv, "signal >>>");
	if ((err = pthread_cond_signal(cv)) != 0)
		diagnose_thr_err(err, "cond_signal", file, line, name, cv);
	CHECKCOND(cv, "signal <<<");
	return err;
}

int
_ep_thr_cond_wait(EP_THR_COND *cv, EP_THR_MUTEX *mtx, EP_TIME_SPEC *timeout,
		const char *file, int line, const char *name)
{
	int err;

	TRACE(cv, "cond_wait-cv");
	TRACEMTX(mtx, "cond-wait-mtx");
	if (!_EpThrUsePthreads)
		return 0;
	CHECKMTX(mtx, "wait >>>");
	CHECKCOND(cv, "wait >>>");
	if (timeout == NULL)
	{
		err = pthread_cond_wait(cv, mtx);
	}
	else
	{
		struct timespec ts;
		ts.tv_sec = timeout->tv_sec;
		ts.tv_nsec = timeout->tv_nsec;
		err = pthread_cond_timedwait(cv, mtx, &ts);
	}
	if (err != 0)
		diagnose_thr_err(err, "cond_wait", file, line, name, cv);
	CHECKMTX(mtx, "wait <<<");
	CHECKCOND(cv, "wait <<<");
	return err;
}

int
_ep_thr_cond_broadcast(EP_THR_COND *cv,
		const char *file, int line, const char *name)
{
	int err;

	TRACE(cv, "cond_broadcast");
	if (!_EpThrUsePthreads)
		return 0;
	CHECKCOND(cv, "broadcast >>>");
	if ((err = pthread_cond_broadcast(cv)) != 0)
		diagnose_thr_err(err, "cond_broadcast", file, line, name, cv);
	CHECKCOND(cv, "broadcast <<<");
	return err;
}


/*
**  Read/Write Lock implementation
*/

int
_ep_thr_rwlock_init(EP_THR_RWLOCK *rwl,
		const char *file, int line, const char *name)
{
	int err;

	TRACE(rwl, "rwlock_init");
	if (!_EpThrUsePthreads)
		return 0;
	if ((err = pthread_rwlock_init(rwl, NULL)) != 0)
		diagnose_thr_err(err, "rwlock_init", file, line, name, rwl);
	return err;
}

int
_ep_thr_rwlock_destroy(EP_THR_RWLOCK *rwl,
		const char *file, int line, const char *name)
{
	int err;

	TRACE(rwl, "rwlock_destroy");
	if (!_EpThrUsePthreads)
		return 0;
	if ((err = pthread_rwlock_destroy(rwl)) != 0)
		diagnose_thr_err(err, "rwlock_destroy", file, line, name, rwl);
	return err;
}

int
_ep_thr_rwlock_rdlock(EP_THR_RWLOCK *rwl,
		const char *file, int line, const char *name)
{
	int err;

	TRACE(rwl, "rwlock_rdlock");
	if (!_EpThrUsePthreads)
		return 0;
	if ((err = pthread_rwlock_rdlock(rwl)) != 0)
		diagnose_thr_err(err, "rwlock_rdlock", file, line, name, rwl);
	return err;
}

int
_ep_thr_rwlock_tryrdlock(EP_THR_RWLOCK *rwl,
		const char *file, int line, const char *name)
{
	int err;

	TRACE(rwl, "rwlock_tryrdlock");
	if (!_EpThrUsePthreads)
		return 0;
	if ((err = pthread_rwlock_tryrdlock(rwl)) != 0)
		diagnose_thr_err(err, "rwlock_tryrdlock", file, line, name, rwl);
	return err;
}

int
_ep_thr_rwlock_wrlock(EP_THR_RWLOCK *rwl,
		const char *file, int line, const char *name)
{
	int err;

	TRACE(rwl, "rwlock_wrlock");
	if (!_EpThrUsePthreads)
		return 0;
	if ((err = pthread_rwlock_wrlock(rwl)) != 0)
		diagnose_thr_err(err, "rwlock_wrlock", file, line, name, rwl);
	return err;
}

int
_ep_thr_rwlock_trywrlock(EP_THR_RWLOCK *rwl,
		const char *file, int line, const char *name)
{
	int err;

	TRACE(rwl, "rwlock_tryrwlock");
	if (!_EpThrUsePthreads)
		return 0;
	if ((err = pthread_rwlock_trywrlock(rwl)) != 0)
		diagnose_thr_err(err, "rwlock_trywrlock", file, line, name, rwl);
	return err;
}

int
_ep_thr_rwlock_unlock(EP_THR_RWLOCK *rwl,
		const char *file, int line, const char *name)
{
	int err;

	TRACE(rwl, "rwlock_unlock");
	if (!_EpThrUsePthreads)
		return 0;
	if ((err = pthread_rwlock_unlock(rwl)) != 0)
		diagnose_thr_err(err, "rwlock_unlock", file, line, name, rwl);
	return err;
}

#else // !EP_OSCF_USE_PTHREADS

void
_ep_thr_init(void)
{
	ep_dbg_printf("WARNING: initializing pthreads, "
			"but pthreads compiled out\n");
}

#endif // EP_OSCF_USE_PTHREADS
