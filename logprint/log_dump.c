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
 * Dump log blocks, not data
 */

void
xfs_log_dump(
	xlog_t			*log,
	int			fd,
	int			print_block_start)
{
	int			r;
	uint			last_cycle = -1;
	xfs_daddr_t		blkno, dupblkno;
	xlog_rec_header_t	*hdr;
	char			buf[XLOG_HEADER_SIZE];

	dupblkno = 0;
	hdr = (xlog_rec_header_t *)buf;
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
		}

		if (CYCLE_LSN(buf, ARCH_CONVERT) == XLOG_HEADER_MAGIC_NUM &&
		    !print_no_data) {
			printf(
		"%6lld HEADER Cycle %d tail %d:%06d len %6d ops %d\n",
				(long long)blkno,
				INT_GET(hdr->h_cycle, ARCH_CONVERT),
				CYCLE_LSN(hdr->h_tail_lsn, ARCH_CONVERT),
				BLOCK_LSN(hdr->h_tail_lsn, ARCH_CONVERT),
				INT_GET(hdr->h_len, ARCH_CONVERT),
				INT_GET(hdr->h_num_logops, ARCH_CONVERT));
		}

		if (GET_CYCLE(buf, ARCH_CONVERT) != last_cycle) {
			printf(
		"[%05lld - %05lld] Cycle 0x%08x New Cycle 0x%08x\n",
				(long long)dupblkno, (long long)blkno,
				last_cycle, GET_CYCLE(buf, ARCH_CONVERT));
			last_cycle = GET_CYCLE(buf, ARCH_CONVERT);
			dupblkno = blkno;
		}
	}
}
