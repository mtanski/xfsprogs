#include <libxfs.h>
#include "pthread.h"
#include "signal.h"
#include "threads.h"
#include "err_protos.h"
#include "protos.h"

int do_parallel = 1;
int thread_count;

/* A quantum of work */
typedef struct work_s {
	struct work_s	*next;
	disp_func_t	*function;
	xfs_mount_t	*mp;
	xfs_agnumber_t	agno;
} work_t;

typedef struct  work_queue_s {
	work_t		*next;
	work_t		*last;
	int		active_threads;
	int		work_count;
	pthread_cond_t	mcv;	/* main thread conditional variable */
	pthread_cond_t	wcv;	/* worker threads conditional variable */
	pthread_mutex_t	mutex;
} work_queue_t;

static	work_queue_t	work_queue;
static	pthread_t	*work_threads;

static	void	*worker_thread(void *arg);

static void
init_workers(work_queue_t *wq, int nw)
{
	int			err;
	pthread_mutexattr_t	mtxattr;

	memset(wq, 0, sizeof(work_queue_t));
	wq->active_threads = nw;

	pthread_cond_init(&wq->mcv, NULL);
	pthread_cond_init(&wq->wcv, NULL);
	pthread_mutexattr_init(&mtxattr);

#ifdef	PTHREAD_MUTEX_SPINBLOCK_NP
	/* NP - Non Portable - Irix */
	if ((err = pthread_mutexattr_settype(&mtxattr,
			PTHREAD_MUTEX_SPINBLOCK_NP)) > 0) {
		do_error(_("init_workers: thread 0x%x: pthread_mutexattr_settype error %d: %s\n"),
			pthread_self(), err, strerror(err));
	}
#endif
#ifdef	PTHREAD_MUTEX_FAST_NP
	/* NP - Non Portable - Linux */
	if ((err = pthread_mutexattr_settype(&mtxattr,
			PTHREAD_MUTEX_FAST_NP)) > 0) {
		do_error(_("init_workers: thread 0x%x: pthread_mutexattr_settype error %d: %s\n"),
			pthread_self(), err, strerror(err));
	}
#endif
	if ((err = pthread_mutex_init(&wq->mutex, &mtxattr)) > 0) {
		do_error(_("init_workers: thread 0x%x: pthread_mutex_init error %d: %s\n"),
			pthread_self(), err, strerror(err));
	}
}

static void
quiesce_workers(work_queue_t *wq)
{
	int	err;

	if ((err = pthread_mutex_lock(&wq->mutex)) > 0)
		do_error(_("quiesce_workers: thread 0x%x: pthread_mutex_lock error %d: %s\n"),
			pthread_self(), err, strerror(err));
	if (wq->active_threads > 0) {
		if ((err = pthread_cond_wait(&wq->mcv, &wq->mutex)) > 0)
			do_error(_("quiesce_workers: thread 0x%x: pthread_cond_wait error %d: %s\n"),
				pthread_self(), err, strerror(err));
	}
	ASSERT(wq->active_threads == 0);
	if ((err = pthread_mutex_unlock(&wq->mutex)) > 0)
		do_error(_("quiesce_workers: thread 0x%x: pthread_mutex_unlock error %d: %s\n"),
			pthread_self(), err, strerror(err));
}

static void
start_workers(work_queue_t *wq, unsigned thcnt, pthread_attr_t *attrp)
{
	int		err;
	unsigned long	i;

	init_workers(wq, thcnt);

	if ((work_threads = (pthread_t *)malloc(sizeof(pthread_t) * thcnt)) == NULL)
		do_error(_("cannot malloc %ld bytes for work_threads array\n"), 
				sizeof(pthread_t) * thcnt);

	/*
	**  Create worker threads
	*/
	for (i = 0; i < thcnt; i++) {
		err = pthread_create(&work_threads[i], attrp, worker_thread, (void *) i);
		if(err > 0) {
		        do_error(_("cannot create worker threads, status = [%d] %s\n"),
				err, strerror(err));
		}
	}
	do_log(_("        - creating %d worker thread(s)\n"), thcnt);

	/*
	**  Wait for all worker threads to initialize
	*/
	quiesce_workers(wq);
}

void
thread_init(void)
{
	int		status;
	size_t		stacksize;
	pthread_attr_t	attr;
	sigset_t	blocked;

	if (do_parallel == 0)
		return;
	if (thread_count == 0)
		thread_count = 2 * libxfs_nproc();

	if ((status = pthread_attr_init(&attr)) != 0)
		do_error(_("status from pthread_attr_init: %d"),status);

	if ((status = pthread_attr_getstacksize(&attr, &stacksize)) != 0)
		do_error(_("status from pthread_attr_getstacksize: %d"), status);

	stacksize *= 4;

	if ((status = pthread_attr_setstacksize(&attr, stacksize)) != 0)
		do_error(_("status from pthread_attr_setstacksize: %d"), status);

	if ((status = pthread_setconcurrency(thread_count)) != 0)
		do_error(_("Status from pthread_setconcurrency(%d): %d"), thread_count, status);

	/*
	 *  block delivery of progress report signal to all threads
         */
	sigemptyset(&blocked);
	sigaddset(&blocked, SIGHUP);
	sigaddset(&blocked, SIGALRM);
	pthread_sigmask(SIG_BLOCK, &blocked, NULL);

	start_workers(&work_queue, thread_count, &attr);
}

