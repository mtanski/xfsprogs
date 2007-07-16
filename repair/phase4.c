/*
 * Copyright (c) 2000-2002,2005 Silicon Graphics, Inc.
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
#include "agheader.h"
#include "incore.h"
#include "protos.h"
#include "err_protos.h"
#include "dinode.h"
#include "dir.h"
#include "bmap.h"
#include "versions.h"
#include "dir2.h"
#include "threads.h"
#include "progress.h"


/*
 * null out quota inode fields in sb if they point to non-existent inodes.
 * this isn't as redundant as it looks since it's possible that the sb field
 * might be set but the imap and inode(s) agree that the inode is
 * free in which case they'd never be cleared so the fields wouldn't
 * be cleared by process_dinode().
 */
void
quotino_check(xfs_mount_t *mp)
{
	ino_tree_node_t *irec;

	if (mp->m_sb.sb_uquotino != NULLFSINO && mp->m_sb.sb_uquotino != 0)  {
		if (verify_inum(mp, mp->m_sb.sb_uquotino))
			irec = NULL;
		else
			irec = find_inode_rec(
				XFS_INO_TO_AGNO(mp, mp->m_sb.sb_uquotino),
				XFS_INO_TO_AGINO(mp, mp->m_sb.sb_uquotino));

		if (irec == NULL || is_inode_free(irec,
				mp->m_sb.sb_uquotino - irec->ino_startnum))  {
			mp->m_sb.sb_uquotino = NULLFSINO;
			lost_uquotino = 1;
		} else
			lost_uquotino = 0;
	}

	if (mp->m_sb.sb_gquotino != NULLFSINO && mp->m_sb.sb_gquotino != 0)  {
		if (verify_inum(mp, mp->m_sb.sb_gquotino))
			irec = NULL;
		else
			irec = find_inode_rec(
				XFS_INO_TO_AGNO(mp, mp->m_sb.sb_gquotino),
				XFS_INO_TO_AGINO(mp, mp->m_sb.sb_gquotino));

		if (irec == NULL || is_inode_free(irec,
				mp->m_sb.sb_gquotino - irec->ino_startnum))  {
			mp->m_sb.sb_gquotino = NULLFSINO;
			if (mp->m_sb.sb_qflags & XFS_GQUOTA_ACCT)
				lost_gquotino = 1;
			else
				lost_pquotino = 1;
		} else
			lost_gquotino = lost_pquotino = 0;
	}
}

void
quota_sb_check(xfs_mount_t *mp)
{
	/*
	 * if the sb says we have quotas and we lost both,
	 * signal a superblock downgrade.  that will cause
	 * the quota flags to get zeroed.  (if we only lost
	 * one quota inode, do nothing and complain later.)
	 *
	 * if the sb says we have quotas but we didn't start out
	 * with any quota inodes, signal a superblock downgrade.
	 *
	 * The sb downgrades are so that older systems can mount
	 * the filesystem.
	 *
	 * if the sb says we don't have quotas but it looks like
	 * we do have quota inodes, then signal a superblock upgrade.
	 *
	 * if the sb says we don't have quotas and we have no
	 * quota inodes, then leave will enough alone.
	 */

	if (fs_quotas &&
	    (mp->m_sb.sb_uquotino == NULLFSINO || mp->m_sb.sb_uquotino == 0) &&
	    (mp->m_sb.sb_gquotino == NULLFSINO || mp->m_sb.sb_gquotino == 0))  {
		lost_quotas = 1;
		fs_quotas = 0;
	} else if (!verify_inum(mp, mp->m_sb.sb_uquotino) &&
			!verify_inum(mp, mp->m_sb.sb_gquotino)) {
		fs_quotas = 1;
	}
}


void
parallel_p4_process_aginodes(xfs_mount_t *mp, xfs_agnumber_t agno)
{
	do_log(_("        - agno = %d\n"), agno);
	process_aginodes(mp, agno, 0, 1, 0);

	/*
	 * now recycle the per-AG duplicate extent records
	 */
	release_dup_extent_tree(agno);
}

