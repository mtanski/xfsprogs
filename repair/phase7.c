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
#include "versions.h"
#include "prefetch.h"
#include "progress.h"
#include "threads.h"

/* dinoc is a pointer to the IN-CORE dinode core */
void
set_nlinks(xfs_dinode_core_t	*dinoc,
		xfs_ino_t	ino,
		__uint32_t	nrefs,
		int		*dirty)
{
	if (!no_modify)  {
		if (dinoc->di_nlink != nrefs)  {
			*dirty = 1;
			do_warn(
		_("resetting inode %llu nlinks from %d to %d\n"),
				ino, dinoc->di_nlink, nrefs);

			if (nrefs > XFS_MAXLINK_1)  {
				ASSERT(fs_inode_nlink);
				do_warn(
_("nlinks %d will overflow v1 ino, ino %llu will be converted to version 2\n"),
					nrefs, ino);

			}
			dinoc->di_nlink = nrefs;
		}
	} else  {
		if (dinoc->di_nlink != nrefs)
			do_warn(
			_("would have reset inode %llu nlinks from %d to %d\n"),
				ino, dinoc->di_nlink, nrefs);
	}
}

void
phase7_alt_function(xfs_mount_t *mp, xfs_agnumber_t agno)
{
	register ino_tree_node_t *irec;
	int			j;
	int			chunk_dirty;
	int			inode_dirty;
	xfs_ino_t		ino;
	__uint32_t		nrefs;
	xfs_agblock_t		agbno;
	xfs_dinode_t		*dip;
	ino_tree_node_t 	*ino_ra;
	xfs_buf_t		*bp;

	if (verbose)
		do_log(_("        - agno = %d\n"), agno);

	ino_ra = prefetch_inode_chunks(mp, agno, NULL);

	/*
	 * read on-disk inodes in chunks. then,
	 * look at each on-disk inode 1 at a time.
	 * if the number of links is bad, reset it.
	 */

	irec = findfirst_inode_rec(agno);

	while (irec != NULL)  {

		if (ino_ra && (irec->ino_startnum >= ino_ra->ino_startnum))
			ino_ra = prefetch_inode_chunks(mp, agno, ino_ra);

		agbno = XFS_AGINO_TO_AGBNO(mp, irec->ino_startnum);
		bp = libxfs_readbuf(mp->m_dev,
				XFS_AGB_TO_DADDR(mp, agno, agbno),
				XFS_FSB_TO_BB(mp, XFS_IALLOC_BLOCKS(mp)), 0);
		if (bp == NULL) {
			if (!no_modify) {
				do_error(
	_("cannot read inode %llu, disk block %lld, cnt %d\n"),
				XFS_AGINO_TO_INO(mp, agno, irec->ino_startnum),
				XFS_AGB_TO_DADDR(mp, agno, agbno),
				(int)XFS_FSB_TO_BB(mp, XFS_IALLOC_BLOCKS(mp)));
				/* NOT REACHED */
			}
			do_warn(
	_("cannot read inode %llu, disk block %lld, cnt %d\n"),
				XFS_AGINO_TO_INO(mp, agno, irec->ino_startnum),
				XFS_AGB_TO_DADDR(mp, agno, agbno),
				(int)XFS_FSB_TO_BB(mp, XFS_IALLOC_BLOCKS(mp)));

			irec = next_ino_rec(irec);
			continue;	/* while */
		}
		chunk_dirty = 0;
		for (j = 0; j < XFS_INODES_PER_CHUNK; j++)  {
			assert(is_inode_confirmed(irec, j));

			if (is_inode_free(irec, j))
				continue;

			assert(no_modify || is_inode_reached(irec, j));
			assert(no_modify ||
					is_inode_referenced(irec, j));

			nrefs = num_inode_references(irec, j);

			ino = XFS_AGINO_TO_INO(mp, agno,
				irec->ino_startnum + j);

			dip = (xfs_dinode_t *)(XFS_BUF_PTR(bp) +
					(j << mp->m_sb.sb_inodelog));
			
			inode_dirty = 0;

			/* Swap the fields we care about to native format */
			dip->di_core.di_magic = INT_GET(dip->di_core.di_magic, 
							ARCH_CONVERT);
			dip->di_core.di_onlink = INT_GET(dip->di_core.di_onlink, 
							ARCH_CONVERT);
			if (INT_GET(dip->di_core.di_version, ARCH_CONVERT) ==
					XFS_DINODE_VERSION_1) 
				dip->di_core.di_nlink = dip->di_core.di_onlink;
			else 
				dip->di_core.di_nlink = 
						INT_GET(dip->di_core.di_nlink, 
							ARCH_CONVERT);

			if (dip->di_core.di_magic != XFS_DINODE_MAGIC) {
				if (!no_modify) {
					do_error(
	_("ino: %llu, bad d_inode magic saw: (0x%x) expecting (0x%x)\n"),
				ino, dip->di_core.di_magic, XFS_DINODE_MAGIC);
					/* NOT REACHED */
				}
				do_warn(
	_("ino: %llu, bad d_inode magic saw: (0x%x) expecting (0x%x)\n"),
				ino, dip->di_core.di_magic, XFS_DINODE_MAGIC);
				continue;
			}
			/*
			 * compare and set links for all inodes
			 * but the lost+found inode.  we keep
			 * that correct as we go.
			 */
			if (dip->di_core.di_nlink != nrefs) {
				if (ino != orphanage_ino) {
					set_nlinks(&dip->di_core, ino,
							nrefs, &inode_dirty);
				}
			}

			/* Swap the fields back */
			dip->di_core.di_magic = INT_GET(dip->di_core.di_magic, 
					ARCH_CONVERT);
			if (inode_dirty && INT_GET(dip->di_core.di_version, 
					ARCH_CONVERT) == XFS_DINODE_VERSION_1) {
				if (!XFS_SB_VERSION_HASNLINK(&mp->m_sb)) {
					ASSERT(dip->di_core.di_nlink <= 
							XFS_MAXLINK_1);
					INT_SET(dip->di_core.di_onlink, 
							ARCH_CONVERT,
							dip->di_core.di_nlink);
					dip->di_core.di_nlink = 
						INT_GET(dip->di_core.di_nlink, 
							ARCH_CONVERT);
				} else {
					/* superblock support v2 nlinks */
					INT_SET(dip->di_core.di_version, 
						ARCH_CONVERT, XFS_DINODE_VERSION_2);
					dip->di_core.di_nlink = 
						INT_GET(dip->di_core.di_nlink, 
							ARCH_CONVERT);
					dip->di_core.di_onlink = 0;
					memset(&(dip->di_core.di_pad[0]), 0,
						sizeof(dip->di_core.di_pad));
				}	
			} else {
				dip->di_core.di_nlink = 
						INT_GET(dip->di_core.di_nlink, 
							ARCH_CONVERT);
				dip->di_core.di_onlink = 
						INT_GET(dip->di_core.di_onlink, 
							ARCH_CONVERT);
			}
			chunk_dirty |= inode_dirty;
		}

		if (chunk_dirty)
			libxfs_writebuf(bp, 0);
		else
			libxfs_putbuf(bp);

		irec = next_ino_rec(irec);
		PROG_RPT_INC(prog_rpt_done[agno], XFS_INODES_PER_CHUNK);
	}
}

