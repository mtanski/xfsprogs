/*
 * Copyright (c) 2000-2003,2005 Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write the Free Software Foundation,
 * Inc.,  51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <xfs/libxfs.h>
#include <aio.h>
#include <diskinfo.h>

extern char *progname;
extern __int64_t findsize(char *);

int
platform_check_ismounted(char *name, char *block, struct stat64 *s, int verbose)
{
	return 0;
}

int
platform_check_iswritable(char *name, char *block, struct stat64 *s, int fatal)
{
	return 1;
}

void
platform_set_blocksize(int fd, char *path, int blocksize)
{
	return;
}

void
platform_flush_device(int fd, dev_t device)
{
	return;
}

void
platform_findsizes(char *path, int fd, long long *sz, int *bsz)
{
	struct stat64		st;

	if (fstat64(fd, &st) < 0) {
		fprintf(stderr,
			_("%s: cannot stat the device file \"%s\": %s\n"),
			progname, path, strerror(errno));
		exit(1);
	}
	if ((st.st_mode & S_IFMT) == S_IFREG) {
		*sz = (long long)(st.st_size >> 9);
	} else {
		*sz = findsize(path);
	}
	*bsz = BBSIZE;
}

int
platform_aio_init(int aio_count)
{
	struct aioinit aio_init;

	memset(&aio_init, 0, sizeof(aio_init));
	aio_init.aio_threads = aio_count;
	aio_init.aio_numusers = aio_count;

	aio_sgi_init64(&aio_init);
	return (1);		/* aio/lio_listio available */
}

char *
platform_findrawpath(char *path)
{
	return findrawpath(path);
}

size_t
platform_memalignment(void)
{
	return sizeof(void *);
}