/*
 * Dequeue from the head of the list.
 * wq->mutex held.
 */
static work_t *
dequeue(work_queue_t *wq)
{
	work_t	*wp;

	ASSERT(wq->work_count > 0);
	wp = wq->next;
	wq->next = wp->next;
	wq->work_count--;
	if (wq->next == NULL) {
		ASSERT(wq->work_count == 0);
		wq->last = NULL;
	}
	wp->next = NULL;
	return (wp);
}

static void *
worker_thread(void *arg)
{
	work_queue_t	*wq;
	work_t		*wp;
	int		err;
	unsigned long	myid;

	wq = &work_queue;
	myid = (unsigned long) arg;
	ts_init();
	libxfs_lio_allocate();

	/*
	 * Loop pulling work from the global work queue.
	 * Check for notification to exit after every chunk of work.
	 */
	while (1) {
		if ((err = pthread_mutex_lock(&wq->mutex)) > 0)
			do_error(_("work_thread%d: thread 0x%x: pthread_mutex_lock error %d: %s\n"),
				myid, pthread_self(), err, strerror(err));
		/*
		 * Wait for work.
		 */
		while (wq->next == NULL) {
			ASSERT(wq->work_count == 0);
			/*
			 * Last thread going to idle sleep must wakeup
			 * the master thread.  Same mutex is used to lock
			 * around two different condition variables.
			 */
			wq->active_threads--;
			ASSERT(wq->active_threads >= 0);
			if (!wq->active_threads) {
				if ((err = pthread_cond_signal(&wq->mcv)) > 0)
					do_error(_("work_thread%d: thread 0x%x: pthread_cond_signal error %d: %s\n"),
						myid, pthread_self(), err, strerror(err));
			}
			if ((err = pthread_cond_wait(&wq->wcv, &wq->mutex)) > 0)
				do_error(_("work_thread%d: thread 0x%x: pthread_cond_wait error %d: %s\n"),
					myid, pthread_self(), err, strerror(err));
			wq->active_threads++;
		}
		/*
		 *  Dequeue work from the head of the list.
		 */
		ASSERT(wq->work_count > 0);
		wp = dequeue(wq);
		if ((err = pthread_mutex_unlock(&wq->mutex)) > 0)
			do_error(_("work_thread%d: thread 0x%x: pthread_mutex_unlock error %d: %s\n"),
				myid, pthread_self(), err, strerror(err));
		/*
		 *  Do the work.
		 */
		(wp->function)(wp->mp, wp->agno);

		free(wp);
	}
	/* NOT REACHED */
	pthread_exit(NULL);
	return (NULL);
}

int
queue_work(disp_func_t func, xfs_mount_t *mp, xfs_agnumber_t agno)
{
	work_queue_t *wq;
	work_t	*wp;

	if (do_parallel == 0) {
		func(mp, agno);
		return 0;
	}
	wq = &work_queue;
	/*
	 * Get memory for a new work structure.
	 */
	if ((wp = (work_t *)memalign(8, sizeof(work_t))) == NULL)
		return (ENOMEM);
	/*
	 * Initialize the new work structure.
	 */
	wp->function = func;
	wp->mp = mp;
	wp->agno = agno;

	/*
	 *  Now queue the new work structure to the work queue.
	 */
	if (wq->next == NULL) {
		wq->next = wp;
	} else {
		wq->last->next = wp;
	}
	wq->last = wp;
	wp->next = NULL;
	wq->work_count++;

	return (0);
}

void
wait_for_workers(void)
{
	int		err;
	work_queue_t	*wq;

	if (do_parallel == 0)
		return;
	wq = &work_queue;
	if ((err = pthread_mutex_lock(&wq->mutex)) > 0)
		do_error(_("wait_for_workers: thread 0x%x: pthread_mutex_lock error %d: %s\n"),
			pthread_self(), err, strerror(err));

	ASSERT(wq->active_threads == 0);
	if (wq->work_count > 0) {
		/* get the workers going */
		if ((err = pthread_cond_broadcast(&wq->wcv)) > 0)
			do_error(_("wait_for_workers: thread 0x%x: pthread_cond_broadcast error %d: %s\n"),
				pthread_self(), err, strerror(err));
		/* and wait for them */
		if ((err = pthread_cond_wait(&wq->mcv, &wq->mutex)) > 0)
			do_error(_("wait_for_workers: thread 0x%x: pthread_cond_wait error %d: %s\n"),
				pthread_self(), err, strerror(err));
	}
	ASSERT(wq->active_threads == 0);
	ASSERT(wq->work_count == 0);

	if ((err = pthread_mutex_unlock(&wq->mutex)) > 0)
		do_error(_("wait_for_workers: thread 0x%x: pthread_mutex_unlock error %d: %s\n"),
			pthread_self(), err, strerror(err));
}
