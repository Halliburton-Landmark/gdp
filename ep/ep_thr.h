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

#ifndef _EP_THR_H_
#define _EP_THR_H_

# include <ep/ep.h>
# include <ep/ep_time.h>

# if EP_OSCF_USE_PTHREADS
#  include <pthread.h>

/*
**  THREADS LOCKING SUPPORT
**
**  These are mostly one-for-one replacements for pthreads.
**  The main differences are:
**
**  (a)	They can be compiled out if you don't have pthreads available.
**	By default they are compiled in, but "-DEP_OSCF_USE_PTHREADS=0"
**	will undo this.
**  (b) The routines themselves have debugging output in the event of
**	errors.
**  (c)	These definitions only cover locking.  The assumption is that
**	if you need threads then you'll do all the thread creation/
**	destruction/control using native pthreads primitives.  In fact,
**	these routines are probably only useful for libraries.
**  (d) It is essential that the application call
**		ep_lib_init(EP_LIB_USEPTHREADS);
**	Otherwise these routines are all null.
**
**  This could be made slightly more efficient by changing all calls
**  to "ep_thr_XYZ" to (_EpThrUsePthreads ? _ep_thr_XYZ : 0).  It's
**  not clear that it's worth the complexity, but it's easy to do in
**  the future if it is.
**
**  XXX	Should these return an EP_STAT instead of an int?
**	PRO:	matches other routines, allows for possible non-errno
**		values in the future.
**	CON:	makes the ep_thr_* routines not drop-in replacements
**		for pthreads_* routines.
**	NOTE:	There's not much to do if these routines fail (kind
**		of like malloc), so unfortunately the return values
**		are usually ignored.  Just live with it?
**
**  XXX	I started implementing the mutex routines as recursive locks,
**	mostly so ep_funclist_invoke would work.  This is how the
**	stdio libraries deal with the problem.  Problem is, that
**	wouldn't fix the problem (because of local variables), so I
**	went for simplicity.
*/

extern void	ep_thr_yield(void);

typedef pthread_mutex_t		EP_THR_MUTEX;
#  define	EP_THR_MUTEX_INITIALIZER	= PTHREAD_MUTEX_INITIALIZER
extern int	ep_thr_mutex_init(EP_THR_MUTEX *mtx, int type);
extern int	ep_thr_mutex_destroy(EP_THR_MUTEX *mtx);
extern int	ep_thr_mutex_lock(EP_THR_MUTEX *mtx);
extern int	ep_thr_mutex_trylock(EP_THR_MUTEX *mtx);
extern int	ep_thr_mutex_unlock(EP_THR_MUTEX *mtx);
extern int	ep_thr_mutex_check(EP_THR_MUTEX *mtx);
#define		EP_THR_MUTEX_NORMAL		PTHREAD_MUTEX_NORMAL
#define		EP_THR_MUTEX_ERRORCHECK		PTHREAD_MUTEX_ERRORCHECK
#define		EP_THR_MUTEX_RECURSIVE		PTHREAD_MUTEX_RECURSIVE
#define		EP_THR_MUTEX_DEFAULT		PTHREAD_MUTEX_DEFAULT

typedef pthread_cond_t		EP_THR_COND;
#  define	EP_THR_COND_INITIALIZER		= PTHREAD_COND_INITIALIZER
extern int	ep_thr_cond_init(EP_THR_COND *cv);
extern int	ep_thr_cond_destroy(EP_THR_COND *cv);
extern int	ep_thr_cond_signal(EP_THR_COND *cv);
extern int	ep_thr_cond_wait(EP_THR_COND *cv, EP_THR_MUTEX *mtx,
				EP_TIME_SPEC *timeout);
extern int	ep_thr_cond_broadcast(EP_THR_COND *cv);

typedef pthread_rwlock_t	EP_THR_RWLOCK;
#  define	EP_THR_RWLOCK_INITIALIZER	= PTHREAD_RWLOCK_INITIALIZER
extern int	ep_thr_rwlock_init(EP_THR_RWLOCK *rwl);
extern int	ep_thr_rwlock_destroy(EP_THR_RWLOCK *rwl);
extern int	ep_thr_rwlock_rdlock(EP_THR_RWLOCK *rwl);
extern int	ep_thr_rwlock_tryrdlock(EP_THR_RWLOCK *rwl);
extern int	ep_thr_rwlock_wrlock(EP_THR_RWLOCK *rwl);
extern int	ep_thr_rwlock_trywrlock(EP_THR_RWLOCK *rwl);
extern int	ep_thr_rwlock_unlock(EP_THR_RWLOCK *rwl);

/*
**  Thread pool declarations
*/

// initialize a thread pool
void		ep_thr_pool_init(
			int min_threads,	// min number of threads
			int max_threads,	// max number of threads
			uint32_t flags);	// flag bits (none yet)

// run a function in a worker thread from the pool
void		ep_thr_pool_run(
			void (*func)(void *),	// the function
			void *arg);		// passed to func

# else // ! EP_OSCF_USE_PTHREADS

# define	ep_thr_yield()

typedef int	EP_THR_MUTEX;
#  define	EP_THR_MUTEX_INITIALIZER
#  define	ep_thr_mutex_init(mtx, type)	0
#  define	ep_thr_mutex_destroy(mtx)	0
#  define	ep_thr_mutex_lock(mtx)		0
#  define	ep_thr_mutex_trylock(mtx)	0
#  define	ep_thr_mutex_unlock(mtx)	0
#  define	ep_thr_mutex_check(mtx)		0
#define		EP_THR_MUTEX_NORMAL		0
#define		EP_THR_MUTEX_ERRORCHECK		0
#define		EP_THR_MUTEX_RECURSIVE		0

typedef int	EP_THR_COND;
#  define	EP_THR_COND_INITIALIZER
#  define	ep_thr_cond_init(cv)		0
#  define	ep_thr_cond_destroy(cv)		0
#  define	ep_thr_cond_signal(cv)		0
#  define	ep_thr_cond_wait(cv, mtx, to)	0
#  define	ep_thr_cond_broadcast(cv)	0

typedef int	EP_THR_RWLOCK;
#  define	EP_THR_RWLOCK_INITIALIZER
#  define	ep_thr_rwlock_init(rwl)		0
#  define	ep_thr_rwlock_destroy(rwl)	0
#  define	ep_thr_rwlock_rdlock(rwl)	0
#  define	ep_thr_rwlock_tryrdlock(rwl)	0
#  define	ep_thr_rwlock_wrlock(rwl)	0
#  define	ep_thr_rwlock_trywrlock(rwl)	0
#  define	ep_thr_rwlock_unlock(rwl)	0

# endif // EP_OSCF_USE_PTHREADS

#endif // _EP_THR_H_
