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

#include <libxfs.h>
#include "avl.h"
#include "globals.h"
#include "incore.h"
#include "agheader.h"
#include "protos.h"
#include "err_protos.h"
#include "threads.h"


static size_t		rt_bmap_size;

static void
reset_rt_bmap(void)
{
	if (rt_ba_bmap)
		memset(rt_ba_bmap, 0x22, rt_bmap_size);	/* XR_E_FREE */
}

static void
init_rt_bmap(
	xfs_mount_t	*mp)
{
	if (mp->m_sb.sb_rextents == 0)
		return;

	rt_bmap_size = roundup(mp->m_sb.sb_rextents / (NBBY / XR_BB),
			       sizeof(__uint64_t));

	rt_ba_bmap = memalign(sizeof(__uint64_t), rt_bmap_size);
	if (!rt_ba_bmap) {
		do_error(
		_("couldn't allocate realtime block map, size = %llu\n"),
			mp->m_sb.sb_rextents);
		return;
	}
}

static void
free_rt_bmap(xfs_mount_t *mp)
{
	free(rt_ba_bmap);
	rt_ba_bmap = NULL;
}


void
reset_bmaps(xfs_mount_t *mp)
{
	xfs_agnumber_t	agno;
	int		ag_hdr_block;
	int		i;

	ag_hdr_block = howmany(4 * mp->m_sb.sb_sectsize, mp->m_sb.sb_blocksize);

	for (agno = 0; agno < mp->m_sb.sb_agcount; agno++)  {
		memset(ba_bmap[agno], 0,
		       roundup((mp->m_sb.sb_agblocks + (NBBY / XR_BB) - 1) /
				(NBBY / XR_BB), sizeof(__uint64_t)));
		for (i = 0; i < ag_hdr_block; i++)
			set_bmap(agno, i, XR_E_INUSE_FS);
	}

	if (mp->m_sb.sb_logstart != 0) {
		xfs_dfsbno_t	logend;

		logend = mp->m_sb.sb_logstart + mp->m_sb.sb_logblocks;

		for (i = mp->m_sb.sb_logstart; i < logend ; i++)  {
			set_bmap(XFS_FSB_TO_AGNO(mp, i),
				 XFS_FSB_TO_AGBNO(mp, i), XR_E_INUSE_FS);
		}
	}

	reset_rt_bmap();
}

void
init_bmaps(xfs_mount_t *mp)
{
	xfs_agblock_t numblocks = mp->m_sb.sb_agblocks;
	int agcount = mp->m_sb.sb_agcount;
	int i;
	size_t size = 0;

	ba_bmap = calloc(agcount, sizeof(__uint64_t *));
	if (!ba_bmap)
		do_error(_("couldn't allocate block map pointers\n"));

	ag_locks = calloc(agcount, sizeof(pthread_mutex_t));
	if (!ag_locks)
		do_error(_("couldn't allocate block map locks\n"));

	for (i = 0; i < agcount; i++)  {
		size = roundup((numblocks+(NBBY/XR_BB)-1) / (NBBY/XR_BB),
		       		sizeof(__uint64_t));

		ba_bmap[i] = memalign(sizeof(__uint64_t), size);
		if (!ba_bmap[i]) {
			do_error(_("couldn't allocate block map, size = %d\n"),
				numblocks);
			return;
		}
		memset(ba_bmap[i], 0, size);
		pthread_mutex_init(&ag_locks[i], NULL);
	}

	init_rt_bmap(mp);
	reset_bmaps(mp);
}

void
free_bmaps(xfs_mount_t *mp)
{
	xfs_agnumber_t i;

	for (i = 0; i < mp->m_sb.sb_agcount; i++)
		free(ba_bmap[i]);
	free(ba_bmap);
	ba_bmap = NULL;

	free_rt_bmap(mp);
}
