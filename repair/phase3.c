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
#include "agheader.h"
#include "incore.h"
#include "protos.h"
#include "err_protos.h"
#include "dinode.h"
#include "threads.h"
#include "progress.h"

/*
 * walks an unlinked list, returns 1 on an error (bogus pointer) or
 * I/O error
 */
int
walk_unlinked_list(xfs_mount_t *mp, xfs_agnumber_t agno, xfs_agino_t start_ino)
{
	xfs_buf_t *bp;
	xfs_dinode_t *dip;
	xfs_agino_t current_ino = start_ino;
	xfs_agblock_t agbno;
	int state;

	while (current_ino != NULLAGINO)  {
		if (verify_aginum(mp, agno, current_ino))
			return(1);
		if ((bp = get_agino_buf(mp, agno, current_ino, &dip)) == NULL)
			return(1);
		/*
		 * if this looks like a decent inode, then continue
		 * following the unlinked pointers.  If not, bail.
		 */
		if (verify_dinode(mp, dip, agno, current_ino) == 0)  {
			/*
			 * check if the unlinked list points to an unknown
			 * inode.  if so, put it on the uncertain inode list
			 * and set block map appropriately.
			 */
			if (find_inode_rec(agno, current_ino) == NULL)  {
				add_aginode_uncertain(agno, current_ino, 1);
				agbno = XFS_AGINO_TO_AGBNO(mp, current_ino);

				PREPAIR_RW_WRITE_LOCK(&per_ag_lock[agno]);
				switch (state = get_agbno_state(mp,
							agno, agbno))  {
				case XR_E_UNKNOWN:
				case XR_E_FREE:
				case XR_E_FREE1:
					set_agbno_state(mp, agno, agbno,
						XR_E_INO);
					PREPAIR_RW_UNLOCK(&per_ag_lock[agno]);
					break;
				case XR_E_BAD_STATE:
					PREPAIR_RW_UNLOCK(&per_ag_lock[agno]);
					do_error(_(
						"bad state in block map %d\n"),
						state);
					abort();
					break;
				default:
					/*
					 * the block looks like inodes
					 * so be conservative and try
					 * to scavenge what's in there.
					 * if what's there is completely
					 * bogus, it'll show up later
					 * and the inode will be trashed
					 * anyway, hopefully without
					 * losing too much other data
					 */
					set_agbno_state(mp, agno, agbno,
						XR_E_INO);
					PREPAIR_RW_UNLOCK(&per_ag_lock[agno]);
					break;
				}
			}
			current_ino = dip->di_next_unlinked;
		} else  {
			current_ino = NULLAGINO;;
		}
		libxfs_putbuf(bp);
	}

	return(0);
}

void
process_agi_unlinked(xfs_mount_t *mp, xfs_agnumber_t agno)
{
	xfs_agnumber_t i;
	xfs_buf_t *bp;
	xfs_agi_t *agip;
	int err = 0;
	int agi_dirty = 0;

	bp = libxfs_readbuf(mp->m_dev,
			XFS_AG_DADDR(mp, agno, XFS_AGI_DADDR(mp)),
			mp->m_sb.sb_sectsize/BBSIZE, 0);
	if (!bp)
		do_error(_("cannot read agi block %lld for ag %u\n"),
			XFS_AG_DADDR(mp, agno, XFS_AGI_DADDR(mp)), agno);

	agip = XFS_BUF_TO_AGI(bp);

	ASSERT(no_modify || INT_GET(agip->agi_seqno, ARCH_CONVERT) == agno);

	for (i = 0; i < XFS_AGI_UNLINKED_BUCKETS; i++)  {
		if (INT_GET(agip->agi_unlinked[i], ARCH_CONVERT) != NULLAGINO)  {
			err += walk_unlinked_list(mp, agno, INT_GET(
					agip->agi_unlinked[i], ARCH_CONVERT));
			/*
			 * clear the list
			 */
			if (!no_modify)  {
				INT_SET(agip->agi_unlinked[i], ARCH_CONVERT,
					NULLAGINO);
				agi_dirty = 1;
			}
		}
	}

	if (err)
		do_warn(_("error following ag %d unlinked list\n"), agno);

	ASSERT(agi_dirty == 0 || (agi_dirty && !no_modify));

	if (agi_dirty && !no_modify)
		libxfs_writebuf(bp, 0);
	else
		libxfs_putbuf(bp);
}

void
parallel_p3_process_aginodes(xfs_mount_t *mp, xfs_agnumber_t agno)
{
	/*
	 * turn on directory processing (inode discovery) and
	 * attribute processing (extra_attr_check)
	 */
	do_log(_("        - agno = %d\n"), agno);
	process_aginodes(mp, agno, 1, 0, 1);
}

void
phase3(xfs_mount_t *mp)
{
	int i, j;

	do_log(_("Phase 3 - for each AG...\n"));
	if (!no_modify)
		do_log(_("        - scan and clear agi unlinked lists...\n"));
	else
		do_log(_("        - scan (but don't clear) agi unlinked lists...\n"));

	set_progress_msg(PROG_FMT_AGI_UNLINKED, (__uint64_t) glob_agcount);

	/*
	 * first, let's look at the possibly bogus inodes
	 */
	for (i = 0; i < mp->m_sb.sb_agcount; i++)  {
		/*
		 * walk unlinked list to add more potential inodes to list
		 */
		process_agi_unlinked(mp, i);
		check_uncertain_aginodes(mp, i);
		PROG_RPT_INC(prog_rpt_done[i], 1);
	}
	print_final_rpt();

	/* ok, now that the tree's ok, let's take a good look */

	do_log(_(
	    "        - process known inodes and perform inode discovery...\n"));

	set_progress_msg(PROG_FMT_PROCESS_INO, (__uint64_t) mp->m_sb.sb_icount);
	if (ag_stride) {
		int 	steps = (mp->m_sb.sb_agcount + ag_stride - 1) / ag_stride;
		for (i = 0; i < steps; i++)
			for (j = i; j < mp->m_sb.sb_agcount; j += ag_stride)
				queue_work(parallel_p3_process_aginodes, mp, j);
	} else {
		for (i = 0; i < mp->m_sb.sb_agcount; i++)
			parallel_p3_process_aginodes(mp, i);
	}
	wait_for_workers();
	print_final_rpt();

	/*
	 * process newly discovered inode chunks
	 */
	do_log(_("        - process newly discovered inodes...\n"));
	set_progress_msg(PROG_FMT_NEW_INODES, (__uint64_t) glob_agcount);
	do  {
		/*
		 * have to loop until no ag has any uncertain
		 * inodes
		 */
		j = 0;
		for (i = 0; i < mp->m_sb.sb_agcount; i++)  {
			j += process_uncertain_aginodes(mp, i);
#ifdef XR_INODE_TRACE
			fprintf(stderr,
				"\t\t phase 3 - process_uncertain_inodes returns %d\n", j);
#endif
			PROG_RPT_INC(prog_rpt_done[i], 1);
		}
	} while (j != 0);
	print_final_rpt();
}
