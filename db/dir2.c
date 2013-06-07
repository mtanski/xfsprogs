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
#include "bit.h"
#include "type.h"
#include "faddr.h"
#include "fprint.h"
#include "field.h"
#include "dir2.h"
#include "init.h"

static int	dir2_block_hdr_count(void *obj, int startoff);
static int	dir2_block_leaf_count(void *obj, int startoff);
static int	dir2_block_leaf_offset(void *obj, int startoff, int idx);
static int	dir2_block_tail_count(void *obj, int startoff);
static int	dir2_block_tail_offset(void *obj, int startoff, int idx);
static int	dir2_block_u_count(void *obj, int startoff);
static int	dir2_block_u_offset(void *obj, int startoff, int idx);
static int	dir2_data_union_freetag_count(void *obj, int startoff);
static int	dir2_data_union_inumber_count(void *obj, int startoff);
static int	dir2_data_union_length_count(void *obj, int startoff);
static int	dir2_data_union_name_count(void *obj, int startoff);
static int	dir2_data_union_namelen_count(void *obj, int startoff);
static int	dir2_data_union_tag_count(void *obj, int startoff);
static int	dir2_data_union_tag_offset(void *obj, int startoff, int idx);
static int	dir2_data_hdr_count(void *obj, int startoff);
static int	dir2_data_u_count(void *obj, int startoff);
static int	dir2_data_u_offset(void *obj, int startoff, int idx);
static int	dir2_free_bests_count(void *obj, int startoff);
static int	dir2_free_hdr_count(void *obj, int startoff);
static int	dir2_leaf_bests_count(void *obj, int startoff);
static int	dir2_leaf_bests_offset(void *obj, int startoff, int idx);
static int	dir2_leaf_ents_count(void *obj, int startoff);
static int	dir2_leaf_hdr_count(void *obj, int startoff);
static int	dir2_leaf_tail_count(void *obj, int startoff);
static int	dir2_leaf_tail_offset(void *obj, int startoff, int idx);
static int	dir2_node_btree_count(void *obj, int startoff);
static int	dir2_node_hdr_count(void *obj, int startoff);

const field_t	dir2_hfld[] = {
	{ "", FLDT_DIR2, OI(0), C1, 0, TYP_NONE },
	{ NULL }
};

#define	BOFF(f)	bitize(offsetof(struct xfs_dir2_data_hdr, f))
#define	DOFF(f)	bitize(offsetof(struct xfs_dir2_data_hdr, f))
#define	FOFF(f)	bitize(offsetof(struct xfs_dir2_free, f))
#define	LOFF(f)	bitize(offsetof(struct xfs_dir2_leaf, f))
#define	NOFF(f)	bitize(offsetof(struct xfs_da_intnode, f))
const field_t	dir2_flds[] = {
	{ "bhdr", FLDT_DIR2_DATA_HDR, OI(BOFF(magic)), dir2_block_hdr_count,
	  FLD_COUNT, TYP_NONE },
	{ "bu", FLDT_DIR2_DATA_UNION, dir2_block_u_offset, dir2_block_u_count,
	  FLD_ARRAY|FLD_OFFSET|FLD_COUNT, TYP_NONE },
	{ "bleaf", FLDT_DIR2_LEAF_ENTRY, dir2_block_leaf_offset,
	  dir2_block_leaf_count, FLD_ARRAY|FLD_OFFSET|FLD_COUNT, TYP_NONE },
	{ "btail", FLDT_DIR2_BLOCK_TAIL, dir2_block_tail_offset,
	  dir2_block_tail_count, FLD_OFFSET|FLD_COUNT, TYP_NONE },
	{ "dhdr", FLDT_DIR2_DATA_HDR, OI(DOFF(magic)), dir2_data_hdr_count,
	  FLD_COUNT, TYP_NONE },
	{ "du", FLDT_DIR2_DATA_UNION, dir2_data_u_offset, dir2_data_u_count,
	  FLD_ARRAY|FLD_OFFSET|FLD_COUNT, TYP_NONE },
	{ "lhdr", FLDT_DIR2_LEAF_HDR, OI(LOFF(hdr)), dir2_leaf_hdr_count,
	  FLD_COUNT, TYP_NONE },
	{ "lbests", FLDT_DIR2_DATA_OFF, dir2_leaf_bests_offset,
	  dir2_leaf_bests_count, FLD_ARRAY|FLD_OFFSET|FLD_COUNT, TYP_NONE },
	{ "lents", FLDT_DIR2_LEAF_ENTRY, OI(LOFF(__ents)), dir2_leaf_ents_count,
	  FLD_ARRAY|FLD_COUNT, TYP_NONE },
	{ "ltail", FLDT_DIR2_LEAF_TAIL, dir2_leaf_tail_offset,
	  dir2_leaf_tail_count, FLD_OFFSET|FLD_COUNT, TYP_NONE },
	{ "nhdr", FLDT_DA_NODE_HDR, OI(NOFF(hdr)), dir2_node_hdr_count,
	  FLD_COUNT, TYP_NONE },
	{ "nbtree", FLDT_DA_NODE_ENTRY, OI(NOFF(__btree)), dir2_node_btree_count,
	  FLD_ARRAY|FLD_COUNT, TYP_NONE },
	{ "fhdr", FLDT_DIR2_FREE_HDR, OI(FOFF(hdr)), dir2_free_hdr_count,
	  FLD_COUNT, TYP_NONE },
	{ "fbests", FLDT_DIR2_DATA_OFFNZ, OI(FOFF(bests)),
	  dir2_free_bests_count, FLD_ARRAY|FLD_COUNT, TYP_NONE },
	{ NULL }
};

