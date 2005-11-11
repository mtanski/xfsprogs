/*
 * Copyright (c) 2000-2001,2005 Silicon Graphics, Inc.
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
#include "bmap.h"
#include "dbread.h"
#include "io.h"
#include "init.h"

int
dbread(void *buf, int nblocks, xfs_fileoff_t bno, int whichfork)
{
	bmap_ext_t	bm;
	char		*bp;
	xfs_dfiloff_t	eb;
	xfs_dfiloff_t	end;
	int		i;
	int		nex;

	nex = 1;
	end = bno + nblocks;
	bp = buf;
	while (bno < end) {
		bmap(bno, end - bno, whichfork, &nex, &bm);
		if (nex == 0) {
			bm.startoff = end;
			bm.blockcount = 1;
		}
		if (bm.startoff > bno) {
			eb = end < bm.startoff ? end : bm.startoff;
			i = (int)XFS_FSB_TO_B(mp, eb - bno);
			memset(bp, 0, i);
			bp += i;
			bno = eb;
		}
		if (bno == end)
			break;
		if (bno > bm.startoff) {
			bm.blockcount -= bno - bm.startoff;
			bm.startblock += bno - bm.startoff;
			bm.startoff = bno;
		}
		if (bm.startoff + bm.blockcount > end)
			bm.blockcount = end - bm.startoff;
		i = read_bbs(XFS_FSB_TO_DADDR(mp, bm.startblock),
			     (int)XFS_FSB_TO_BB(mp, bm.blockcount),
			     (void **)&bp, NULL);
		if (i)
			return i;
		bp += XFS_FSB_TO_B(mp, bm.blockcount);
		bno += bm.blockcount;
	}
	return 0;
}
