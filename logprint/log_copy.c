/*
 * Copyright (c) 2004 Silicon Graphics, Inc.  All Rights Reserved.
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

#include "logprint.h"

/*
 * Extract a log and write it out to a file
 */

void
xfs_log_copy(
	xlog_t		*log,
	int		fd,
	char		*filename)
{
	int		ofd, r;
	xfs_daddr_t	blkno;
	char		buf[XLOG_HEADER_SIZE];

	if ((ofd = open(filename, O_CREAT|O_EXCL|O_RDWR|O_TRUNC, 0666)) == -1) {
		perror("open");
		exit(1);
	}

	xlog_print_lseek(log, fd, 0, SEEK_SET);
	for (blkno = 0; blkno < log->l_logBBsize; blkno++) {
		r = read(fd, buf, sizeof(buf));
		if (r < 0) {
			fprintf(stderr, "%s: read error (%lld): %s\n",
				__FUNCTION__, (long long)blkno,
				strerror(errno));
			continue;
		} else if (r == 0) {
			printf("%s: physical end of log at %lld\n",
				__FUNCTION__, (long long)blkno);
			break;
		} else if (r != sizeof(buf)) {
			fprintf(stderr, "%s: short read? (%lld)\n",
					__FUNCTION__, (long long)blkno);
			continue;
		}

		r = write(ofd, buf, sizeof(buf));
		if (r < 0) {
			fprintf(stderr, "%s: write error (%lld): %s\n",
				__FUNCTION__, (long long)blkno,
				strerror(errno));
			break;
		} else if (r != sizeof(buf)) {
			fprintf(stderr, "%s: short write? (%lld)\n",
				__FUNCTION__, (long long)blkno);
			continue;
		}
	}

	close(ofd);
}
