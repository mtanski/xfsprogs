/*
 * Copyright (c) 2000-2006 Silicon Graphics, Inc.
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

#include <xfs.h>

/*
 * Mount initialization code establishing various mount
 * fields from the superblock associated with the given
 * mount structure.
 */
void
xfs_mount_common(xfs_mount_t *mp, xfs_sb_t *sbp)
{
	int	i;

	mp->m_agfrotor = mp->m_agirotor = 0;
	spinlock_init(&mp->m_agirotor_lock, "m_agirotor_lock");
	mp->m_maxagi = mp->m_sb.sb_agcount;
	mp->m_blkbit_log = sbp->sb_blocklog + XFS_NBBYLOG;
	mp->m_blkbb_log = sbp->sb_blocklog - BBSHIFT;
	mp->m_sectbb_log = sbp->sb_sectlog - BBSHIFT;
	mp->m_agno_log = xfs_highbit32(sbp->sb_agcount - 1) + 1;
	mp->m_agino_log = sbp->sb_inopblog + sbp->sb_agblklog;
	mp->m_litino = sbp->sb_inodesize -
		((uint)sizeof(xfs_dinode_core_t) + (uint)sizeof(xfs_agino_t));
	mp->m_blockmask = sbp->sb_blocksize - 1;
	mp->m_blockwsize = sbp->sb_blocksize >> XFS_WORDLOG;
	mp->m_blockwmask = mp->m_blockwsize - 1;
	INIT_LIST_HEAD(&mp->m_del_inodes);

	/*
	 * Setup for attributes, in case they get created.
	 * This value is for inodes getting attributes for the first time,
	 * the per-inode value is for old attribute values.
	 */
	ASSERT(sbp->sb_inodesize >= 256 && sbp->sb_inodesize <= 2048);
	switch (sbp->sb_inodesize) {
	case 256:
		mp->m_attroffset = XFS_LITINO(mp) -
				   XFS_BMDR_SPACE_CALC(MINABTPTRS);
		break;
	case 512:
	case 1024:
	case 2048:
		mp->m_attroffset = XFS_BMDR_SPACE_CALC(6 * MINABTPTRS);
		break;
	default:
		ASSERT(0);
	}
	ASSERT(mp->m_attroffset < XFS_LITINO(mp));

	for (i = 0; i < 2; i++) {
		mp->m_alloc_mxr[i] = XFS_BTREE_BLOCK_MAXRECS(sbp->sb_blocksize,
			xfs_alloc, i == 0);
		mp->m_alloc_mnr[i] = XFS_BTREE_BLOCK_MINRECS(sbp->sb_blocksize,
			xfs_alloc, i == 0);
	}
	for (i = 0; i < 2; i++) {
		mp->m_bmap_dmxr[i] = XFS_BTREE_BLOCK_MAXRECS(sbp->sb_blocksize,
			xfs_bmbt, i == 0);
		mp->m_bmap_dmnr[i] = XFS_BTREE_BLOCK_MINRECS(sbp->sb_blocksize,
			xfs_bmbt, i == 0);
	}
	for (i = 0; i < 2; i++) {
		mp->m_inobt_mxr[i] = XFS_BTREE_BLOCK_MAXRECS(sbp->sb_blocksize,
			xfs_inobt, i == 0);
		mp->m_inobt_mnr[i] = XFS_BTREE_BLOCK_MINRECS(sbp->sb_blocksize,
			xfs_inobt, i == 0);
	}

	mp->m_bsize = XFS_FSB_TO_BB(mp, 1);
	mp->m_ialloc_inos = (int)MAX((__uint16_t)XFS_INODES_PER_CHUNK,
					sbp->sb_inopblock);
	mp->m_ialloc_blks = mp->m_ialloc_inos >> sbp->sb_inopblog;
}

