#ifndef _XFS_REPAIR_PREFETCH_H
#define	_XFS_REPAIR_PREFETCH_H

struct blkmap;
struct da_bt_cursor;
struct xfs_mount;

extern 	int do_prefetch;

struct ino_tree_node *prefetch_inode_chunks(
	struct xfs_mount *,
	xfs_agnumber_t,
	struct ino_tree_node *);

extern void prefetch_dir1(
	struct xfs_mount	*mp,
	xfs_dablk_t		bno,
	struct da_bt_cursor	*da_cursor);

extern void prefetch_dir2(
	struct xfs_mount	*mp,
	struct blkmap		*blkmap);

extern void prefetch_p6_dir1(
	struct xfs_mount	*mp,
	xfs_ino_t		ino,
	struct xfs_inode	*ip,
	xfs_dablk_t		da_bno,
	xfs_fsblock_t		*fblockp);

extern void prefetch_p6_dir2(
	struct xfs_mount	*mp,
	struct xfs_inode	*ip);

extern void prefetch_sb(
	struct xfs_mount	*mp,
	xfs_agnumber_t		agno);

extern void prefetch_roots(
	struct xfs_mount 	*mp,
	xfs_agnumber_t 		agno,
	xfs_agf_t		*agf,
	xfs_agi_t		*agi);

#endif /* _XFS_REPAIR_PREFETCH_H */