#define	BTOFF(f)	bitize(offsetof(xfs_dir2_block_tail_t, f))
const field_t	dir2_block_tail_flds[] = {
	{ "count", FLDT_UINT32D, OI(BTOFF(count)), C1, 0, TYP_NONE },
	{ "stale", FLDT_UINT32D, OI(BTOFF(stale)), C1, 0, TYP_NONE },
	{ NULL }
};

#define	DFOFF(f)	bitize(offsetof(xfs_dir2_data_free_t, f))
const field_t	dir2_data_free_flds[] = {
	{ "offset", FLDT_DIR2_DATA_OFF, OI(DFOFF(offset)), C1, 0, TYP_NONE },
	{ "length", FLDT_DIR2_DATA_OFF, OI(DFOFF(length)), C1, 0, TYP_NONE },
	{ NULL }
};

#define	DHOFF(f)	bitize(offsetof(xfs_dir2_data_hdr_t, f))
const field_t	dir2_data_hdr_flds[] = {
	{ "magic", FLDT_UINT32X, OI(DHOFF(magic)), C1, 0, TYP_NONE },
	{ "bestfree", FLDT_DIR2_DATA_FREE, OI(DHOFF(bestfree)),
	  CI(XFS_DIR2_DATA_FD_COUNT), FLD_ARRAY, TYP_NONE },
	{ NULL }
};

#define	DEOFF(f)	bitize(offsetof(xfs_dir2_data_entry_t, f))
#define	DUOFF(f)	bitize(offsetof(xfs_dir2_data_unused_t, f))
const field_t	dir2_data_union_flds[] = {
	{ "freetag", FLDT_UINT16X, OI(DUOFF(freetag)),
	  dir2_data_union_freetag_count, FLD_COUNT, TYP_NONE },
	{ "inumber", FLDT_INO, OI(DEOFF(inumber)),
	  dir2_data_union_inumber_count, FLD_COUNT, TYP_INODE },
	{ "length", FLDT_DIR2_DATA_OFF, OI(DUOFF(length)),
	  dir2_data_union_length_count, FLD_COUNT, TYP_NONE },
	{ "namelen", FLDT_UINT8D, OI(DEOFF(namelen)),
	  dir2_data_union_namelen_count, FLD_COUNT, TYP_NONE },
	{ "name", FLDT_CHARNS, OI(DEOFF(name)), dir2_data_union_name_count,
	  FLD_COUNT, TYP_NONE },
	{ "tag", FLDT_DIR2_DATA_OFF, dir2_data_union_tag_offset,
	  dir2_data_union_tag_count, FLD_OFFSET|FLD_COUNT, TYP_NONE },
	{ NULL }
};

