/*
 * Copyright (c) 2000 Silicon Graphics, Inc.  All Rights Reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * 
 * Further, this software is distributed without any warranty that it is
 * free of the rightful claim of any third person regarding infringement
 * or the like.  Any license provided herein, whether implied or
 * otherwise, applies only to this software file.  Patent licenses, if
 * any, provided herein do not apply to combinations of this program with
 * other software, or any other product whatsoever.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston MA 02111-1307, USA.
 * 
 * Contact information: Silicon Graphics, Inc., 1600 Amphitheatre Pkwy,
 * Mountain View, CA  94043, or:
 * 
 * http://www.sgi.com 
 * 
 * For further information regarding this notice, see: 
 * 
 * http://oss.sgi.com/projects/GenInfo/SGIGPLNoticeExplan/
 */

/* 
 * Make file utility for xfs.
 */

#include <libxfs.h>
#include <malloc.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/vfs.h>
#include <ctype.h>

#define	MAXBUFFERSIZE	(256 * 1024)

static char *progname;

static void
usage(void)
{
	fprintf(stderr, "%s: [-npv] <size> <name1> [<name2>] ...\n", progname);
	exit(2);
}

static int
openfd(char *name, int oflags)
{
	struct statfs buf;
	int fd;

	fd = open(name, oflags, 0600);
	if (fd < 0) {
		perror(name);
		return -1;
	}

	fstatfs(fd, &buf);
	if (buf.f_type != XFS_SUPER_MAGIC) {
		fprintf(stderr, "%s: "
			"file [\"%s\"] is not on an XFS filesystem\n",
			progname, name);
		return -1;
	}
	return fd;
}

int
main(int argc, char **argv)
{
	int fd;
	loff_t result;
	loff_t size = 0;
	loff_t mult = 0;
	int bytes = 0;
	loff_t wrote = 0;
	int len = 0;
	int c;
	int errflg = 0;
	int errs = 0;
	int nobytes = 0;
	int prealloc = 0;
	int verbose = 0;
	struct dioattr da;
	void *buf = NULL;
	int buflen = 0, nbuflen;
	int bufalign = 0, nbufalign, bufmin;
	int oflags;
	xfs_flock64_t flck;

	progname = basename(argv[0]);
	while ((c = getopt(argc, argv, "npvV")) != EOF) {
		switch(c) {
			case 'n':
				nobytes++;
				break;
			case 'p':
				prealloc++;
				break;
			case 'v':
				verbose++;
				break;
			case 'V':
				printf("%s version %s\n", progname, VERSION);
				exit(0);
			default:
				errflg++;
				break;
		}
	}

	if (argc < optind + 2 || errflg)
		usage();

	mult = 1;

	len = strlen(argv[optind]);

	if (isalpha(argv[optind][len-1])) {
		switch (argv[optind][len-1]) {
		case 'k':
		case 'K':
			mult = 1024;
			break;
		case 'b':
		case 'B':
			mult = 512;
			break;
		case 'm':
		case 'M':
			mult  = 1024;
			mult *= 1024;
			break;
		case 'g':
		case 'G':
			mult  = 1024;
			mult *= 1024;
			mult *= 1024;
			break;
		default:
			fprintf(stderr, "unknown size %s\n", argv[optind]);
			usage();
		}

		argv[optind][len-1] = '\0';
	}

	size = atoll(argv[optind]) * mult;

	optind++;

	while (optind < argc) {
		if (verbose)
			fprintf(stdout, "%s %lld bytes %s\n",
						argv[optind], (long long)size,
						prealloc
						  ? "(pre-allocated)"
						  : "");

		oflags = O_CREAT|O_TRUNC|O_WRONLY|(nobytes ? 0 : O_DIRECT);

		if ((fd = openfd(argv[optind], oflags)) == -1) {
			optind++;
			errs++;
			continue;
		}

		if (   (oflags & O_DIRECT)
		    && (   (fd < 0 && errno == EINVAL)
			|| ioctl(fd, XFS_IOC_DIOINFO, &da) < 0)) {

			close(fd);

			oflags &= ~O_DIRECT;

			if ((fd = openfd(argv[optind], oflags)) == -1) {
				optind++;
				errs++;
				continue;
			}
		}

		if (size == 0) {
			close(fd);
			optind++;
			continue;
		}

		if ((result = lseek64(fd, size - 1, SEEK_SET)) < 0LL) {
			/*
			 * This check doesn't actually work for 6.2
			 * efs and nfs2, although it should.
			 */
			fprintf(stderr, "lseek64 error, result = %lld\n",
				(long long)result);
			if (errno)
				perror(argv[optind]);
			errs++;
		} else if (nobytes) {
			if (write(fd, "", 1) < 0) {
				perror(argv[optind]);
				errs++;
			}
		} else {
			flck.l_whence = SEEK_SET;
			flck.l_start  = 0LL;
			flck.l_len    = size;
			if (prealloc)
				(void)ioctl(fd, XFS_IOC_RESVSP64, &flck);
			if (oflags & O_DIRECT) {
				nbufalign = da.d_mem;

				if (   da.d_miniosz <= MAXBUFFERSIZE
				    && MAXBUFFERSIZE <= da.d_maxiosz)
					nbuflen = MAXBUFFERSIZE;
				else if (da.d_maxiosz < MAXBUFFERSIZE)
					nbuflen = da.d_maxiosz;
				else
					nbuflen = da.d_miniosz;

				bufmin = da.d_miniosz;
			} else {
				nbuflen = MAXBUFFERSIZE;
				nbufalign = sizeof(long);
				bufmin = 0;
			}

			if (nbuflen > buflen || nbufalign > bufalign) {
				if (buf)
					free(buf);
				buf = memalign(nbufalign, nbuflen);
				buflen = nbuflen;
				bzero(buf, nbuflen);
				nbufalign = bufalign;
			}

			wrote = 0;

			lseek64(fd, 0LL, SEEK_SET);

			while (wrote < size) {
				if (size - wrote >= buflen)
					bytes = buflen;
				else if (bufmin)
					bytes = roundup(size - wrote, bufmin);
				else
					bytes = size - wrote;

				len = write(fd, buf, bytes);

				if (len < 0) {
					perror(argv[optind]);
					unlink(argv[optind]);
					errs++;
					break;
				}

				wrote += len;
			}

			if (wrote > size && ftruncate64(fd, size) < 0) {
				perror(argv[optind]);
				unlink(argv[optind]);
				errs++;
			}
		}

		if ( close(fd) < 0 ) {
			perror(argv[optind]);
			unlink(argv[optind]);
			errs++;
		}

		optind++;
	}

	return errs != 0;
}
