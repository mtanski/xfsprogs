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

extern const field_t	dir2_flds[];
extern const field_t	dir2_hfld[];
extern const field_t	dir2_block_tail_flds[];
extern const field_t	dir2_data_free_flds[];
extern const field_t	dir2_data_hdr_flds[];
extern const field_t	dir2_data_union_flds[];
extern const field_t	dir2_free_hdr_flds[];
extern const field_t	dir2_leaf_entry_flds[];
extern const field_t	dir2_leaf_hdr_flds[];
extern const field_t	dir2_leaf_tail_flds[];

extern const field_t	da_blkinfo_flds[];
extern const field_t	da_node_entry_flds[];
extern const field_t	da_node_hdr_flds[];

/*
 * generic dir2 structures used by xfs_db
 */
typedef union {
	xfs_dir2_data_entry_t	entry;
	xfs_dir2_data_unused_t	unused;
} xfs_dir2_data_union_t;

typedef struct xfs_dir2_data {
	xfs_dir2_data_hdr_t	hdr;		/* magic XFS_DIR2_DATA_MAGIC */
	xfs_dir2_data_union_t	u[1];
} xfs_dir2_data_t;

typedef struct xfs_dir2_block {
	xfs_dir2_data_hdr_t	hdr;		/* magic XFS_DIR2_BLOCK_MAGIC */
	xfs_dir2_data_union_t	u[1];
	xfs_dir2_leaf_entry_t	leaf[1];
	xfs_dir2_block_tail_t	tail;
} xfs_dir2_block_t;

typedef struct xfs_dir2_sf {
	xfs_dir2_sf_hdr_t	hdr;		/* shortform header */
	xfs_dir2_sf_entry_t	list[1];	/* shortform entries */
} xfs_dir2_sf_t;

static inline xfs_dir2_inou_t *xfs_dir2_sf_inumberp(xfs_dir2_sf_entry_t *sfep)
{
	return (xfs_dir2_inou_t *)&(sfep)->name[(sfep)->namelen];
}

extern int	dir2_data_union_size(void *obj, int startoff, int idx);
extern int	dir2_size(void *obj, int startoff, int idx);
