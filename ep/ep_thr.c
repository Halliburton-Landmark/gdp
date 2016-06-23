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
#include <ep_dbg.h>
#include <ep_thr.h>
#include <stdio.h>
#include <string.h>
#include <sys/errno.h>

#if EP_OSCF_USE_PTHREADS

static EP_DBG	Dbg = EP_DBG_INIT("libep.thr", "Threading support");

bool	_EpThrUsePthreads = false;	// also used by ep_dbg_*

#if EP_OPT_EXTENDED_MUTEX_CHECK
#include <ep_string.h>
#define CHECKMTX(m, e) \
    do	\
    {								\
	if (m->__data.__owner < 0 ||				\
	    m->__data.__lock > 1 || m->__data.__nusers > 1)	\
		fprintf(stderr,					\
		    "%smutex_%s(%p): __lock=%d, __owner=%d, __nusers=%d%s\n",	\
		    EpVid->vidfgred, e, m,			\
		    m->__data.__lock, m->__data.__owner,	\
		    m->__data.__nusers,	EpVid->vidnorm);	\
    } while (false)
#define CHECKCOND(c, e)							\
    do									\
    {									\
	if (ep_dbg_test(Dbg, 10))					\
	    fprintf(stderr,						\
		    "%scond_%s(%p): __lock=%d, __futex=%d, __nwaiters=%d%s\n",	\
		    EpVid->vidfgred, e, c,				\
		    c->__data.__lock, c->__data.__futex,		\
		    c->__data.__nwaiters, EpVid->vidnorm);		\
    } while (false)
#endif

#ifndef CHECKMTX
# define CHECKMTX(m, e)
# define CHECKCOND(c, e)
#endif

/*
**  Helper routines
*/

static void
diagnose_thr_err(int err, const char *where)
{
	// timed out is not unexpected, so put it at a high debug level
	if (ep_dbg_test(Dbg, err == ETIMEDOUT ? 90 : 4))
	{
		char nbuf[40];

		strerror_r(err, nbuf, sizeof nbuf);
		ep_dbg_printf("ep_thr_%s: %s\n", where, nbuf);
	}
	if (ep_dbg_test(Dbg, 101))
		EP_ASSERT_FAILURE("exiting on thread error");
}

static void
printtrace(void *lock, const char *where,
		const char *file, int line, const char *name)
{
	pthread_t self = pthread_self();

	ep_dbg_printf("ep_thr_%s %s:%d %p (%s) [%p]%s\n",
			where, file, line, lock, name, self,
			_EpThrUsePthreads ? "" : " (ignored)");
}

#define TRACE(lock, where)	\
		if (ep_dbg_test(Dbg, 99))	\
			printtrace(lock, where, file, line, name)

void
_ep_thr_init(void)
{
	_EpThrUsePthreads = true;
}


/*
**  Basics
*/

int
ep_thr_spawn(EP_THR *thidp, void *(*thfunc)(void *), void *arg)
{
	int r;

	// to make it compile
	const char *file = NULL;
	int line = 0;
	const char *name = NULL;

	TRACE(NULL, "spawn");
	if (!_EpThrUsePthreads)
		return EPERM;
	r = pthread_create(thidp, NULL, thfunc, arg);
	if (r != 0)
		diagnose_thr_err(errno, "spawn");
	return r;
}


void
ep_thr_yield(void)
{
	// to make it compile
	const char *file = NULL;
	int line = 0;
	const char *name = NULL;

	TRACE(NULL, "yield");
	if (!_EpThrUsePthreads)
		return;
	if (sched_yield() < 0)
		diagnose_thr_err(errno, "yield");
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

	TRACE(mtx, "mutex_init");
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
		diagnose_thr_err(err, "mutex_init");
	pthread_mutexattr_destroy(&attr);
	CHECKMTX(mtx, "init <<<");
	return err;
}

int
_ep_thr_mutex_destroy(EP_THR_MUTEX *mtx,
		const char *file, int line, const char *name)
{
	int err;

	TRACE(mtx, "mutex_destroy");
	if (!_EpThrUsePthreads)
		return 0;
	CHECKMTX(mtx, "destroy >>>");
	if ((err = pthread_mutex_destroy(mtx)) != 0)
		diagnose_thr_err(err, "mutex_destroy");
	return err;
}

int
_ep_thr_mutex_lock(EP_THR_MUTEX *mtx,
		const char *file, int line, const char *name)
{
	int err;

	TRACE(mtx, "mutex_lock");
	if (!_EpThrUsePthreads)
		return 0;
	CHECKMTX(mtx, "lock >>>");
	if ((err = pthread_mutex_lock(mtx)) != 0)
		diagnose_thr_err(err, "mutex_lock");
	CHECKMTX(mtx, "lock <<<");
	return err;
}

int
_ep_thr_mutex_trylock(EP_THR_MUTEX *mtx,
		const char *file, int line, const char *name)
{
	int err;

	TRACE(mtx, "mutex_trylock");
	if (!_EpThrUsePthreads)
		return 0;
	CHECKMTX(mtx, "trylock >>>");
	if ((err = pthread_mutex_trylock(mtx)) != 0)
	{
		// ignore "resource busy" and "resource deadlock avoided" errors
		// (EDEADLK should only occur if libep.thr.mutex.type=errorcheck)
		if (err != EBUSY && err != EDEADLK)
			diagnose_thr_err(err, "mutex_trylock");
	}
	CHECKMTX(mtx, "trylock <<<");
	return err;
}

int
_ep_thr_mutex_unlock(EP_THR_MUTEX *mtx,
		const char *file, int line, const char *name)
{
	int err;

	TRACE(mtx, "mutex_unlock");
	if (!_EpThrUsePthreads)
		return 0;
	CHECKMTX(mtx, "unlock >>>");
	if ((err = pthread_mutex_unlock(mtx)) != 0)
		diagnose_thr_err(err, "mutex_unlock");
	CHECKMTX(mtx, "unlock <<<");
	return err;
}

int
_ep_thr_mutex_check(EP_THR_MUTEX *mtx)
{
	CHECKMTX(mtx, "check ===");
	return 0;
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
		diagnose_thr_err(err, "cond_init");
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
		diagnose_thr_err(err, "cond_destroy");
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
		diagnose_thr_err(err, "cond_signal");
	CHECKCOND(cv, "signal <<<");
	return err;
}

int
_ep_thr_cond_wait(EP_THR_COND *cv, EP_THR_MUTEX *mtx, EP_TIME_SPEC *timeout,
		const char *file, int line, const char *name)
{
	int err;

	TRACE(cv, "cond_wait-cv");
	TRACE(mtx, "cond-wait-mtx");
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
		diagnose_thr_err(err, "cond_wait");
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
		diagnose_thr_err(err, "cond_broadcast");
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
		diagnose_thr_err(err, "rwlock_init");
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
		diagnose_thr_err(err, "rwlock_destroy");
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
		diagnose_thr_err(err, "rwlock_rdlock");
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
		diagnose_thr_err(err, "rwlock_tryrdlock");
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
		diagnose_thr_err(err, "rwlock_wrlock");
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
		diagnose_thr_err(err, "rwlock_trywrlock");
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
		diagnose_thr_err(err, "rwlock_unlock");
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
