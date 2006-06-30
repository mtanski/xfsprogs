#include <xfs/libxfs.h>
#include "init.h"
#include "aio.h"

#define	DEF_PREFETCH_INOS	16
#define	DEF_PREFETCH_DIRS	16
#define	DEF_PREFETCH_AIO	32
int	libxfs_lio_ino_count = DEF_PREFETCH_INOS;
int	libxfs_lio_dir_count = DEF_PREFETCH_DIRS;
int	libxfs_lio_aio_count = DEF_PREFETCH_AIO;

static pthread_key_t lio_ino_key;
static pthread_key_t lio_dir_key;

void
libxfs_lio_allocate(void)
{
#ifdef	_AIOCB64_T_DEFINED
	size_t		size;
	void		*voidp;

	/*
	 * allocate a per-thread buffer which will be used in libxfs_readbuf_list
	 * in the following order:
	 * libxfs_lio_req_t array
	 * aiocb64_t array
	 * aiocb64_t * array
	 * xfs_buf_t * array
	 */
	size = sizeof(libxfs_lio_req_t) + sizeof(aiocb64_t) +  sizeof(aiocb64_t *) + sizeof(xfs_buf_t *);

	voidp = malloc(libxfs_lio_ino_count*size);
	if (voidp == NULL) {
		fprintf(stderr, "lio_allocate: cannot allocate thread specific storage\n");
		exit(1);
		/* NO RETURN */
		return;
	}
	pthread_setspecific(lio_ino_key,  voidp);

	voidp = malloc(libxfs_lio_dir_count*size);
	if (voidp == NULL) {
		fprintf(stderr, "lio_allocate: cannot allocate thread specific storage\n");
		exit(1);
		/* NO RETURN */
		return;
	}
	pthread_setspecific(lio_dir_key,  voidp);
#endif	/* _AIOCB64_T_DEFINED */
}

int
libxfs_lio_init(void)
{
#ifdef	_AIOCB64_T_DEFINED
	if (platform_aio_init(libxfs_lio_aio_count)) {
		pthread_key_create(&lio_ino_key, NULL);
		pthread_key_create(&lio_dir_key, NULL);
		return (1);
	}
#endif	/* _AIOCB64_T_DEFINED */
	return (0);
}

void *
libxfs_get_lio_buffer(int type)
{
#ifdef	_AIOCB64_T_DEFINED
	if (type == LIBXFS_LIO_TYPE_INO)
		return pthread_getspecific(lio_ino_key);
	if (type == LIBXFS_LIO_TYPE_DIR)
		return pthread_getspecific(lio_dir_key);
	if (type == LIBXFS_LIO_TYPE_RAW) {
		/* use the inode buffers since there is
		 * no overlap with the other requests.
		 */
		return pthread_getspecific(lio_ino_key);
	}
	fprintf(stderr, "get_lio_buffer: invalid type 0x%x\n", type);
	exit(1);
#endif
	return NULL;
}

/* ARGSUSED */
void
libxfs_put_lio_buffer(void *buffer)
{
	return;	/* nothing to do */
}

static int
lio_compare(const void *e1, const void *e2)
{
	libxfs_lio_req_t *r1 = (libxfs_lio_req_t *) e1;
	libxfs_lio_req_t *r2 = (libxfs_lio_req_t *) e2;

	return (int) (r1->blkno - r2->blkno);
}

int
libxfs_readbuf_list(dev_t dev, int nent, void *voidp, int type)
{
#ifdef	_AIOCB64_T_DEFINED
	libxfs_lio_req_t	*rblp;
	xfs_buf_t		*bp, **bplist;
	aiocb64_t		*aioclist, **aiocptr;
	int			i, nbp, err;
	int			fd;

	if (nent <= 0)
		return 0;
	if ((type == LIBXFS_LIO_TYPE_INO) || (type == LIBXFS_LIO_TYPE_RAW)) {
		if (libxfs_lio_ino_count == 0)
			return (0);
		if (nent > libxfs_lio_ino_count)
			nent = libxfs_lio_ino_count;
	}
	else if (type == LIBXFS_LIO_TYPE_DIR) {
		if (libxfs_lio_dir_count == 0)
			return (0);
		if (nent > libxfs_lio_dir_count)
			nent = libxfs_lio_dir_count;
		if (nent > 2)
			qsort(voidp, nent, sizeof(libxfs_lio_req_t), lio_compare);
	}
	else {
		fprintf(stderr, "Invalid type 0x%x in libxfs_readbuf_list\n", type);
		abort();
		/* NO RETURN */
		return (0);
	}

	/* space for lio_listio processing, see libxfs_lio_allocate */
	rblp = (libxfs_lio_req_t *) voidp;
	aioclist = (aiocb64_t *) (rblp + nent);
	aiocptr = (aiocb64_t **) (aioclist + nent);
	bplist = (xfs_buf_t **) (aiocptr + nent);

	bzero(aioclist, nent*sizeof(aiocb64_t));

	/* look in buffer cache */
	for (i = 0, nbp = 0; i < nent; i++) {
		ASSERT(rblp[i].len);
		bp = libxfs_getbuf(dev, rblp[i].blkno, rblp[i].len);
		if (bp == NULL)
			continue;
		if (bp->b_flags & (LIBXFS_B_UPTODATE|LIBXFS_B_DIRTY)) {
			/* already in cache */
			libxfs_putbuf(bp);
			continue;
		}
		bplist[nbp++] = bp;
	}

	if (nbp == 0)
		return (0); /* Nothing to do */

	if (nbp == 1) {
		libxfs_putbuf(bplist[0]);	/* single buffer, no point */
		return (0);
	}

	fd = libxfs_device_to_fd(dev);

	for (i = 0; i < nbp; i++) {
		aioclist[i].aio_fildes = fd;
		aioclist[i].aio_nbytes = XFS_BUF_COUNT(bplist[i]);
		aioclist[i].aio_buf = XFS_BUF_PTR(bplist[i]);
		aioclist[i].aio_offset = LIBXFS_BBTOOFF64(XFS_BUF_ADDR(bplist[i]));
		aioclist[i].aio_lio_opcode = LIO_READ;
		aiocptr[i] = &aioclist[i];
	}

	err = lio_listio64(LIO_WAIT, aiocptr, nbp, NULL);

	if (err != 0) {
		fprintf(stderr, "lio_listio (%d entries) failure err = %d\n", nbp, err);
	}

	for (i = 0; i < nbp; i++) {
		/* buffer with data in cache available via future libxfs_readbuf */
		if (err == 0)
			bplist[i]->b_flags |= LIBXFS_B_UPTODATE;
		libxfs_putbuf(bplist[i]);
	}

	return (err == 0? nbp : -1);
#else	/* _AIOCB64_T_DEFINED */
	return -1;
#endif	/* _AIOCB64_T_DEFINED */
}