static struct {
    short offset;
    short type;	    /* 0 = integer
		     * 1 = binary / string (no translation)
		     */
} xfs_sb_info[] = {
    { offsetof(xfs_sb_t, sb_magicnum),	 0 },
    { offsetof(xfs_sb_t, sb_blocksize),	 0 },
    { offsetof(xfs_sb_t, sb_dblocks),	 0 },
    { offsetof(xfs_sb_t, sb_rblocks),	 0 },
    { offsetof(xfs_sb_t, sb_rextents),	 0 },
    { offsetof(xfs_sb_t, sb_uuid),	 1 },
    { offsetof(xfs_sb_t, sb_logstart),	 0 },
    { offsetof(xfs_sb_t, sb_rootino),	 0 },
    { offsetof(xfs_sb_t, sb_rbmino),	 0 },
    { offsetof(xfs_sb_t, sb_rsumino),	 0 },
    { offsetof(xfs_sb_t, sb_rextsize),	 0 },
    { offsetof(xfs_sb_t, sb_agblocks),	 0 },
    { offsetof(xfs_sb_t, sb_agcount),	 0 },
    { offsetof(xfs_sb_t, sb_rbmblocks),	 0 },
    { offsetof(xfs_sb_t, sb_logblocks),	 0 },
    { offsetof(xfs_sb_t, sb_versionnum), 0 },
    { offsetof(xfs_sb_t, sb_sectsize),	 0 },
    { offsetof(xfs_sb_t, sb_inodesize),	 0 },
    { offsetof(xfs_sb_t, sb_inopblock),	 0 },
    { offsetof(xfs_sb_t, sb_fname[0]),	 1 },
    { offsetof(xfs_sb_t, sb_blocklog),	 0 },
    { offsetof(xfs_sb_t, sb_sectlog),	 0 },
    { offsetof(xfs_sb_t, sb_inodelog),	 0 },
    { offsetof(xfs_sb_t, sb_inopblog),	 0 },
    { offsetof(xfs_sb_t, sb_agblklog),	 0 },
    { offsetof(xfs_sb_t, sb_rextslog),	 0 },
    { offsetof(xfs_sb_t, sb_inprogress), 0 },
    { offsetof(xfs_sb_t, sb_imax_pct),	 0 },
    { offsetof(xfs_sb_t, sb_icount),	 0 },
    { offsetof(xfs_sb_t, sb_ifree),	 0 },
    { offsetof(xfs_sb_t, sb_fdblocks),	 0 },
    { offsetof(xfs_sb_t, sb_frextents),	 0 },
    { offsetof(xfs_sb_t, sb_uquotino),	 0 },
    { offsetof(xfs_sb_t, sb_gquotino),	 0 },
    { offsetof(xfs_sb_t, sb_qflags),	 0 },
    { offsetof(xfs_sb_t, sb_flags),	 0 },
    { offsetof(xfs_sb_t, sb_shared_vn),	 0 },
    { offsetof(xfs_sb_t, sb_inoalignmt), 0 },
    { offsetof(xfs_sb_t, sb_unit),	 0 },
    { offsetof(xfs_sb_t, sb_width),	 0 },
    { offsetof(xfs_sb_t, sb_dirblklog),	 0 },
    { offsetof(xfs_sb_t, sb_logsectlog), 0 },
    { offsetof(xfs_sb_t, sb_logsectsize),0 },
    { offsetof(xfs_sb_t, sb_logsunit),	 0 },
    { offsetof(xfs_sb_t, sb_features2),	 0 },
    { offsetof(xfs_sb_t, sb_bad_features2), 0 },
    { sizeof(xfs_sb_t),			 0 }
};

/*
 * xfs_xlatesb
 *     data	  - on disk version of sb
 *     sb	  - a superblock
 *     dir	  - conversion direction: <0 - convert sb to buf
 *					  >0 - convert buf to sb
 *     arch	  - architecture to read/write from/to buf
 *     fields	  - which fields to copy (bitmask)
 */
void
xfs_xlatesb(
	void		*data,
	xfs_sb_t	*sb,
	int		dir,
	__int64_t	fields)
{
	xfs_caddr_t	buf_ptr;
	xfs_caddr_t	mem_ptr;
	xfs_sb_field_t	f;
	int		first;
	int		size;

	ASSERT(dir);
	ASSERT(fields);

	if (!fields)
		return;

	buf_ptr = (xfs_caddr_t)data;
	mem_ptr = (xfs_caddr_t)sb;

	while (fields) {
		f = (xfs_sb_field_t)xfs_lowbit64((__uint64_t)fields);
		first = xfs_sb_info[f].offset;
		size = xfs_sb_info[f + 1].offset - first;

		ASSERT(xfs_sb_info[f].type == 0 || xfs_sb_info[f].type == 1);

		if (size == 1 || xfs_sb_info[f].type == 1) {
			if (dir > 0) {
				memcpy(mem_ptr + first, buf_ptr + first, size);
			} else {
				memcpy(buf_ptr + first, mem_ptr + first, size);
			}
		} else {
			switch (size) {
			case 2:
				INT_XLATE(*(__uint16_t*)(buf_ptr+first),
					  *(__uint16_t*)(mem_ptr+first),
					  dir, ARCH_CONVERT);
				break;
			case 4:
				INT_XLATE(*(__uint32_t*)(buf_ptr+first),
					  *(__uint32_t*)(mem_ptr+first),
					  dir, ARCH_CONVERT);
				break;
			case 8:
				INT_XLATE(*(__uint64_t*)(buf_ptr+first),
					  *(__uint64_t*)(mem_ptr+first), dir, ARCH_CONVERT);
				break;
			default:
				ASSERT(0);
			}
		}

		fields &= ~(1LL << f);
	}
}

/*
 * xfs_mod_sb() can be used to copy arbitrary changes to the
 * in-core superblock into the superblock buffer to be logged.
 * It does not provide the higher level of locking that is
 * needed to protect the in-core superblock from concurrent
 * access.
 */