#define	LEOFF(f)	bitize(offsetof(xfs_dir2_leaf_entry_t, f))
const field_t	dir2_leaf_entry_flds[] = {
	{ "hashval", FLDT_UINT32X, OI(LEOFF(hashval)), C1, 0, TYP_NONE },
	{ "address", FLDT_UINT32X, OI(LEOFF(address)), C1, 0, TYP_NONE },
	{ NULL }
};

#define	LHOFF(f)	bitize(offsetof(xfs_dir2_leaf_hdr_t, f))
const field_t	dir2_leaf_hdr_flds[] = {
	{ "info", FLDT_DA_BLKINFO, OI(LHOFF(info)), C1, 0, TYP_NONE },
	{ "count", FLDT_UINT16D, OI(LHOFF(count)), C1, 0, TYP_NONE },
	{ "stale", FLDT_UINT16D, OI(LHOFF(stale)), C1, 0, TYP_NONE },
	{ NULL }
};

#define	LTOFF(f)	bitize(offsetof(xfs_dir2_leaf_tail_t, f))
const field_t	dir2_leaf_tail_flds[] = {
	{ "bestcount", FLDT_UINT32D, OI(LTOFF(bestcount)), C1, 0, TYP_NONE },
	{ NULL }
};

#define	FHOFF(f)	bitize(offsetof(xfs_dir2_free_hdr_t, f))
const field_t	dir2_free_hdr_flds[] = {
	{ "magic", FLDT_UINT32X, OI(FHOFF(magic)), C1, 0, TYP_NONE },
	{ "firstdb", FLDT_INT32D, OI(FHOFF(firstdb)), C1, 0, TYP_NONE },
	{ "nvalid", FLDT_INT32D, OI(FHOFF(nvalid)), C1, 0, TYP_NONE },
	{ "nused", FLDT_INT32D, OI(FHOFF(nused)), C1, 0, TYP_NONE },
	{ NULL }
};

#define	DBOFF(f)	bitize(offsetof(xfs_da_blkinfo_t, f))
const field_t	da_blkinfo_flds[] = {
	{ "forw", FLDT_DIRBLOCK, OI(DBOFF(forw)), C1, 0, TYP_INODATA },
	{ "back", FLDT_DIRBLOCK, OI(DBOFF(back)), C1, 0, TYP_INODATA },
	{ "magic", FLDT_UINT16X, OI(DBOFF(magic)), C1, 0, TYP_NONE },
	{ "pad", FLDT_UINT16X, OI(DBOFF(pad)), C1, FLD_SKIPALL, TYP_NONE },
	{ NULL }
};

#define	EOFF(f)	bitize(offsetof(xfs_da_node_entry_t, f))
const field_t	da_node_entry_flds[] = {
	{ "hashval", FLDT_UINT32X, OI(EOFF(hashval)), C1, 0, TYP_NONE },
	{ "before", FLDT_DIRBLOCK, OI(EOFF(before)), C1, 0, TYP_INODATA },
	{ NULL }
};

#define	HOFF(f)	bitize(offsetof(xfs_da_node_hdr_t, f))
const field_t	da_node_hdr_flds[] = {
	{ "info", FLDT_DA_BLKINFO, OI(HOFF(info)), C1, 0, TYP_NONE },
	{ "count", FLDT_UINT16D, OI(HOFF(__count)), C1, 0, TYP_NONE },
	{ "level", FLDT_UINT16D, OI(HOFF(__level)), C1, 0, TYP_NONE },
	{ NULL }
};

/*
 * Worker functions shared between either dir2/dir3 or block/data formats
 */
static int
__dir2_block_tail_offset(
	struct xfs_dir2_data_hdr *block,
	int			startoff,
	int			idx)
{
	struct xfs_dir2_block_tail *btp;

	ASSERT(startoff == 0);
	ASSERT(idx == 0);
	btp = xfs_dir2_block_tail_p(mp, block);
	return bitize((int)((char *)btp - (char *)block));
}

