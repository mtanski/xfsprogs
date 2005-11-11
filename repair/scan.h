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
#ifndef _XR_SCAN_H
#define _XR_SCAN_H

struct blkmap;

void scan_sbtree(
	xfs_agblock_t	root,
	int		nlevels,
	xfs_agnumber_t	agno,
	int		suspect,
	void		(*func)(xfs_btree_sblock_t	*block,
				int			level,
				xfs_agblock_t		bno,
				xfs_agnumber_t		agno,
				int			suspect,
				int			isroot),
	int		isroot);

int scan_lbtree(
	xfs_dfsbno_t	root,
	int		nlevels,
	int		(*func)(xfs_btree_lblock_t	*block,
				int			level,
				int			type,
				int			whichfork,
				xfs_dfsbno_t		bno,
				xfs_ino_t		ino,
				xfs_drfsbno_t		*tot,
				__uint64_t		*nex,
				struct blkmap		**blkmapp,
				bmap_cursor_t		*bm_cursor,
				int			isroot,
				int			check_dups,
				int			*dirty),
	int		type,
	int		whichfork,
	xfs_ino_t	ino,
	xfs_drfsbno_t	*tot,
	__uint64_t	*nex,
	struct blkmap	**blkmapp,
	bmap_cursor_t	*bm_cursor,
	int		isroot,
	int		check_dups);

int scanfunc_bmap(
	xfs_btree_lblock_t	*ablock,
	int			level,
	int			type,
	int			whichfork,
	xfs_dfsbno_t		bno,
	xfs_ino_t		ino,
	xfs_drfsbno_t		*tot,
	__uint64_t		*nex,
	struct blkmap		**blkmapp,
	bmap_cursor_t		*bm_cursor,
	int			isroot,
	int			check_dups,
	int			*dirty);

void scanfunc_bno(
	xfs_btree_sblock_t	*ablock,
	int			level,
	xfs_agblock_t		bno,
	xfs_agnumber_t		agno,
	int			suspect,
	int			isroot);

void scanfunc_cnt(
	xfs_btree_sblock_t	*ablock,
	int			level,
	xfs_agblock_t		bno,
	xfs_agnumber_t		agno,
	int			suspect,
	int			isroot);

void
scanfunc_ino(
	xfs_btree_sblock_t	*ablock,
	int			level,
	xfs_agblock_t		bno,
	xfs_agnumber_t		agno,
	int			suspect,
	int			isroot);

#endif /* _XR_SCAN_H */
