#ifndef	_XFS_REPAIR_THREADS_H_
#define	_XFS_REPAIR_THREADS_H_

extern int		do_parallel;
extern int		thread_count;
/*
**  locking variants - rwlock/mutex
*/
#define PREPAIR_RW_LOCK_ATTR		PTHREAD_PROCESS_PRIVATE

#define	PREPAIR_RW_LOCK_ALLOC(lkp, n)				\
	if (do_parallel) {					\
		lkp = malloc(n*sizeof(pthread_rwlock_t));	\
		if (lkp == NULL)				\
			do_error("cannot alloc %d locks\n", n);	\
			/* NO RETURN */				\
	}
#define PREPAIR_RW_LOCK_INIT(l,a)	if (do_parallel) pthread_rwlock_init((l),(a))
#define PREPAIR_RW_READ_LOCK(l) 	if (do_parallel) pthread_rwlock_rdlock((l))
#define PREPAIR_RW_WRITE_LOCK(l)	if (do_parallel) pthread_rwlock_wrlock((l))
#define PREPAIR_RW_UNLOCK(l)		if (do_parallel) pthread_rwlock_unlock((l))
#define PREPAIR_RW_WRITE_LOCK_NOTEST(l)	pthread_rwlock_wrlock((l))
#define PREPAIR_RW_UNLOCK_NOTEST(l)	pthread_rwlock_unlock((l))
#define PREPAIR_RW_LOCK_DELETE(l)	if (do_parallel) pthread_rwlock_destroy((l))

#define PREPAIR_MTX_LOCK_INIT(m, a)	if (do_parallel) pthread_mutex_init((m), (a))
#define PREPAIR_MTX_ATTR_INIT(a)	if (do_parallel) pthread_mutexattr_init((a))
#define PREPAIR_MTX_ATTR_SET(a, l)	if (do_parallel) pthread_mutexattr_settype((a), l)
#define PREPAIR_MTX_LOCK(m)		if (do_parallel) pthread_mutex_lock(m)
#define PREPAIR_MTX_UNLOCK(m)		if (do_parallel) pthread_mutex_unlock(m)


typedef void	disp_func_t(xfs_mount_t *mp, xfs_agnumber_t agno);
extern	int	queue_work(disp_func_t func, xfs_mount_t *mp, xfs_agnumber_t agno);
extern	void	wait_for_workers(void);

#endif	/* _XFS_REPAIR_THREADS_H_ */