static int
__dir2_data_entries_count(
	char	*ptr,
	char	*endptr)
{
	int	i;

	for (i = 0; ptr < endptr; i++) {
		struct xfs_dir2_data_entry *dep;
		struct xfs_dir2_data_unused *dup;

		dup = (xfs_dir2_data_unused_t *)ptr;
		if (be16_to_cpu(dup->freetag) == XFS_DIR2_DATA_FREE_TAG)
			ptr += be16_to_cpu(dup->length);
		else {
			dep = (xfs_dir2_data_entry_t *)ptr;
			ptr += xfs_dir2_data_entsize(dep->namelen);
		}
	}
	return i;
}

static char *
__dir2_data_entry_offset(
	char	*ptr,
	char	*endptr,
	int	idx)
{
	int	i;

	for (i = 0; i < idx; i++) {
		struct xfs_dir2_data_entry *dep;
		struct xfs_dir2_data_unused *dup;

		ASSERT(ptr < endptr);
		dup = (xfs_dir2_data_unused_t *)ptr;
		if (be16_to_cpu(dup->freetag) == XFS_DIR2_DATA_FREE_TAG)
			ptr += be16_to_cpu(dup->length);
		else {
			dep = (xfs_dir2_data_entry_t *)ptr;
			ptr += xfs_dir2_data_entsize(dep->namelen);
		}
	}
	return ptr;
}

/*
 * Block format functions
 */
static int
dir2_block_hdr_count(
	void			*obj,
	int			startoff)
{
	struct xfs_dir2_data_hdr *block;

	ASSERT(startoff == 0);
	block = obj;
	return be32_to_cpu(block->magic) == XFS_DIR2_BLOCK_MAGIC;
}

static int
dir2_block_leaf_count(
	void			*obj,
	int			startoff)
{
	struct xfs_dir2_data_hdr *block;
	struct xfs_dir2_block_tail *btp;

	ASSERT(startoff == 0);
	block = obj;
	if (be32_to_cpu(block->magic) != XFS_DIR2_BLOCK_MAGIC)
		return 0;
	btp = xfs_dir2_block_tail_p(mp, block);
	return be32_to_cpu(btp->count);
}

static int
dir2_block_leaf_offset(
	void			*obj,
	int			startoff,
	int			idx)
{
	struct xfs_dir2_data_hdr *block;
	struct xfs_dir2_block_tail *btp;
	struct xfs_dir2_leaf_entry *lep;

	ASSERT(startoff == 0);
	block = obj;
	ASSERT(be32_to_cpu(block->magic) == XFS_DIR2_BLOCK_MAGIC);
	btp = xfs_dir2_block_tail_p(mp, block);
	lep = xfs_dir2_block_leaf_p(btp) + idx;
	return bitize((int)((char *)lep - (char *)block));
}

static int
dir2_block_tail_count(
	void			*obj,
	int			startoff)
{
	struct xfs_dir2_data_hdr *block;

	ASSERT(startoff == 0);
	block = obj;
	return be32_to_cpu(block->magic) == XFS_DIR2_BLOCK_MAGIC;
}

/*ARGSUSED*/
static int
dir2_block_tail_offset(
	void			*obj,
	int			startoff,
	int			idx)
{
	struct xfs_dir2_data_hdr *block = obj;

	ASSERT(be32_to_cpu(block->magic) == XFS_DIR2_BLOCK_MAGIC);
	return __dir2_block_tail_offset(block, startoff, idx);
}

static int
dir2_block_u_count(
	void			*obj,
	int			startoff)
{
	struct xfs_dir2_data_hdr *block = obj;
	struct xfs_dir2_block_tail *btp;

	ASSERT(startoff == 0);
	if (be32_to_cpu(block->magic) != XFS_DIR2_BLOCK_MAGIC)
		return 0;

	btp = xfs_dir2_block_tail_p(mp, block);
	return __dir2_data_entries_count((char *)xfs_dir3_data_unused_p(block),
					 (char *)xfs_dir2_block_leaf_p(btp));
}

static int
dir2_block_u_offset(
	void			*obj,
	int			startoff,
	int			idx)
{
	struct xfs_dir2_data_hdr *block = obj;
	struct xfs_dir2_block_tail *btp;
	char			*ptr;

	ASSERT(startoff == 0);
	ASSERT(be32_to_cpu(block->magic) == XFS_DIR2_BLOCK_MAGIC);
	btp = xfs_dir2_block_tail_p(mp, block);
	ptr = __dir2_data_entry_offset((char *)xfs_dir3_data_unused_p(block),
				       (char *)xfs_dir2_block_leaf_p(btp), idx);
	return bitize((int)(ptr - (char *)block));
}