void
phase4(xfs_mount_t *mp)
{
	ino_tree_node_t		*irec;
	xfs_drtbno_t		bno;
	xfs_drtbno_t		rt_start;
	xfs_extlen_t		rt_len;
	xfs_agnumber_t		i;
	xfs_agblock_t		j;
	xfs_agblock_t		ag_end;
	xfs_agblock_t		extent_start;
	xfs_extlen_t		extent_len;
	int			ag_hdr_len = 4 * mp->m_sb.sb_sectsize;
	int			ag_hdr_block;
	int			bstate;
	int			count_bcnt_extents(xfs_agnumber_t agno);
	int			count_bno_extents(xfs_agnumber_t agno);

	ag_hdr_block = howmany(ag_hdr_len, mp->m_sb.sb_blocksize);

	do_log(_("Phase 4 - check for duplicate blocks...\n"));
	do_log(_("        - setting up duplicate extent list...\n"));

	set_progress_msg(PROG_FMT_DUP_EXTENT, (__uint64_t) glob_agcount);

	irec = find_inode_rec(XFS_INO_TO_AGNO(mp, mp->m_sb.sb_rootino),
				XFS_INO_TO_AGINO(mp, mp->m_sb.sb_rootino));

	/*
	 * we always have a root inode, even if it's free...
	 * if the root is free, forget it, lost+found is already gone
	 */
	if (is_inode_free(irec, 0) || !inode_isadir(irec, 0))  {
		need_root_inode = 1;
		if (no_modify)
			do_warn(_("root inode would be lost\n"));
		else
			do_warn(_("root inode lost\n"));
	}

	for (i = 0; i < mp->m_sb.sb_agcount; i++)  {
		ag_end = (i < mp->m_sb.sb_agcount - 1) ? mp->m_sb.sb_agblocks :
			mp->m_sb.sb_dblocks -
				(xfs_drfsbno_t) mp->m_sb.sb_agblocks * i;
		extent_start = extent_len = 0;
		/*
		 * set up duplicate extent list for this ag
		 */
		for (j = ag_hdr_block; j < ag_end; j++)  {

			/* Process in chunks of 16 (XR_BB_UNIT/XR_BB) */
			if ((extent_start == 0) && ((j & XR_BB_MASK) == 0)) {
				switch(ba_bmap[i][j>>XR_BB]) {
				case XR_E_UNKNOWN_LL:
				case XR_E_FREE1_LL:
				case XR_E_FREE_LL:
				case XR_E_INUSE_LL:
				case XR_E_INUSE_FS_LL:
				case XR_E_INO_LL:
				case XR_E_FS_MAP_LL:
					j += (XR_BB_UNIT/XR_BB) - 1;
					continue;
				}
			}

			bstate = get_agbno_state(mp, i, j);

			switch (bstate)  {
			case XR_E_BAD_STATE:
			default:
				do_warn(
				_("unknown block state, ag %d, block %d\n"),
					i, j);
				/* fall through .. */
			case XR_E_UNKNOWN:
			case XR_E_FREE1:
			case XR_E_FREE:
			case XR_E_INUSE:
			case XR_E_INUSE_FS:
			case XR_E_INO:
			case XR_E_FS_MAP:
				if (extent_start == 0)
					continue;
				else  {
					/*
					 * add extent and reset extent state
					 */
					add_dup_extent(i, extent_start,
							extent_len);
					extent_start = 0;
					extent_len = 0;
				}
				break;
			case XR_E_MULT:
				if (extent_start == 0)  {
					extent_start = j;
					extent_len = 1;
				} else if (extent_len == MAXEXTLEN)  {
					add_dup_extent(i, extent_start,
							extent_len);
					extent_start = j;
					extent_len = 1;
				} else
					extent_len++;
				break;
			}
		}
		/*
		 * catch tail-case, extent hitting the end of the ag
		 */
		if (extent_start != 0)
			add_dup_extent(i, extent_start, extent_len);
		PROG_RPT_INC(prog_rpt_done[i], 1);
	}
	print_final_rpt();

	/*
	 * initialize realtime bitmap
	 */
	rt_start = 0;
	rt_len = 0;

	for (bno = 0; bno < mp->m_sb.sb_rextents; bno++)  {

		bstate = get_rtbno_state(mp, bno);

		switch (bstate)  {
		case XR_E_BAD_STATE:
		default:
			do_warn(_("unknown rt extent state, extent %llu\n"),
				bno);
			/* fall through .. */
		case XR_E_UNKNOWN:
		case XR_E_FREE1:
		case XR_E_FREE:
		case XR_E_INUSE:
		case XR_E_INUSE_FS:
		case XR_E_INO:
		case XR_E_FS_MAP:
			if (rt_start == 0)
				continue;
			else  {
				/*
				 * add extent and reset extent state
				 */
				add_rt_dup_extent(rt_start, rt_len);
				rt_start = 0;
				rt_len = 0;
			}
			break;
		case XR_E_MULT:
			if (rt_start == 0)  {
				rt_start = bno;
				rt_len = 1;
			} else if (rt_len == MAXEXTLEN)  {
				/*
				 * large extent case
				 */
				add_rt_dup_extent(rt_start, rt_len);
				rt_start = bno;
				rt_len = 1;
			} else
				rt_len++;
			break;
		}
	}

	/*
	 * catch tail-case, extent hitting the end of the ag
	 */
	if (rt_start != 0)
		add_rt_dup_extent(rt_start, rt_len);

	/*
	 * initialize bitmaps for all AGs
	 */
	for (i = 0; i < mp->m_sb.sb_agcount; i++)  {
		/*
		 * now reset the bitmap for all ags
		 */
		bzero(ba_bmap[i],
		    roundup((mp->m_sb.sb_agblocks+(NBBY/XR_BB)-1)/(NBBY/XR_BB),
						sizeof(__uint64_t)));
		for (j = 0; j < ag_hdr_block; j++)
			set_agbno_state(mp, i, j, XR_E_INUSE_FS);
	}
	set_bmap_rt(mp->m_sb.sb_rextents);
	set_bmap_log(mp);
	set_bmap_fs(mp);

	do_log(_("        - check for inodes claiming duplicate blocks...\n"));
	set_progress_msg(PROG_FMT_DUP_BLOCKS, (__uint64_t) mp->m_sb.sb_icount);

	/*
	 * ok, now process the inodes -- signal 2-pass check per inode.
	 * first pass checks if the inode conflicts with a known
	 * duplicate extent.  if so, the inode is cleared and second
	 * pass is skipped.  second pass sets the block bitmap
	 * for all blocks claimed by the inode.  directory
	 * and attribute processing is turned OFF since we did that
	 * already in phase 3.
	 */
	if (ag_stride) {
		int 	steps = (mp->m_sb.sb_agcount + ag_stride - 1) / ag_stride;
		for (i = 0; i < steps; i++)
			for (j = i; j < mp->m_sb.sb_agcount; j += ag_stride)
				queue_work(parallel_p4_process_aginodes, mp, j);
	} else {
		for (i = 0; i < mp->m_sb.sb_agcount; i++)
			parallel_p4_process_aginodes(mp, i);
	}

	wait_for_workers();
	print_final_rpt();

	/*
	 * free up memory used to track trealtime duplicate extents
	 */
	if (rt_start != 0)
		free_rt_dup_extent_tree(mp);

	/*
	 * ensure consistency of quota inode pointers in superblock,
	 * make sure they point to real inodes
	 */
	quotino_check(mp);
	quota_sb_check(mp);
}