void
xfs_mod_sb(xfs_trans_t *tp, __int64_t fields)
{
	xfs_buf_t	*bp;
	int		first;
	int		last;
	xfs_mount_t	*mp;
	xfs_sb_t	*sbp;
	xfs_sb_field_t	f;

	ASSERT(fields);
	if (!fields)
		return;
	mp = tp->t_mountp;
	bp = xfs_trans_getsb(tp, mp, 0);
	sbp = XFS_BUF_TO_SBP(bp);
	first = sizeof(xfs_sb_t);
	last = 0;

	/* translate/copy */
	xfs_xlatesb(XFS_BUF_PTR(bp), &(mp->m_sb), -1, fields);

	/* find modified range */
	f = (xfs_sb_field_t)xfs_lowbit64((__uint64_t)fields);
	ASSERT((1LL << f) & XFS_SB_MOD_BITS);
	first = xfs_sb_info[f].offset;
	f = (xfs_sb_field_t)xfs_highbit64((__uint64_t)fields);
	ASSERT((1LL << f) & XFS_SB_MOD_BITS);
	last = xfs_sb_info[f + 1].offset - 1;

	xfs_trans_log_buf(tp, bp, first, last);
}

xfs_agnumber_t
xfs_initialize_perag(xfs_mount_t *mp, xfs_agnumber_t agcount)
{
	xfs_agnumber_t	index, max_metadata;
	xfs_perag_t	*pag;
	xfs_agino_t	agino;
	xfs_ino_t	ino;
	xfs_sb_t	*sbp = &mp->m_sb;
	xfs_ino_t	max_inum = XFS_MAXINUMBER_32;

	/* Check to see if the filesystem can overflow 32 bit inodes */
	agino = XFS_OFFBNO_TO_AGINO(mp, sbp->sb_agblocks - 1, 0);
	ino = XFS_AGINO_TO_INO(mp, agcount - 1, agino);

	/* Clear the mount flag if no inode can overflow 32 bits
	 * on this filesystem, or if specifically requested..
	 */
	if ((mp->m_flags & XFS_MOUNT_32BITINOOPT) && ino > max_inum) {
		mp->m_flags |= XFS_MOUNT_32BITINODES;
	} else {
		mp->m_flags &= ~XFS_MOUNT_32BITINODES;
	}

	/* If we can overflow then setup the ag headers accordingly */
	if (mp->m_flags & XFS_MOUNT_32BITINODES) {
		/* Calculate how much should be reserved for inodes to
		 * meet the max inode percentage.
		 */
		if (mp->m_maxicount) {
			__uint64_t	icount;

			icount = sbp->sb_dblocks * sbp->sb_imax_pct;
			do_div(icount, 100);
			icount += sbp->sb_agblocks - 1;
			do_div(icount, sbp->sb_agblocks);
			max_metadata = icount;
		} else {
			max_metadata = agcount;
		}
		for (index = 0; index < agcount; index++) {
			ino = XFS_AGINO_TO_INO(mp, index, agino);
			if (ino > max_inum) {
				index++;
				break;
			}

			/* This ag is prefered for inodes */
			pag = &mp->m_perag[index];
			pag->pagi_inodeok = 1;
			if (index < max_metadata)
				pag->pagf_metadata = 1;
		}
	} else {
		/* Setup default behavior for smaller filesystems */
		for (index = 0; index < agcount; index++) {
			pag = &mp->m_perag[index];
			pag->pagi_inodeok = 1;
		}
	}
	return index;
}

/*
 * xfs_initialize_perag_data
 *
 * Read in each per-ag structure so we can count up the number of
 * allocated inodes, free inodes and used filesystem blocks as this
 * information is no longer persistent in the superblock. Once we have
 * this information, write it into the in-core superblock structure.
 */
STATIC int
xfs_initialize_perag_data(xfs_mount_t *mp, xfs_agnumber_t agcount)
{
	xfs_agnumber_t	index;
	xfs_perag_t	*pag;
	xfs_sb_t	*sbp = &mp->m_sb;
	__uint64_t	ifree = 0;
	__uint64_t	ialloc = 0;
	__uint64_t	bfree = 0;
	__uint64_t	bfreelst = 0;
	__uint64_t	btree = 0;
	int		error;
	int		s;

	for (index = 0; index < agcount; index++) {
		/*
		 * read the agf, then the agi. This gets us
		 * all the information we need and populates the
		 * per-ag structures for us.
		 */
		error = xfs_alloc_pagf_init(mp, NULL, index, 0);
		if (error)
			return error;

		error = xfs_ialloc_pagi_init(mp, NULL, index);
		if (error)
			return error;
		pag = &mp->m_perag[index];
		ifree += pag->pagi_freecount;
		ialloc += pag->pagi_count;
		bfree += pag->pagf_freeblks;
		bfreelst += pag->pagf_flcount;
		btree += pag->pagf_btreeblks;
	}
	/*
	 * Overwrite incore superblock counters with just-read data
	 */
	s = XFS_SB_LOCK(mp);
	sbp->sb_ifree = ifree;
	sbp->sb_icount = ialloc;
	sbp->sb_fdblocks = bfree + bfreelst + btree;
	XFS_SB_UNLOCK(mp, s);
	return 0;
}