/*
 * Data block format functions
 */
static int
dir2_data_union_freetag_count(
	void			*obj,
	int			startoff)
{
	xfs_dir2_data_unused_t	*dup;
	char			*end;

	ASSERT(bitoffs(startoff) == 0);
	dup = (xfs_dir2_data_unused_t *)((char *)obj + byteize(startoff));
	end = (char *)&dup->freetag + sizeof(dup->freetag);
	return end <= (char *)obj + mp->m_dirblksize &&
	       be16_to_cpu(dup->freetag) == XFS_DIR2_DATA_FREE_TAG;
}

static int
dir2_data_union_inumber_count(
	void			*obj,
	int			startoff)
{
	xfs_dir2_data_entry_t	*dep;
	xfs_dir2_data_unused_t	*dup;
	char			*end;

	ASSERT(bitoffs(startoff) == 0);
	dup = (xfs_dir2_data_unused_t *)((char *)obj + byteize(startoff));
	dep = (xfs_dir2_data_entry_t *)dup;
	end = (char *)&dep->inumber + sizeof(dep->inumber);
	return end <= (char *)obj + mp->m_dirblksize &&
	       be16_to_cpu(dup->freetag) != XFS_DIR2_DATA_FREE_TAG;
}

static int
dir2_data_union_length_count(
	void			*obj,
	int			startoff)
{
	xfs_dir2_data_unused_t	*dup;
	char			*end;

	ASSERT(bitoffs(startoff) == 0);
	dup = (xfs_dir2_data_unused_t *)((char *)obj + byteize(startoff));
	end = (char *)&dup->length + sizeof(dup->length);
	return end <= (char *)obj + mp->m_dirblksize &&
	       be16_to_cpu(dup->freetag) == XFS_DIR2_DATA_FREE_TAG;
}

static int
dir2_data_union_name_count(
	void			*obj,
	int			startoff)
{
	xfs_dir2_data_entry_t	*dep;
	xfs_dir2_data_unused_t	*dup;
	char			*end;

	ASSERT(bitoffs(startoff) == 0);
	dup = (xfs_dir2_data_unused_t *)((char *)obj + byteize(startoff));
	dep = (xfs_dir2_data_entry_t *)dup;
	end = (char *)&dep->namelen + sizeof(dep->namelen);
	if (end >= (char *)obj + mp->m_dirblksize ||
	    be16_to_cpu(dup->freetag) == XFS_DIR2_DATA_FREE_TAG)
		return 0;
	end = (char *)&dep->name[0] + dep->namelen;
	return end <= (char *)obj + mp->m_dirblksize ? dep->namelen : 0;
}

static int
dir2_data_union_namelen_count(
	void			*obj,
	int			startoff)
{
	xfs_dir2_data_entry_t	*dep;
	xfs_dir2_data_unused_t	*dup;
	char			*end;

	ASSERT(bitoffs(startoff) == 0);
	dup = (xfs_dir2_data_unused_t *)((char *)obj + byteize(startoff));
	dep = (xfs_dir2_data_entry_t *)dup;
	end = (char *)&dep->namelen + sizeof(dep->namelen);
	return end <= (char *)obj + mp->m_dirblksize &&
	       be16_to_cpu(dup->freetag) != XFS_DIR2_DATA_FREE_TAG;
}