void
phase7_alt(xfs_mount_t *mp)
{
	int		i;

	set_progress_msg(no_modify ? PROGRESS_FMT_VRFY_LINK : PROGRESS_FMT_CORR_LINK,
		(__uint64_t) mp->m_sb.sb_icount);
	libxfs_bcache_purge();

	for (i = 0; i < glob_agcount; i++)  {
		queue_work(phase7_alt_function, mp, i);
	}
	wait_for_workers();
	print_final_rpt();
}

void
phase7(xfs_mount_t *mp)
{
	ino_tree_node_t		*irec;
	xfs_inode_t		*ip;
	xfs_trans_t		*tp;
	int			i;
	int			j;
	int			error;
	int			dirty;
	xfs_ino_t		ino;
	__uint32_t		nrefs;

	if (!no_modify)
		do_log(_("Phase 7 - verify and correct link counts...\n"));
	else
		do_log(_("Phase 7 - verify link counts...\n"));


	if (do_prefetch) {
		phase7_alt(mp);
		return;
	}

	tp = libxfs_trans_alloc(mp, XFS_TRANS_REMOVE);

	error = libxfs_trans_reserve(tp, (no_modify ? 0 : 10),
			XFS_REMOVE_LOG_RES(mp), 0, XFS_TRANS_PERM_LOG_RES,
			XFS_REMOVE_LOG_COUNT);

	ASSERT(error == 0);

	/*
	 * for each ag, look at each inode 1 at a time using the
	 * sim code.  if the number of links is bad, reset it,
	 * log the inode core, commit the transaction, and
	 * allocate a new transaction
	 */
	for (i = 0; i < glob_agcount; i++)  {
		irec = findfirst_inode_rec(i);

		while (irec != NULL)  {
			for (j = 0; j < XFS_INODES_PER_CHUNK; j++)  {
				ASSERT(is_inode_confirmed(irec, j));

				if (is_inode_free(irec, j))
					continue;

				ASSERT(no_modify || is_inode_reached(irec, j));
				ASSERT(no_modify ||
						is_inode_referenced(irec, j));

				nrefs = num_inode_references(irec, j);

				ino = XFS_AGINO_TO_INO(mp, i,
					irec->ino_startnum + j);

				error = libxfs_trans_iget(mp, tp, ino, 0, 0, &ip);

				if (error)  {
					if (!no_modify)
						do_error(
				_("couldn't map inode %llu, err = %d\n"),
							ino, error);
					else  {
						do_warn(
	_("couldn't map inode %llu, err = %d, can't compare link counts\n"),
							ino, error);
						continue;
					}
				}

				dirty = 0;

				/*
				 * compare and set links for all inodes
				 * but the lost+found inode.  we keep
				 * that correct as we go.
				 */
				if (ino != orphanage_ino)
					set_nlinks(&ip->i_d, ino, nrefs,
							&dirty);

				if (!dirty)  {
					libxfs_trans_iput(tp, ip, 0);
				} else  {
					libxfs_trans_log_inode(tp, ip,
							XFS_ILOG_CORE);
					/*
					 * no need to do a bmap finish since
					 * we're not allocating anything
					 */
					ASSERT(error == 0);
					error = libxfs_trans_commit(tp,
						XFS_TRANS_RELEASE_LOG_RES|
						XFS_TRANS_SYNC, NULL);

					ASSERT(error == 0);

					tp = libxfs_trans_alloc(mp,
							XFS_TRANS_REMOVE);

					error = libxfs_trans_reserve(tp,
						(no_modify ? 0 : 10),
						XFS_REMOVE_LOG_RES(mp),
						0, XFS_TRANS_PERM_LOG_RES,
						XFS_REMOVE_LOG_COUNT);
					ASSERT(error == 0);
				}
			}
			irec = next_ino_rec(irec);
		}
	}

	/*
	 * always have one unfinished transaction coming out
	 * of the loop.  cancel it.
	 */
	libxfs_trans_cancel(tp, XFS_TRANS_RELEASE_LOG_RES);
}