static int
dir2_data_union_tag_count(
	void			*obj,
	int			startoff)
{
	xfs_dir2_data_entry_t	*dep;
	xfs_dir2_data_unused_t	*dup;
	char			*end;
	__be16			*tagp;

	ASSERT(bitoffs(startoff) == 0);
	dup = (xfs_dir2_data_unused_t *)((char *)obj + byteize(startoff));
	dep = (xfs_dir2_data_entry_t *)dup;
	end = (char *)&dup->freetag + sizeof(dup->freetag);
	if (end > (char *)obj + mp->m_dirblksize)
		return 0;
	if (be16_to_cpu(dup->freetag) == XFS_DIR2_DATA_FREE_TAG) {
		end = (char *)&dup->length + sizeof(dup->length);
		if (end > (char *)obj + mp->m_dirblksize)
			return 0;
		tagp = xfs_dir2_data_unused_tag_p(dup);
	} else {
		end = (char *)&dep->namelen + sizeof(dep->namelen);
		if (end > (char *)obj + mp->m_dirblksize)
			return 0;
		tagp = xfs_dir2_data_entry_tag_p(dep);
	}
	end = (char *)tagp + sizeof(*tagp);
	return end <= (char *)obj + mp->m_dirblksize;
}

/*ARGSUSED*/
static int
dir2_data_union_tag_offset(
	void			*obj,
	int			startoff,
	int			idx)
{
	xfs_dir2_data_entry_t	*dep;
	xfs_dir2_data_unused_t	*dup;

	ASSERT(bitoffs(startoff) == 0);
	ASSERT(idx == 0);
	dup = (xfs_dir2_data_unused_t *)((char *)obj + byteize(startoff));
	if (be16_to_cpu(dup->freetag) == XFS_DIR2_DATA_FREE_TAG)
		return bitize((int)((char *)xfs_dir2_data_unused_tag_p(dup) -
				    (char *)dup));
	dep = (xfs_dir2_data_entry_t *)dup;
	return bitize((int)((char *)xfs_dir2_data_entry_tag_p(dep) -
			    (char *)dep));
}

/*ARGSUSED*/
static int
dir2_data_hdr_count(
	void			*obj,
	int			startoff)
{
	struct xfs_dir2_data_hdr *data;

	ASSERT(startoff == 0);
	data = obj;
	return be32_to_cpu(data->magic) == XFS_DIR2_DATA_MAGIC;
}

/*ARGSUSED*/
static int
dir2_data_u_count(
	void			*obj,
	int			startoff)
{
	struct xfs_dir2_data_hdr *data = obj;

	ASSERT(startoff == 0);
	if (be32_to_cpu(data->magic) != XFS_DIR2_DATA_MAGIC)
		return 0;

	return __dir2_data_entries_count((char *)xfs_dir3_data_unused_p(data),
					 (char *)data + mp->m_dirblksize);
}

static int
dir2_data_u_offset(
	void			*obj,
	int			startoff,
	int			idx)
{
	struct xfs_dir2_data_hdr *data = obj;
	char			*ptr;

	ASSERT(startoff == 0);
	ASSERT(be32_to_cpu(data->magic) == XFS_DIR2_DATA_MAGIC);
	ptr = __dir2_data_entry_offset((char *)xfs_dir3_data_unused_p(data),
				       (char *)data + mp->m_dirblksize, idx);
	return bitize((int)(ptr - (char *)data));
}

int
dir2_data_union_size(
	void			*obj,
	int			startoff,
	int			idx)
{
	xfs_dir2_data_entry_t	*dep;
	xfs_dir2_data_unused_t	*dup;

	ASSERT(bitoffs(startoff) == 0);
	ASSERT(idx == 0);
	dup = (xfs_dir2_data_unused_t *)((char *)obj + byteize(startoff));
	if (be16_to_cpu(dup->freetag) == XFS_DIR2_DATA_FREE_TAG)
		return bitize(be16_to_cpu(dup->length));
	else {
		dep = (xfs_dir2_data_entry_t *)dup;
		return bitize(xfs_dir2_data_entsize(dep->namelen));
	}
}

/*ARGSUSED*/
static int
dir2_free_bests_count(
	void			*obj,
	int			startoff)
{
	struct xfs_dir2_free	*free;

	ASSERT(startoff == 0);
	free = obj;
	if (be32_to_cpu(free->hdr.magic) != XFS_DIR2_FREE_MAGIC)
		return 0;
	return be32_to_cpu(free->hdr.nvalid);
}

/*ARGSUSED*/
static int
dir2_free_hdr_count(
	void			*obj,
	int			startoff)
{
	struct xfs_dir2_free	*free;

	ASSERT(startoff == 0);
	free = obj;
	return be32_to_cpu(free->hdr.magic) == XFS_DIR2_FREE_MAGIC;
}

/*ARGSUSED*/
static int
dir2_leaf_bests_count(
	void			*obj,
	int			startoff)
{
	struct xfs_dir2_leaf	*leaf;
	struct xfs_dir2_leaf_tail *ltp;

	ASSERT(startoff == 0);
	leaf = obj;
	if (be16_to_cpu(leaf->hdr.info.magic) != XFS_DIR2_LEAF1_MAGIC)
		return 0;
	ltp = xfs_dir2_leaf_tail_p(mp, leaf);
	return be32_to_cpu(ltp->bestcount);
}

/*ARGSUSED*/
static int
dir2_leaf_bests_offset(
	void			*obj,
	int			startoff,
	int			idx)
{
	struct xfs_dir2_leaf	*leaf;
	struct xfs_dir2_leaf_tail *ltp;
	__be16			*lbp;

	ASSERT(startoff == 0);
	leaf = obj;
	ASSERT(be16_to_cpu(leaf->hdr.info.magic) == XFS_DIR2_LEAF1_MAGIC);
	ltp = xfs_dir2_leaf_tail_p(mp, leaf);
	lbp = xfs_dir2_leaf_bests_p(ltp) + idx;
	return bitize((int)((char *)lbp - (char *)leaf));
}

/*ARGSUSED*/
static int
dir2_leaf_ents_count(
	void			*obj,
	int			startoff)
{
	struct xfs_dir2_leaf	*leaf;

	ASSERT(startoff == 0);
	leaf = obj;
	if (be16_to_cpu(leaf->hdr.info.magic) != XFS_DIR2_LEAF1_MAGIC &&
	    be16_to_cpu(leaf->hdr.info.magic) != XFS_DIR2_LEAFN_MAGIC)
		return 0;
	return be16_to_cpu(leaf->hdr.count);
}

/*ARGSUSED*/
static int
dir2_leaf_hdr_count(
	void			*obj,
	int			startoff)
{
	struct xfs_dir2_leaf	*leaf;

	ASSERT(startoff == 0);
	leaf = obj;
	return be16_to_cpu(leaf->hdr.info.magic) == XFS_DIR2_LEAF1_MAGIC ||
	       be16_to_cpu(leaf->hdr.info.magic) == XFS_DIR2_LEAFN_MAGIC;
}

/*ARGSUSED*/
static int
dir2_leaf_tail_count(
	void			*obj,
	int			startoff)
{
	struct xfs_dir2_leaf	*leaf;

	ASSERT(startoff == 0);
	leaf = obj;
	return be16_to_cpu(leaf->hdr.info.magic) == XFS_DIR2_LEAF1_MAGIC;
}

/*ARGSUSED*/
static int
dir2_leaf_tail_offset(
	void			*obj,
	int			startoff,
	int			idx)
{
	struct xfs_dir2_leaf	*leaf;
	struct xfs_dir2_leaf_tail *ltp;

	ASSERT(startoff == 0);
	ASSERT(idx == 0);
	leaf = obj;
	ASSERT(be16_to_cpu(leaf->hdr.info.magic) == XFS_DIR2_LEAF1_MAGIC);
	ltp = xfs_dir2_leaf_tail_p(mp, leaf);
	return bitize((int)((char *)ltp - (char *)leaf));
}

/*ARGSUSED*/
static int
dir2_node_btree_count(
	void			*obj,
	int			startoff)
{
	xfs_da_intnode_t	*node;

	ASSERT(startoff == 0);
	node = obj;
	if (be16_to_cpu(node->hdr.info.magic) != XFS_DA_NODE_MAGIC)
		return 0;
	return be16_to_cpu(node->hdr.__count);
}

/*ARGSUSED*/
static int
dir2_node_hdr_count(
	void			*obj,
	int			startoff)
{
	struct xfs_da_intnode	*node;

	ASSERT(startoff == 0);
	node = obj;
	return be16_to_cpu(node->hdr.info.magic) == XFS_DA_NODE_MAGIC;
}

/*ARGSUSED*/
int
dir2_size(
	void	*obj,
	int	startoff,
	int	idx)
{
	return bitize(mp->m_dirblksize);
}
