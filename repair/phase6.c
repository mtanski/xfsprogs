/*
 * Copyright (c) 2000-2005 Silicon Graphics, Inc.
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
#include "dir.h"
#include "dir2.h"
#include "protos.h"
#include "err_protos.h"
#include "dinode.h"
#include "prefetch.h"
#include "progress.h"
#include "threads.h"
#include "versions.h"

static struct cred		zerocr;
static struct fsxattr 		zerofsx;
static xfs_ino_t		orphanage_ino;

/*
 * Data structures and routines to keep track of directory entries
 * and whether their leaf entry has been seen. Also used for name
 * duplicate checking and rebuilding step if required.
 */
typedef struct dir_hash_ent {
	struct dir_hash_ent	*nextbyaddr;	/* next in addr bucket */
	struct dir_hash_ent	*nextbyhash;	/* next in name bucket */
	struct dir_hash_ent	*nextbyorder;	/* next in order added */
	xfs_dahash_t		hashval;	/* hash value of name */
	__uint32_t		address;	/* offset of data entry */
	xfs_ino_t 		inum;		/* inode num of entry */
	short			junkit;		/* name starts with / */
	short			seen;		/* have seen leaf entry */
	int	  	    	namelen;	/* length of name */
	uchar_t    	    	*name;		/* pointer to name (no NULL) */
} dir_hash_ent_t;

typedef struct dir_hash_tab {
	int			size;		/* size of hash tables */
	int			names_duped;	/* 1 = ent names malloced */
	dir_hash_ent_t		*first;		/* ptr to first added entry */
	dir_hash_ent_t		*last;		/* ptr to last added entry */
	dir_hash_ent_t		**byhash;	/* ptr to name hash buckets */
	dir_hash_ent_t		**byaddr;	/* ptr to addr hash buckets */
} dir_hash_tab_t;

#define	DIR_HASH_TAB_SIZE(n)	\
	(sizeof(dir_hash_tab_t) + (sizeof(dir_hash_ent_t *) * (n) * 2))
#define	DIR_HASH_FUNC(t,a)	((a) % (t)->size)

/*
 * Track the contents of the freespace table in a directory.
 */
typedef struct freetab {
	int			naents;	/* expected number of data blocks */
	int			nents;	/* number of data blocks processed */
	struct freetab_ent {
		xfs_dir2_data_off_t	v;
		short			s;
	} ents[1];
} freetab_t;
#define	FREETAB_SIZE(n)	\
	(offsetof(freetab_t, ents) + (sizeof(struct freetab_ent) * (n)))

#define	DIR_HASH_CK_OK		0
#define	DIR_HASH_CK_DUPLEAF	1
#define	DIR_HASH_CK_BADHASH	2
#define	DIR_HASH_CK_NODATA	3
#define	DIR_HASH_CK_NOLEAF	4
#define	DIR_HASH_CK_BADSTALE	5
#define	DIR_HASH_CK_TOTAL	6

/*
 * Returns 0 if the name already exists (ie. a duplicate)
 */
static int
dir_hash_add(
	dir_hash_tab_t		*hashtab,
	__uint32_t		addr,
	xfs_ino_t		inum,
	int			namelen,
	uchar_t			*name)
{
	xfs_dahash_t		hash = 0;
	int			byaddr;
	int			byhash = 0;
	dir_hash_ent_t		*p;
	int			dup;
	short			junk;

	ASSERT(!hashtab->names_duped);

	junk = name[0] == '/';
	byaddr = DIR_HASH_FUNC(hashtab, addr);
	dup = 0;

	if (!junk) {
		hash = libxfs_da_hashname(name, namelen);
		byhash = DIR_HASH_FUNC(hashtab, hash);

		/*
		 * search hash bucket for existing name.
		 */
		for (p = hashtab->byhash[byhash]; p; p = p->nextbyhash) {
			if (p->hashval == hash && p->namelen == namelen) {
				if (memcmp(p->name, name, namelen) == 0) {
					dup = 1;
					junk = 1;
					break;
				}
			}
		}
	}

	if ((p = malloc(sizeof(*p))) == NULL)
		do_error(_("malloc failed in dir_hash_add (%u bytes)\n"),
			sizeof(*p));

	p->nextbyaddr = hashtab->byaddr[byaddr];
	hashtab->byaddr[byaddr] = p;
	if (hashtab->last)
		hashtab->last->nextbyorder = p;
	else
		hashtab->first = p;
	p->nextbyorder = NULL;
	hashtab->last = p;

	if (!(p->junkit = junk)) {
		p->hashval = hash;
		p->nextbyhash = hashtab->byhash[byhash];
		hashtab->byhash[byhash] = p;
	}
	p->address = addr;
	p->inum = inum;
	p->seen = 0;
	p->namelen = namelen;
	p->name = name;

	return !dup;
}

/*
 * checks to see if any data entries are not in the leaf blocks
 */
static int
dir_hash_unseen(
	dir_hash_tab_t	*hashtab)
{
	int		i;
	dir_hash_ent_t	*p;

	for (i = 0; i < hashtab->size; i++) {
		for (p = hashtab->byaddr[i]; p; p = p->nextbyaddr) {
			if (p->seen == 0)
				return 1;
		}
	}
	return 0;
}

static int
dir_hash_check(
	dir_hash_tab_t	*hashtab,
	xfs_inode_t	*ip,
	int		seeval)
{
	static char	*seevalstr[DIR_HASH_CK_TOTAL];
	static int	done;

	if (!done) {
		seevalstr[DIR_HASH_CK_OK] = _("ok");
		seevalstr[DIR_HASH_CK_DUPLEAF] = _("duplicate leaf");
		seevalstr[DIR_HASH_CK_BADHASH] = _("hash value mismatch");
		seevalstr[DIR_HASH_CK_NODATA] = _("no data entry");
		seevalstr[DIR_HASH_CK_NOLEAF] = _("no leaf entry");
		seevalstr[DIR_HASH_CK_BADSTALE] = _("bad stale count");
		done = 1;
	}

	if (seeval == DIR_HASH_CK_OK && dir_hash_unseen(hashtab))
		seeval = DIR_HASH_CK_NOLEAF;
	if (seeval == DIR_HASH_CK_OK)
		return 0;
	do_warn(_("bad hash table for directory inode %llu (%s): "),
		ip->i_ino, seevalstr[seeval]);
	if (!no_modify)
		do_warn(_("rebuilding\n"));
	else
		do_warn(_("would rebuild\n"));
	return 1;
}

static void
dir_hash_done(
	dir_hash_tab_t	*hashtab)
{
	int		i;
	dir_hash_ent_t	*n;
	dir_hash_ent_t	*p;

	for (i = 0; i < hashtab->size; i++) {
		for (p = hashtab->byaddr[i]; p; p = n) {
			n = p->nextbyaddr;
			if (hashtab->names_duped)
				free(p->name);
			free(p);
		}
	}
	free(hashtab);
}

static dir_hash_tab_t *
dir_hash_init(
	xfs_fsize_t	size)
{
	dir_hash_tab_t	*hashtab;
	int		hsize;

	hsize = size / (16 * 4);
	if (hsize > 65536)
		hsize = 63336;
	else if (hsize < 16)
		hsize = 16;
	if ((hashtab = calloc(DIR_HASH_TAB_SIZE(hsize), 1)) == NULL)
		do_error(_("calloc failed in dir_hash_init\n"));
	hashtab->size = hsize;
	hashtab->byhash = (dir_hash_ent_t**)((char *)hashtab +
		sizeof(dir_hash_tab_t));
	hashtab->byaddr = (dir_hash_ent_t**)((char *)hashtab +
		sizeof(dir_hash_tab_t) + sizeof(dir_hash_ent_t*) * hsize);
	return hashtab;
}

static int
dir_hash_see(
	dir_hash_tab_t		*hashtab,
	xfs_dahash_t		hash,
	xfs_dir2_dataptr_t	addr)
{
	int			i;
	dir_hash_ent_t		*p;

	i = DIR_HASH_FUNC(hashtab, addr);
	for (p = hashtab->byaddr[i]; p; p = p->nextbyaddr) {
		if (p->address != addr)
			continue;
		if (p->seen)
			return DIR_HASH_CK_DUPLEAF;
		if (p->junkit == 0 && p->hashval != hash)
			return DIR_HASH_CK_BADHASH;
		p->seen = 1;
		return DIR_HASH_CK_OK;
	}
	return DIR_HASH_CK_NODATA;
}

/*
 * checks to make sure leafs match a data entry, and that the stale
 * count is valid.
 */
static int
dir_hash_see_all(
	dir_hash_tab_t		*hashtab,
	xfs_dir2_leaf_entry_t	*ents,
	int			count,
	int			stale)
{
	int			i;
	int			j;
	int			rval;

	for (i = j = 0; i < count; i++) {
		if (INT_GET(ents[i].address, ARCH_CONVERT) == XFS_DIR2_NULL_DATAPTR) {
			j++;
			continue;
		}
		rval = dir_hash_see(hashtab, INT_GET(ents[i].hashval, ARCH_CONVERT), INT_GET(ents[i].address, ARCH_CONVERT));
		if (rval != DIR_HASH_CK_OK)
			return rval;
	}
	return j == stale ? DIR_HASH_CK_OK : DIR_HASH_CK_BADSTALE;
}

/*
 * Convert name pointers into locally allocated memory.
 * This must only be done after all the entries have been added.
 */
static void
dir_hash_dup_names(dir_hash_tab_t *hashtab)
{
	uchar_t			*name;
	dir_hash_ent_t		*p;

	if (hashtab->names_duped)
		return;

	for (p = hashtab->first; p; p = p->nextbyorder) {
		name = malloc(p->namelen);
		memcpy(name, p->name, p->namelen);
		p->name = name;
	}
	hashtab->names_duped = 1;
}

/*
 * Version 1 or 2 directory routine wrappers
*/
static void
dir_init(xfs_mount_t *mp, xfs_trans_t *tp, xfs_inode_t *dp, xfs_inode_t *pdp)
{
	if (XFS_SB_VERSION_HASDIRV2(&mp->m_sb))
		libxfs_dir2_init(tp, dp, pdp);
	else
		libxfs_dir_init(tp, dp, pdp);
}

static int
dir_createname(xfs_mount_t *mp, xfs_trans_t *tp, xfs_inode_t *pip,
		char *name, int namelen, xfs_ino_t inum, xfs_fsblock_t *first,
		xfs_bmap_free_t *flist, xfs_extlen_t total)
{
	if (XFS_SB_VERSION_HASDIRV2(&mp->m_sb))
		return libxfs_dir2_createname(tp, pip,
				(uchar_t *)name, namelen,
				inum, first, flist, total);
	else
		return libxfs_dir_createname(tp, pip,
				(uchar_t *)name, namelen,
				inum, first, flist, total);
}

static int
dir_lookup(xfs_mount_t *mp, xfs_trans_t *tp, xfs_inode_t *dp, char *name,
		int namelen, xfs_ino_t *inum)
{
	if (XFS_SB_VERSION_HASDIRV2(&mp->m_sb))
		return libxfs_dir2_lookup(tp, dp,
					(uchar_t *)name, namelen, inum);
	else
		return libxfs_dir_lookup(tp, dp,
					(uchar_t *)name, namelen, inum);
}

static int
dir_replace(xfs_mount_t *mp, xfs_trans_t *tp, xfs_inode_t *dp, char *name,
		int namelen, xfs_ino_t inum, xfs_fsblock_t *firstblock,
		xfs_bmap_free_t *flist, xfs_extlen_t total)
{
	if (XFS_SB_VERSION_HASDIRV2(&mp->m_sb))
		return libxfs_dir2_replace(tp, dp,
				(uchar_t *)name, namelen, inum,
				firstblock, flist, total);
	else
		return libxfs_dir_replace(tp, dp,
				(uchar_t *)name, namelen, inum,
				firstblock, flist, total);
}

static int
dir_bogus_removename(xfs_mount_t *mp, xfs_trans_t *tp, xfs_inode_t *dp,
		char *name, xfs_fsblock_t *firstblock, xfs_bmap_free_t *flist,
		xfs_extlen_t total, xfs_dahash_t hashval, int namelen)
{
	if (XFS_SB_VERSION_HASDIRV2(&mp->m_sb))
		return libxfs_dir2_bogus_removename(tp, dp,
				(uchar_t *)name, firstblock,
				flist, total, hashval, namelen);
	else
		return libxfs_dir_bogus_removename(tp, dp,
				(uchar_t *)name, firstblock,
				flist, total, hashval, namelen);
}


static void
res_failed(
	int	err)
{
	if (err == ENOSPC) {
		do_error(_("ran out of disk space!\n"));
	} else
		do_error(_("xfs_trans_reserve returned %d\n"), err);
}

void
mk_rbmino(xfs_mount_t *mp)
{
	xfs_trans_t	*tp;
	xfs_inode_t	*ip;
	xfs_bmbt_irec_t	*ep;
	xfs_fsblock_t	first;
	int		i;
	int		nmap;
	int		committed;
	int		error;
	xfs_bmap_free_t	flist;
	xfs_dfiloff_t	bno;
	xfs_bmbt_irec_t	map[XFS_BMAP_MAX_NMAP];

	/*
	 * first set up inode
	 */
	tp = libxfs_trans_alloc(mp, 0);

	if ((i = libxfs_trans_reserve(tp, 10, 0, 0, 0, 0)))
		res_failed(i);

	error = libxfs_trans_iget(mp, tp, mp->m_sb.sb_rbmino, 0, 0, &ip);
	if (error) {
		do_error(
		_("couldn't iget realtime bitmap inode -- error - %d\n"),
			error);
	}

	memset(&ip->i_d, 0, sizeof(xfs_dinode_core_t));

	ip->i_d.di_magic = XFS_DINODE_MAGIC;
	ip->i_d.di_mode = S_IFREG;
	ip->i_d.di_version = XFS_DINODE_VERSION_1;
	ip->i_d.di_format = XFS_DINODE_FMT_EXTENTS;
	ip->i_d.di_aformat = XFS_DINODE_FMT_EXTENTS;

	ip->i_d.di_nlink = 1;		/* account for sb ptr */

	/*
	 * now the ifork
	 */
	ip->i_df.if_flags = XFS_IFEXTENTS;
	ip->i_df.if_bytes = ip->i_df.if_real_bytes = 0;
	ip->i_df.if_u1.if_extents = NULL;

	ip->i_d.di_size = mp->m_sb.sb_rbmblocks * mp->m_sb.sb_blocksize;

	/*
	 * commit changes
	 */
	libxfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);
	libxfs_trans_ihold(tp, ip);
	libxfs_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES|XFS_TRANS_SYNC, NULL);

	/*
	 * then allocate blocks for file and fill with zeroes (stolen
	 * from mkfs)
	 */
	tp = libxfs_trans_alloc(mp, 0);
	if ((error = libxfs_trans_reserve(tp, mp->m_sb.sb_rbmblocks +
			(XFS_BM_MAXLEVELS(mp, XFS_DATA_FORK) - 1), 0, 0, 0, 0)))
		res_failed(error);

	libxfs_trans_ijoin(tp, ip, 0);
	bno = 0;
	XFS_BMAP_INIT(&flist, &first);
	while (bno < mp->m_sb.sb_rbmblocks) {
		nmap = XFS_BMAP_MAX_NMAP;
		error = libxfs_bmapi(tp, ip, bno,
			  (xfs_extlen_t)(mp->m_sb.sb_rbmblocks - bno),
			  XFS_BMAPI_WRITE, &first, mp->m_sb.sb_rbmblocks,
			  map, &nmap, &flist);
		if (error) {
			do_error(
			_("couldn't allocate realtime bitmap, error = %d\n"),
				error);
		}
		for (i = 0, ep = map; i < nmap; i++, ep++) {
			libxfs_device_zero(mp->m_dev,
				XFS_FSB_TO_DADDR(mp, ep->br_startblock),
				XFS_FSB_TO_BB(mp, ep->br_blockcount));
			bno += ep->br_blockcount;
		}
	}
	error = libxfs_bmap_finish(&tp, &flist, first, &committed);
	if (error) {
		do_error(
		_("allocation of the realtime bitmap failed, error = %d\n"),
			error);
	}
	libxfs_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES|XFS_TRANS_SYNC, 0);
}

int
fill_rbmino(xfs_mount_t *mp)
{
	xfs_buf_t	*bp;
	xfs_trans_t	*tp;
	xfs_inode_t	*ip;
	xfs_rtword_t	*bmp;
	xfs_fsblock_t	first;
	int		nmap;
	int		error;
	xfs_dfiloff_t	bno;
	xfs_bmbt_irec_t	map;

	bmp = btmcompute;
	bno = 0;

	tp = libxfs_trans_alloc(mp, 0);

	if ((error = libxfs_trans_reserve(tp, 10, 0, 0, 0, 0)))
		res_failed(error);

	error = libxfs_trans_iget(mp, tp, mp->m_sb.sb_rbmino, 0, 0, &ip);
	if (error) {
		do_error(
		_("couldn't iget realtime bitmap inode -- error - %d\n"),
			error);
	}

	while (bno < mp->m_sb.sb_rbmblocks)  {
		/*
		 * fill the file one block at a time
		 */
		nmap = 1;
		error = libxfs_bmapi(tp, ip, bno, 1, XFS_BMAPI_WRITE,
					&first, 1, &map, &nmap, NULL);
		if (error || nmap != 1) {
			do_error(
		_("couldn't map realtime bitmap block %llu, error = %d\n"),
				bno, error);
		}

		ASSERT(map.br_startblock != HOLESTARTBLOCK);

		error = libxfs_trans_read_buf(
				mp, tp, mp->m_dev,
				XFS_FSB_TO_DADDR(mp, map.br_startblock),
				XFS_FSB_TO_BB(mp, 1), 1, &bp);

		if (error) {
			do_warn(
_("can't access block %llu (fsbno %llu) of realtime bitmap inode %llu\n"),
				bno, map.br_startblock, mp->m_sb.sb_rbmino);
			return(1);
		}

		memmove(XFS_BUF_PTR(bp), bmp, mp->m_sb.sb_blocksize);

		libxfs_trans_log_buf(tp, bp, 0, mp->m_sb.sb_blocksize - 1);

		bmp = (xfs_rtword_t *)((__psint_t) bmp + mp->m_sb.sb_blocksize);
		bno++;
	}

	libxfs_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES|XFS_TRANS_SYNC, 0);
	return(0);
}

int
fill_rsumino(xfs_mount_t *mp)
{
	xfs_buf_t	*bp;
	xfs_trans_t	*tp;
	xfs_inode_t	*ip;
	xfs_suminfo_t	*smp;
	xfs_fsblock_t	first;
	int		nmap;
	int		error;
	xfs_dfiloff_t	bno;
	xfs_dfiloff_t	end_bno;
	xfs_bmbt_irec_t	map;

	smp = sumcompute;
	bno = 0;
	end_bno = mp->m_rsumsize >> mp->m_sb.sb_blocklog;

	tp = libxfs_trans_alloc(mp, 0);

	if ((error = libxfs_trans_reserve(tp, 10, 0, 0, 0, 0)))
		res_failed(error);

	error = libxfs_trans_iget(mp, tp, mp->m_sb.sb_rsumino, 0, 0, &ip);
	if (error) {
		do_error(
		_("couldn't iget realtime summary inode -- error - %d\n"),
			error);
	}

	while (bno < end_bno)  {
		/*
		 * fill the file one block at a time
		 */
		nmap = 1;
		error = libxfs_bmapi(tp, ip, bno, 1, XFS_BMAPI_WRITE,
					&first, 1, &map, &nmap, NULL);
		if (error || nmap != 1) {
			do_error(
	_("couldn't map realtime summary inode block %llu, error = %d\n"),
				bno, error);
		}

		ASSERT(map.br_startblock != HOLESTARTBLOCK);

		error = libxfs_trans_read_buf(
				mp, tp, mp->m_dev,
				XFS_FSB_TO_DADDR(mp, map.br_startblock),
				XFS_FSB_TO_BB(mp, 1), 1, &bp);

		if (error) {
			do_warn(
_("can't access block %llu (fsbno %llu) of realtime summary inode %llu\n"),
				bno, map.br_startblock, mp->m_sb.sb_rsumino);
			return(1);
		}

		memmove(XFS_BUF_PTR(bp), smp, mp->m_sb.sb_blocksize);

		libxfs_trans_log_buf(tp, bp, 0, mp->m_sb.sb_blocksize - 1);

		smp = (xfs_suminfo_t *)((__psint_t)smp + mp->m_sb.sb_blocksize);
		bno++;
	}

	libxfs_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES|XFS_TRANS_SYNC, 0);
	return(0);
}

void
mk_rsumino(xfs_mount_t *mp)
{
	xfs_trans_t	*tp;
	xfs_inode_t	*ip;
	xfs_bmbt_irec_t	*ep;
	xfs_fsblock_t	first;
	int		i;
	int		nmap;
	int		committed;
	int		error;
	int		nsumblocks;
	xfs_bmap_free_t	flist;
	xfs_dfiloff_t	bno;
	xfs_bmbt_irec_t	map[XFS_BMAP_MAX_NMAP];

	/*
	 * first set up inode
	 */
	tp = libxfs_trans_alloc(mp, 0);

	if ((i = libxfs_trans_reserve(tp, 10, XFS_ICHANGE_LOG_RES(mp), 0,
				XFS_TRANS_PERM_LOG_RES, XFS_MKDIR_LOG_COUNT)))
		res_failed(i);

	error = libxfs_trans_iget(mp, tp, mp->m_sb.sb_rsumino, 0, 0, &ip);
	if (error) {
		do_error(
		_("couldn't iget realtime summary inode -- error - %d\n"),
			error);
	}

	memset(&ip->i_d, 0, sizeof(xfs_dinode_core_t));

	ip->i_d.di_magic = XFS_DINODE_MAGIC;
	ip->i_d.di_mode = S_IFREG;
	ip->i_d.di_version = XFS_DINODE_VERSION_1;
	ip->i_d.di_format = XFS_DINODE_FMT_EXTENTS;
	ip->i_d.di_aformat = XFS_DINODE_FMT_EXTENTS;

	ip->i_d.di_nlink = 1;		/* account for sb ptr */

	/*
	 * now the ifork
	 */
	ip->i_df.if_flags = XFS_IFEXTENTS;
	ip->i_df.if_bytes = ip->i_df.if_real_bytes = 0;
	ip->i_df.if_u1.if_extents = NULL;

	ip->i_d.di_size = mp->m_rsumsize;

	/*
	 * commit changes
	 */
	libxfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);
	libxfs_trans_ihold(tp, ip);
	libxfs_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES|XFS_TRANS_SYNC, 0);

	/*
	 * then allocate blocks for file and fill with zeroes (stolen
	 * from mkfs)
	 */
	tp = libxfs_trans_alloc(mp, 0);
	XFS_BMAP_INIT(&flist, &first);

	nsumblocks = mp->m_rsumsize >> mp->m_sb.sb_blocklog;
	if ((error = libxfs_trans_reserve(tp,
				  mp->m_sb.sb_rbmblocks +
				      (XFS_BM_MAXLEVELS(mp, XFS_DATA_FORK) - 1),
				  BBTOB(128), 0, XFS_TRANS_PERM_LOG_RES,
				  XFS_DEFAULT_PERM_LOG_COUNT)))
		res_failed(error);

	libxfs_trans_ijoin(tp, ip, 0);
	bno = 0;
	XFS_BMAP_INIT(&flist, &first);
	while (bno < nsumblocks) {
		nmap = XFS_BMAP_MAX_NMAP;
		error = libxfs_bmapi(tp, ip, bno,
			  (xfs_extlen_t)(nsumblocks - bno),
			  XFS_BMAPI_WRITE, &first, nsumblocks,
			  map, &nmap, &flist);
		if (error) {
			do_error(
		_("couldn't allocate realtime summary inode, error = %d\n"),
				error);
		}
		for (i = 0, ep = map; i < nmap; i++, ep++) {
			libxfs_device_zero(mp->m_dev,
				      XFS_FSB_TO_DADDR(mp, ep->br_startblock),
				      XFS_FSB_TO_BB(mp, ep->br_blockcount));
			bno += ep->br_blockcount;
		}
	}
	error = libxfs_bmap_finish(&tp, &flist, first, &committed);
	if (error) {
		do_error(
	_("allocation of the realtime summary ino failed, error = %d\n"),
			error);
	}
	libxfs_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES|XFS_TRANS_SYNC, 0);
}

/*
 * makes a new root directory.
 */
void
mk_root_dir(xfs_mount_t *mp)
{
	xfs_trans_t	*tp;
	xfs_inode_t	*ip;
	int		i;
	int		error;
	const mode_t	mode = 0755;
	ino_tree_node_t	*irec;

	tp = libxfs_trans_alloc(mp, 0);
	ip = NULL;

	if ((i = libxfs_trans_reserve(tp, 10, XFS_ICHANGE_LOG_RES(mp), 0,
				XFS_TRANS_PERM_LOG_RES, XFS_MKDIR_LOG_COUNT)))
		res_failed(i);

	error = libxfs_trans_iget(mp, tp, mp->m_sb.sb_rootino, 0, 0, &ip);
	if (error) {
		do_error(_("could not iget root inode -- error - %d\n"), error);
	}

	/*
	 * take care of the core -- initialization from xfs_ialloc()
	 */
	memset(&ip->i_d, 0, sizeof(xfs_dinode_core_t));

	ip->i_d.di_magic = XFS_DINODE_MAGIC;
	ip->i_d.di_mode = (__uint16_t) mode|S_IFDIR;
	ip->i_d.di_version = XFS_DINODE_VERSION_1;
	ip->i_d.di_format = XFS_DINODE_FMT_EXTENTS;
	ip->i_d.di_aformat = XFS_DINODE_FMT_EXTENTS;

	ip->i_d.di_nlink = 1;		/* account for . */

	libxfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);

	/*
	 * now the ifork
	 */
	ip->i_df.if_flags = XFS_IFEXTENTS;
	ip->i_df.if_bytes = ip->i_df.if_real_bytes = 0;
	ip->i_df.if_u1.if_extents = NULL;

	mp->m_rootip = ip;

	/*
	 * initialize the directory
	 */
	dir_init(mp, tp, ip, ip);

	libxfs_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES|XFS_TRANS_SYNC, 0);

	irec = find_inode_rec(XFS_INO_TO_AGNO(mp, mp->m_sb.sb_rootino),
				XFS_INO_TO_AGINO(mp, mp->m_sb.sb_rootino));
	set_inode_isadir(irec, XFS_INO_TO_AGINO(mp, mp->m_sb.sb_rootino) -
				irec->ino_startnum);
}

/*
 * orphanage name == lost+found
 */
xfs_ino_t
mk_orphanage(xfs_mount_t *mp)
{
	xfs_ino_t	ino;
	xfs_trans_t	*tp;
	xfs_inode_t	*ip;
	xfs_inode_t	*pip;
	xfs_fsblock_t	first;
	int		i;
	int		committed;
	int		error;
	xfs_bmap_free_t	flist;
	const int	mode = 0755;
	int		nres;

	/*
	 * check for an existing lost+found first, if it exists, return
	 * it's inode. Otherwise, we can create it. Bad lost+found inodes
	 * would have been cleared in phase3 and phase4.
	 */

	if ((i = libxfs_iget(mp, NULL, mp->m_sb.sb_rootino, 0, &pip, 0)))
		do_error(_("%d - couldn't iget root inode to obtain %s\n"),
			i, ORPHANAGE);

	if (dir_lookup(mp, NULL, pip, ORPHANAGE, strlen(ORPHANAGE),
			&ino) == 0)
		return ino;

	/*
	 * could not be found, create it
	 */

	tp = libxfs_trans_alloc(mp, 0);
	XFS_BMAP_INIT(&flist, &first);

	nres = XFS_MKDIR_SPACE_RES(mp, strlen(ORPHANAGE));
	if ((i = libxfs_trans_reserve(tp, nres, XFS_MKDIR_LOG_RES(mp), 0,
				XFS_TRANS_PERM_LOG_RES, XFS_MKDIR_LOG_COUNT)))
		res_failed(i);

	/*
	 * use iget/ijoin instead of trans_iget because the ialloc
	 * wrapper can commit the transaction and start a new one
	 */
/*	if ((i = libxfs_iget(mp, NULL, mp->m_sb.sb_rootino, 0, &pip, 0)))
		do_error(_("%d - couldn't iget root inode to make %s\n"),
			i, ORPHANAGE);*/

	error = libxfs_inode_alloc(&tp, pip, mode|S_IFDIR,
					1, 0, &zerocr, &zerofsx, &ip);
	if (error) {
		do_error(_("%s inode allocation failed %d\n"),
			ORPHANAGE, error);
	}
	ip->i_d.di_nlink++;		/* account for . */

	/*
	 * now that we know the transaction will stay around,
	 * add the root inode to it
	 */
	libxfs_trans_ijoin(tp, pip, 0);

	/*
	 * create the actual entry
	 */
	if ((error = dir_createname(mp, tp, pip, ORPHANAGE,
			strlen(ORPHANAGE), ip->i_ino, &first, &flist, nres))) {
		do_error(
		_("can't make %s, createname error %d\n"),
			ORPHANAGE, error);
	}

	/*
	 * bump up the link count in the root directory to account
	 * for .. in the new directory
	 */
	pip->i_d.di_nlink++;
	add_inode_ref(find_inode_rec(XFS_INO_TO_AGNO(mp, mp->m_sb.sb_rootino),
				XFS_INO_TO_AGINO(mp, mp->m_sb.sb_rootino)), 0);


	libxfs_trans_log_inode(tp, pip, XFS_ILOG_CORE);
	dir_init(mp, tp, ip, pip);
	libxfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);

	error = libxfs_bmap_finish(&tp, &flist, first, &committed);
	if (error) {
		do_error(_("%s directory creation failed -- bmapf error %d\n"),
			ORPHANAGE, error);
	}

	ino = ip->i_ino;

	libxfs_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES|XFS_TRANS_SYNC, 0);

	return(ino);
}

/*
 * move a file to the orphange.
 */
static void
mv_orphanage(
	xfs_mount_t		*mp,
	xfs_ino_t		ino,		/* inode # to be moved */
	int			isa_dir)	/* 1 if inode is a directory */
{
	xfs_inode_t		*orphanage_ip;
	xfs_ino_t		entry_ino_num;
	xfs_inode_t		*ino_p;
	xfs_trans_t		*tp;
	xfs_fsblock_t		first;
	xfs_bmap_free_t		flist;
	int			err;
	int			committed;
	char			fname[MAXPATHLEN + 1];
	int			fnamelen;
	int			nres;
	int			incr;
	ino_tree_node_t		*irec;
	int			ino_offset = 0;

	fnamelen = snprintf(fname, sizeof(fname), "%llu",
			(unsigned long long)ino);

	err = libxfs_iget(mp, NULL, orphanage_ino, 0, &orphanage_ip, 0);
	if (err)
		do_error(_("%d - couldn't iget orphanage inode\n"), err);
	/*
	 * Make sure the filename is unique in the lost+found
	 */
	incr = 0;
	while (dir_lookup(mp, NULL, orphanage_ip, fname, fnamelen,
			&entry_ino_num) == 0) {
		fnamelen = snprintf(fname, sizeof(fname), "%llu.%d",
				(unsigned long long)ino, ++incr);
	}

	tp = libxfs_trans_alloc(mp, 0);

	if ((err = libxfs_iget(mp, NULL, ino, 0, &ino_p, 0)))
		do_error(_("%d - couldn't iget disconnected inode\n"), err);

	if (isa_dir)  {
		irec = find_inode_rec(XFS_INO_TO_AGNO(mp, orphanage_ino),
				XFS_INO_TO_AGINO(mp, orphanage_ino));
		if (irec)
			ino_offset = XFS_INO_TO_AGINO(mp, orphanage_ino) -
					irec->ino_startnum;
		nres = XFS_DIRENTER_SPACE_RES(mp, fnamelen) +
		       XFS_DIRENTER_SPACE_RES(mp, 2);
		err = dir_lookup(mp, tp, ino_p, "..", 2, &entry_ino_num);
		if (err) {
			ASSERT(err == ENOENT);

			if ((err = libxfs_trans_reserve(tp, nres,
					XFS_RENAME_LOG_RES(mp), 0,
					XFS_TRANS_PERM_LOG_RES,
					XFS_RENAME_LOG_COUNT)))
				do_error(
	_("space reservation failed (%d), filesystem may be out of space\n"),
					err);

			libxfs_trans_ijoin(tp, orphanage_ip, 0);
			libxfs_trans_ijoin(tp, ino_p, 0);

			XFS_BMAP_INIT(&flist, &first);
			if ((err = dir_createname(mp, tp, orphanage_ip, fname,
						fnamelen, ino, &first,
						&flist, nres)))
				do_error(
	_("name create failed in %s (%d), filesystem may be out of space\n"),
					ORPHANAGE, err);

			if (irec)
				add_inode_ref(irec, ino_offset);
			else
				orphanage_ip->i_d.di_nlink++;
			libxfs_trans_log_inode(tp, orphanage_ip, XFS_ILOG_CORE);

			if ((err = dir_createname(mp, tp, ino_p, "..", 2,
						orphanage_ino, &first, &flist, nres)))
				do_error(
	_("creation of .. entry failed (%d), filesystem may be out of space\n"),
					err);

			ino_p->i_d.di_nlink++;
			libxfs_trans_log_inode(tp, ino_p, XFS_ILOG_CORE);

			if ((err = libxfs_bmap_finish(&tp, &flist, first, &committed)))
				do_error(
	_("bmap finish failed (err - %d), filesystem may be out of space\n"),
					err);

			libxfs_trans_commit(tp,
				XFS_TRANS_RELEASE_LOG_RES|XFS_TRANS_SYNC, 0);
		} else  {
			if ((err = libxfs_trans_reserve(tp, nres,
					XFS_RENAME_LOG_RES(mp), 0,
					XFS_TRANS_PERM_LOG_RES,
					XFS_RENAME_LOG_COUNT)))
				do_error(
	_("space reservation failed (%d), filesystem may be out of space\n"),
					err);

			libxfs_trans_ijoin(tp, orphanage_ip, 0);
			libxfs_trans_ijoin(tp, ino_p, 0);

			XFS_BMAP_INIT(&flist, &first);

			if ((err = dir_createname(mp, tp, orphanage_ip, fname,
						fnamelen, ino, &first,
						&flist, nres)))
				do_error(
	_("name create failed in %s (%d), filesystem may be out of space\n"),
					ORPHANAGE, err);

			if (irec)
				add_inode_ref(irec, ino_offset);
			else
				orphanage_ip->i_d.di_nlink++;
			libxfs_trans_log_inode(tp, orphanage_ip, XFS_ILOG_CORE);

			/*
			 * don't replace .. value if it already points
			 * to us.  that'll pop a libxfs/kernel ASSERT.
			 */
			if (entry_ino_num != orphanage_ino)  {
				if ((err = dir_replace(mp, tp, ino_p, "..",
							2, orphanage_ino, &first,
							&flist, nres)))
					do_error(
	_("name replace op failed (%d), filesystem may be out of space\n"),
						err);
			}

			if ((err = libxfs_bmap_finish(&tp, &flist, first,
							&committed)))
				do_error(
	_("bmap finish failed (%d), filesystem may be out of space\n"),
					err);

			libxfs_trans_commit(tp,
				XFS_TRANS_RELEASE_LOG_RES|XFS_TRANS_SYNC, 0);
		}

	} else  {
		/*
		 * use the remove log reservation as that's
		 * more accurate.  we're only creating the
		 * links, we're not doing the inode allocation
		 * also accounted for in the create
		 */
		nres = XFS_DIRENTER_SPACE_RES(mp, fnamelen);
		if ((err = libxfs_trans_reserve(tp, nres, XFS_REMOVE_LOG_RES(mp), 0,
				XFS_TRANS_PERM_LOG_RES, XFS_REMOVE_LOG_COUNT)))
			do_error(
	_("space reservation failed (%d), filesystem may be out of space\n"),
				err);

		libxfs_trans_ijoin(tp, orphanage_ip, 0);
		libxfs_trans_ijoin(tp, ino_p, 0);

		XFS_BMAP_INIT(&flist, &first);
		if ((err = dir_createname(mp, tp, orphanage_ip, fname,
				fnamelen, ino, &first, &flist, nres)))
			do_error(
	_("name create failed in %s (%d), filesystem may be out of space\n"),
				ORPHANAGE, err);
		ASSERT(err == 0);

		ino_p->i_d.di_nlink = 1;
		libxfs_trans_log_inode(tp, ino_p, XFS_ILOG_CORE);

		if ((err = libxfs_bmap_finish(&tp, &flist, first, &committed)))
			do_error(
	_("bmap finish failed (%d), filesystem may be out of space\n"),
				err);

		libxfs_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES|XFS_TRANS_SYNC, 0);
	}
}

/*
 * like get_first_dblock_fsbno only it uses the simulation code instead
 * of raw I/O.
 *
 * Returns the fsbno of the first (leftmost) block in the directory leaf.
 * sets *bno to the directory block # corresponding to the returned fsbno.
 */
static xfs_dfsbno_t
map_first_dblock_fsbno(xfs_mount_t	*mp,
			xfs_ino_t	ino,
			xfs_inode_t	*ip,
			xfs_dablk_t	*bno)
{
	xfs_fsblock_t		fblock;
	xfs_da_intnode_t	*node;
	xfs_buf_t		*bp;
	xfs_dablk_t		da_bno;
	xfs_dfsbno_t		fsbno;
	xfs_bmbt_irec_t		map;
	int			nmap;
	int			i;
	int			error;
	char			*ftype;

	/*
	 * traverse down left-side of tree until we hit the
	 * left-most leaf block setting up the btree cursor along
	 * the way.
	 */
	da_bno = 0;
	*bno = 0;
	i = -1;
	node = NULL;
	fblock = NULLFSBLOCK;
	ftype = _("dir");

	nmap = 1;
	error = libxfs_bmapi(NULL, ip, (xfs_fileoff_t) da_bno, 1,
			XFS_BMAPI_METADATA, &fblock, 0,
			&map, &nmap, NULL);
	if (error || nmap != 1)  {
		if (!no_modify)
			do_error(
_("can't map block %d in %s inode %llu, xfs_bmapi returns %d, nmap = %d\n"),
				da_bno, ftype, ino, error, nmap);
		else  {
			do_warn(
_("can't map block %d in %s inode %llu, xfs_bmapi returns %d, nmap = %d\n"),
				da_bno, ftype, ino, error, nmap);
			return(NULLDFSBNO);
		}
	}

	if ((fsbno = map.br_startblock) == HOLESTARTBLOCK)  {
		if (!no_modify)
			do_error(_("block %d in %s ino %llu doesn't exist\n"),
				da_bno, ftype, ino);
		else  {
			do_warn(_("block %d in %s ino %llu doesn't exist\n"),
				da_bno, ftype, ino);
			return(NULLDFSBNO);
		}
	}

	if (ip->i_d.di_size <= XFS_LBSIZE(mp))
		return(fsbno);

	if (XFS_SB_VERSION_HASDIRV2(&mp->m_sb))
		return(fsbno);

	do {
		/*
		 * walk down left side of btree, release buffers as you
		 * go.  if the root block is a leaf (single-level btree),
		 * just return it.
		 *
		 */

		bp = libxfs_readbuf(mp->m_dev, XFS_FSB_TO_DADDR(mp, fsbno),
				XFS_FSB_TO_BB(mp, 1), 0);

		if (!bp) {
			do_warn(
	_("can't read block %u (fsbno %llu) for directory inode %llu\n"),
					da_bno, fsbno, ino);
			return(NULLDFSBNO);
		}

		node = (xfs_da_intnode_t *)XFS_BUF_PTR(bp);

		if (INT_GET(node->hdr.info.magic, ARCH_CONVERT) != XFS_DA_NODE_MAGIC)  {
			libxfs_putbuf(bp);
			do_warn(
_("bad dir/attr magic number in inode %llu, file bno = %u, fsbno = %llu\n"),
				ino, da_bno, fsbno);
			return(NULLDFSBNO);
		}

		if (i == -1)
			i = INT_GET(node->hdr.level, ARCH_CONVERT);

		da_bno = INT_GET(node->btree[0].before, ARCH_CONVERT);

		libxfs_putbuf(bp);
		bp = NULL;

		nmap = 1;
		error = libxfs_bmapi(NULL, ip, (xfs_fileoff_t) da_bno, 1,
				XFS_BMAPI_METADATA, &fblock, 0,
				&map, &nmap, NULL);
		if (error || nmap != 1)  {
			if (!no_modify)
				do_error(
_("can't map block %d in %s ino %llu, xfs_bmapi returns %d, nmap = %d\n"),
					da_bno, ftype, ino, error, nmap);
			else  {
				do_warn(
_("can't map block %d in %s ino %llu, xfs_bmapi returns %d, nmap = %d\n"),
					da_bno, ftype, ino, error, nmap);
				return(NULLDFSBNO);
			}
		}
		if ((fsbno = map.br_startblock) == HOLESTARTBLOCK)  {
			if (!no_modify)
				do_error(
				_("block %d in %s inode %llu doesn't exist\n"),
					da_bno, ftype, ino);
			else  {
				do_warn(
				_("block %d in %s inode %llu doesn't exist\n"),
					da_bno, ftype, ino);
				return(NULLDFSBNO);
			}
		}

		i--;
	} while(i > 0);

	*bno = da_bno;
	return(fsbno);
}

/*
 * scan longform directory and prune first bad entry.  returns 1 if
 * it had to remove something, 0 if it made it all the way through
 * the directory.  prune_lf_dir_entry does all the necessary bmap calls.
 *
 * hashval is an in/out -- starting hashvalue in, hashvalue of the
 *			deleted entry (if there was one) out
 *
 * this routine can NOT be called if running in no modify mode
 */
static int
prune_lf_dir_entry(xfs_mount_t *mp, xfs_ino_t ino, xfs_inode_t *ip,
			xfs_dahash_t *hashval)
{
	xfs_dfsbno_t		fsbno;
	int			i;
	int			index;
	int			error;
	int			namelen;
	xfs_bmap_free_t		free_list;
	xfs_fsblock_t		first_block;
	xfs_buf_t		*bp;
	xfs_dir_leaf_name_t	*namest;
	xfs_dir_leafblock_t	*leaf;
	xfs_dir_leaf_entry_t	*entry;
	xfs_trans_t		*tp;
	xfs_dablk_t		da_bno;
	xfs_fsblock_t		fblock;
	int			committed;
	int			nmap;
	xfs_bmbt_irec_t		map;
	char			fname[MAXNAMELEN + 1];
	char			*ftype;
	int			nres;

	/*
	 * ok, this is kind of a schizoid routine.  we use our
	 * internal bmapi routines to walk the directory.  when
	 * we find a bogus entry, we release the buffer so
	 * the simulation code doesn't deadlock and use the
	 * sim code to remove the entry.  That will cause an
	 * extra bmap traversal to map the block but I think
	 * that's preferable to hacking the bogus removename
	 * function to be really different and then trying to
	 * maintain both versions as time goes on.
	 *
	 * first, grab the dinode and find the right leaf block.
	 */

	ftype = _("dir");
	da_bno = 0;
	bp = NULL;
	namest = NULL;
	fblock = NULLFSBLOCK;

	fsbno = map_first_dblock_fsbno(mp, ino, ip, &da_bno);

	/*
	 * now go foward along the leaves of the btree looking
	 * for an entry beginning with '/'
	 */
	do {
		bp = libxfs_readbuf(mp->m_dev, XFS_FSB_TO_DADDR(mp, fsbno),
				XFS_FSB_TO_BB(mp, 1), 0);

		if (!bp)  {
			do_error(
	_("can't read directory inode %llu (leaf) block %u (fsbno %llu)\n"),
				ino, da_bno, fsbno);
			/* NOTREACHED */
		}

		leaf = (xfs_dir_leafblock_t *)XFS_BUF_PTR(bp);
		ASSERT(INT_GET(leaf->hdr.info.magic, ARCH_CONVERT) == XFS_DIR_LEAF_MAGIC);
		entry = &leaf->entries[0];

		for (index = -1, i = 0;
				i < INT_GET(leaf->hdr.count, ARCH_CONVERT) && index == -1;
				i++)  {
			namest = XFS_DIR_LEAF_NAMESTRUCT(leaf, INT_GET(entry->nameidx, ARCH_CONVERT));
			if (namest->name[0] != '/')
				entry++;
			else
				index = i;
		}

		/*
		 * if we got a bogus entry, exit loop with a pointer to
		 * the leaf block buffer.  otherwise, keep trying blocks
		 */
		da_bno = INT_GET(leaf->hdr.info.forw, ARCH_CONVERT);

		if (index == -1)  {
			if (bp != NULL)  {
				libxfs_putbuf(bp);
				bp = NULL;
			}

			/*
			 * map next leaf block unless we've run out
			 */
			if (da_bno != 0)  {
				nmap = 1;
				error = libxfs_bmapi(NULL, ip,
						(xfs_fileoff_t) da_bno, 1,
						XFS_BMAPI_METADATA, &fblock, 0,
						&map, &nmap, NULL);
				if (error || nmap != 1)
					do_error(
_("can't map block %d in directory %llu, xfs_bmapi returns %d, nmap = %d\n"),
						da_bno, ino, error, nmap);
				if ((fsbno = map.br_startblock)
						== HOLESTARTBLOCK)  {
					do_error(
				_("%s ino %llu block %d doesn't exist\n"),
						ftype, ino, da_bno);
				}
			}
		}
	} while (da_bno != 0 && index == -1);

	/*
	 * if we hit the edge of the tree with no bad entries, we're done
	 * and the buffer was released.
	 */
	if (da_bno == 0 && index == -1)
		return(0);

	ASSERT(index >= 0);
	ASSERT(entry == &leaf->entries[index]);
	ASSERT(namest == XFS_DIR_LEAF_NAMESTRUCT(leaf,
				INT_GET(entry->nameidx, ARCH_CONVERT)));

	/*
	 * snag the info we need out of the directory then release all buffers
	 */
	memmove(fname, namest->name, entry->namelen);
	fname[entry->namelen] = '\0';
	*hashval = INT_GET(entry->hashval, ARCH_CONVERT);
	namelen = entry->namelen;

	libxfs_putbuf(bp);

	/*
	 * ok, now the hard part, blow away the index'th entry in this block
	 *
	 * allocate a remove transaction for it.  that's not quite true since
	 * we're only messing with one inode, not two but...
	 */

	tp = libxfs_trans_alloc(mp, XFS_TRANS_REMOVE);

	nres = XFS_REMOVE_SPACE_RES(mp);
	error = libxfs_trans_reserve(tp, nres, XFS_REMOVE_LOG_RES(mp),
				    0, XFS_TRANS_PERM_LOG_RES,
				    XFS_REMOVE_LOG_COUNT);
	if (error)
		res_failed(error);

	libxfs_trans_ijoin(tp, ip, 0);
	libxfs_trans_ihold(tp, ip);

	XFS_BMAP_INIT(&free_list, &first_block);

	error = dir_bogus_removename(mp, tp, ip, fname,
		&first_block, &free_list, nres, *hashval, namelen);

	if (error)  {
		do_error(
_("couldn't remove bogus entry \"%s\" in\n\tdirectory inode %llu, errno = %d\n"),
			fname, ino, error);
		/* NOTREACHED */
	}

	error = libxfs_bmap_finish(&tp, &free_list, first_block, &committed);

	ASSERT(error == 0);

	libxfs_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES|XFS_TRANS_SYNC, 0);

	return(1);
}

static int
entry_junked(
	const char 	*msg,
	const char	*iname,
	xfs_ino_t	ino1,
	xfs_ino_t	ino2)
{
	do_warn(msg, iname, ino1, ino2);
	if (!no_modify) {
		if (verbose)
			do_warn(_(", marking entry to be junked\n"));
		else
			do_warn("\n");
	} else
		do_warn(_(", would junk entry\n"));
	return !no_modify;
}

/*
 * process a leaf block, also checks for .. entry
 * and corrects it to match what we think .. should be
 */
static void
lf_block_dir_entry_check(xfs_mount_t		*mp,
			xfs_ino_t		ino,
			xfs_dir_leafblock_t	*leaf,
			int			*dirty,
			int			*num_illegal,
			int			*need_dot,
			ino_tree_node_t		*current_irec,
			int			current_ino_offset,
			dir_hash_tab_t		*hashtab,
			xfs_dablk_t		da_bno)
{
	xfs_dir_leaf_entry_t	*entry;
	ino_tree_node_t		*irec;
	xfs_ino_t		lino;
	xfs_ino_t		parent;
	xfs_dir_leaf_name_t	*namest;
	int			i;
	int			junkit;
	int			ino_offset;
	int			nbad;
	char			fname[MAXNAMELEN + 1];

	entry = &leaf->entries[0];
	*dirty = 0;
	nbad = 0;

	/*
	 * look at each entry.  reference inode pointed to by each
	 * entry in the incore inode tree.
	 * if not a directory, set reached flag, increment link count
	 * if a directory and reached, mark entry as to be deleted.
	 * if a directory, check to see if recorded parent
	 *	matches current inode #,
	 *	if so, then set reached flag, increment link count
	 *		of current and child dir inodes, push the child
	 *		directory inode onto the directory stack.
	 *	if current inode != parent, then mark entry to be deleted.
	 *
	 * return
	 */
	for (i = 0; i < INT_GET(leaf->hdr.count, ARCH_CONVERT); entry++, i++)  {
		/*
		 * snag inode #, update link counts, and make sure
		 * this isn't a loop if the child is a directory
		 */
		namest = XFS_DIR_LEAF_NAMESTRUCT(leaf, INT_GET(entry->nameidx, ARCH_CONVERT));

		/*
		 * skip bogus entries (leading '/').  they'll be deleted
		 * later
		 */
		if (namest->name[0] == '/')  {
			nbad++;
			continue;
		}

		junkit = 0;

		XFS_DIR_SF_GET_DIRINO(&namest->inumber, &lino);
		memmove(fname, namest->name, entry->namelen);
		fname[entry->namelen] = '\0';

		ASSERT(lino != NULLFSINO);

		/*
		 * skip the '..' entry since it's checked when the
		 * directory is reached by something else.  if it never
		 * gets reached, it'll be moved to the orphanage and we'll
		 * take care of it then.
		 */
		if (entry->namelen == 2 && namest->name[0] == '.' &&
				namest->name[1] == '.')
			continue;

		ASSERT(no_modify || !verify_inum(mp, lino));

		/*
		 * special case the . entry.  we know there's only one
		 * '.' and only '.' points to itself because bogus entries
		 * got trashed in phase 3 if there were > 1.
		 * bump up link count for '.' but don't set reached
		 * until we're actually reached by another directory
		 * '..' is already accounted for or will be taken care
		 * of when directory is moved to orphanage.
		 */
		if (ino == lino)  {
			ASSERT(namest->name[0] == '.' && entry->namelen == 1);
			add_inode_ref(current_irec, current_ino_offset);
			*need_dot = 0;
			continue;
		}

		/*
		 * skip entries with bogus inumbers if we're in no modify mode
		 */
		if (no_modify && verify_inum(mp, lino))
			continue;

		/*
		 * ok, now handle the rest of the cases besides '.' and '..'
		 */
		irec = find_inode_rec(XFS_INO_TO_AGNO(mp, lino),
					XFS_INO_TO_AGINO(mp, lino));

		if (irec == NULL)  {
			nbad++;
			if (entry_junked(_("entry \"%s\" in dir inode %llu "
					"points to non-existent inode %llu"),
					fname, ino, lino)) {
				namest->name[0] = '/';
				*dirty = 1;
			}
			continue;
		}

		ino_offset = XFS_INO_TO_AGINO(mp, lino) - irec->ino_startnum;

		/*
		 * if it's a free inode, blow out the entry.
		 * by now, any inode that we think is free
		 * really is free.
		 */
		if (is_inode_free(irec, ino_offset))  {
			nbad++;
			if (entry_junked(_("entry \"%s\" in dir inode %llu "
					"points to free inode %llu"),
					fname, ino, lino)) {
				namest->name[0] = '/';
				*dirty = 1;
			}
			continue;
		}
		/*
		 * check if this inode is lost+found dir in the root
		 */
		if (ino == mp->m_sb.sb_rootino && strcmp(fname, ORPHANAGE) == 0) {
			/* root inode, "lost+found", if it's not a directory,
			 * trash it, otherwise, assign it */
			if (!inode_isadir(irec, ino_offset)) {
				nbad++;
				if (entry_junked(_("%s (ino %llu) in root "
						"(%llu) is not a directory"),
						ORPHANAGE, lino, ino)) {
					namest->name[0] = '/';
					*dirty = 1;
				}
				continue;
			}
			/*
			 * if this is a dup, it will be picked up below,
			 * otherwise, mark it as the orphanage for later.
			 */
			if (!orphanage_ino)
				orphanage_ino = lino;
		}
		/*
		 * check for duplicate names in directory.
		 */
		if (!dir_hash_add(hashtab, (da_bno << mp->m_sb.sb_blocklog) +
				entry->nameidx, lino, entry->namelen,
				namest->name)) {
			nbad++;
			if (entry_junked(_("entry \"%s\" (ino %llu) in dir "
					"%llu is a duplicate name"),
					fname, lino, ino)) {
				namest->name[0] = '/';
				*dirty = 1;
			}
			if (lino == orphanage_ino)
				orphanage_ino = 0;
			continue;
		}
		/*
		 * check easy case first, regular inode, just bump
		 * the link count and continue
		 */
		if (!inode_isadir(irec, ino_offset))  {
			add_inode_reached(irec, ino_offset);
			continue;
		}

		parent = get_inode_parent(irec, ino_offset);
		ASSERT(parent != 0);

		/*
		 * bump up the link counts in parent and child
		 * directory but if the link doesn't agree with
		 * the .. in the child, blow out the entry.
		 * if the directory has already been reached,
		 * blow away the entry also.
		 */
		if (is_inode_reached(irec, ino_offset))  {
			junkit = 1;
			do_warn(_("entry \"%s\" in dir %llu points to an "
				"already connected dir inode %llu,\n"),
				fname, ino, lino);
		} else if (parent == ino)  {
			add_inode_reached(irec, ino_offset);
			add_inode_ref(current_irec, current_ino_offset);
		} else if (parent == NULLFSINO) {
			/* ".." was missing, but this entry refers to it,
			   so, set it as the parent and mark for rebuild */
			do_warn(_("entry \"%s\" in dir ino %llu doesn't have a"
				" .. entry, will set it in ino %llu.\n"),
				fname, ino, lino);
			set_inode_parent(irec, ino_offset, ino);
			add_inode_reached(irec, ino_offset);
			add_inode_ref(current_irec, current_ino_offset);
		} else {
			junkit = 1;
			do_warn(_("entry \"%s\" in dir ino %llu not consistent"
				" with .. value (%llu) in ino %llu,\n"),
				fname, ino, parent, lino);
		}

		if (junkit)  {
			if (lino == orphanage_ino)
				orphanage_ino = 0;
			junkit = 0;
			nbad++;
			if (!no_modify)  {
				namest->name[0] = '/';
				*dirty = 1;
				if (verbose)
					do_warn(
					_("\twill clear entry \"%s\"\n"),
						fname);
			} else  {
				do_warn(_("\twould clear entry \"%s\"\n"),
					fname);
			}
		}
	}

	*num_illegal += nbad;
}

/*
 * succeeds or dies, inode never gets dirtied since all changes
 * happen in file blocks.  the inode size and other core info
 * is already correct, it's just the leaf entries that get altered.
 */
static void
longform_dir_entry_check(xfs_mount_t	*mp,
			xfs_ino_t	ino,
			xfs_inode_t	*ip,
			int		*num_illegal,
			int		*need_dot,
			ino_tree_node_t	*irec,
			int		ino_offset,
			dir_hash_tab_t	*hashtab)
{
	xfs_dir_leafblock_t	*leaf;
	xfs_buf_t		*bp;
	xfs_dfsbno_t		fsbno;
	xfs_fsblock_t		fblock;
	xfs_dablk_t		da_bno;
	int			dirty;
	int			nmap;
	int			error;
	int			skipit;
	xfs_bmbt_irec_t		map;
	char			*ftype;

	da_bno = 0;
	fblock = NULLFSBLOCK;
	*need_dot = 1;
	ftype = _("dir");

	fsbno = map_first_dblock_fsbno(mp, ino, ip, &da_bno);

	if (fsbno == NULLDFSBNO && no_modify)  {
		do_warn(_("cannot map block 0 of directory inode %llu\n"), ino);
		return;
	}

	do {
		ASSERT(fsbno != NULLDFSBNO);
		skipit = 0;

		bp = libxfs_readbuf(mp->m_dev, XFS_FSB_TO_DADDR(mp, fsbno),
				XFS_FSB_TO_BB(mp, 1), 0);

		if (!bp) {
			do_error(
	_("can't read block %u (fsbno %llu) for directory inode %llu\n"),
					da_bno, fsbno, ino);
			/* NOTREACHED */
		}

		leaf = (xfs_dir_leafblock_t *)XFS_BUF_PTR(bp);

		if (INT_GET(leaf->hdr.info.magic, ARCH_CONVERT) !=
		    XFS_DIR_LEAF_MAGIC)  {
			if (!no_modify)  {
				do_error(
_("bad magic # (0x%x) for dir ino %llu leaf block (bno %u fsbno %llu)\n"),
					INT_GET(leaf->hdr.info.magic,
						ARCH_CONVERT),
					ino, da_bno, fsbno);
				/* NOTREACHED */
			} else  {
				/*
				 * this block's bad but maybe the
				 * forward pointer is good...
				 */
				skipit = 1;
				dirty = 0;
			}
		}

		if (!skipit)
			lf_block_dir_entry_check(mp, ino, leaf, &dirty,
					num_illegal, need_dot, irec,
					ino_offset, hashtab, da_bno);

		da_bno = INT_GET(leaf->hdr.info.forw, ARCH_CONVERT);

		ASSERT(dirty == 0 || (dirty && !no_modify));

		if (dirty && !no_modify)
			libxfs_writebuf(bp, 0);
		else
			libxfs_putbuf(bp);
		bp = NULL;

		if (da_bno != 0)  {
			nmap = 1;
			error = libxfs_bmapi(NULL, ip, (xfs_fileoff_t)da_bno, 1,
					XFS_BMAPI_METADATA, &fblock, 0,
					&map, &nmap, NULL);
			if (error || nmap != 1)  {
				if (!no_modify)
					do_error(
_("can't map leaf block %d in dir %llu, xfs_bmapi returns %d, nmap = %d\n"),
						da_bno, ino, error, nmap);
				else  {
					do_warn(
_("can't map leaf block %d in dir %llu, xfs_bmapi returns %d, nmap = %d\n"),
						da_bno, ino, error, nmap);
					return;
				}
			}
			if ((fsbno = map.br_startblock) == HOLESTARTBLOCK)  {
				if (!no_modify)
					do_error(
				_("block %d in %s ino %llu doesn't exist\n"),
						da_bno, ftype, ino);
				else  {
					do_warn(
				_("block %d in %s ino %llu doesn't exist\n"),
						da_bno, ftype, ino);
					return;
				}
			}
		}
	} while (da_bno != 0);
}

/*
 * Unexpected failure during the rebuild will leave the entries in
 * lost+found on the next run
 */

static void
longform_dir2_rebuild(
	xfs_mount_t		*mp,
	xfs_ino_t		ino,
	xfs_inode_t		*ip,
	ino_tree_node_t		*irec,
	int			ino_offset,
	dir_hash_tab_t		*hashtab)
{
	int			error;
	int			nres;
	xfs_trans_t		*tp;
	xfs_fileoff_t		lastblock;
	xfs_fsblock_t		firstblock;
	xfs_bmap_free_t		flist;
	xfs_inode_t		pip;
	dir_hash_ent_t		*p;
	int			committed;
	int			done;

	/*
	 * trash directory completely and rebuild from scratch using the
	 * name/inode pairs in the hash table
	 */

	do_warn(_("rebuilding directory inode %llu\n"), ino);

	/*
	 * first attempt to locate the parent inode, if it can't be
	 * found, set it to the root inode and it'll be moved to the
	 * orphanage later (the inode number here needs to be valid
	 * for the libxfs_dir2_init() call).
	 */
	pip.i_ino = get_inode_parent(irec, ino_offset);
	if (pip.i_ino == NULLFSINO)
		pip.i_ino = mp->m_sb.sb_rootino;

	XFS_BMAP_INIT(&flist, &firstblock);

	tp = libxfs_trans_alloc(mp, 0);
	nres = XFS_REMOVE_SPACE_RES(mp);
	error = libxfs_trans_reserve(tp, nres, XFS_REMOVE_LOG_RES(mp), 0,
			XFS_TRANS_PERM_LOG_RES, XFS_REMOVE_LOG_COUNT);
	if (error)
		res_failed(error);
	libxfs_trans_ijoin(tp, ip, 0);
	libxfs_trans_ihold(tp, ip);

	if ((error = libxfs_bmap_last_offset(tp, ip, &lastblock,
						XFS_DATA_FORK)))
		do_error(_("xfs_bmap_last_offset failed -- error - %d\n"),
			error);

	/* free all data, leaf, node and freespace blocks */

	if ((error = libxfs_bunmapi(tp, ip, 0, lastblock,
			XFS_BMAPI_METADATA, 0, &firstblock, &flist,
			&done))) {
		do_warn(_("xfs_bunmapi failed -- error - %d\n"), error);
		libxfs_trans_cancel(tp, XFS_TRANS_RELEASE_LOG_RES |
					XFS_TRANS_ABORT);
		return;
	}

	ASSERT(done);

	libxfs_dir2_init(tp, ip, &pip);

	error = libxfs_bmap_finish(&tp, &flist, firstblock, &committed);

	libxfs_trans_commit(tp,
			XFS_TRANS_RELEASE_LOG_RES|XFS_TRANS_SYNC, 0);

	/* go through the hash list and re-add the inodes */

	for (p = hashtab->first; p; p = p->nextbyorder) {

		if (p->name[0] == '/' || (p->name[0] == '.' && (p->namelen == 1
				|| (p->namelen == 2 && p->name[1] == '.'))))
			continue;

		tp = libxfs_trans_alloc(mp, 0);
		nres = XFS_CREATE_SPACE_RES(mp, p->namelen);
		if ((error = libxfs_trans_reserve(tp, nres,
				XFS_CREATE_LOG_RES(mp), 0,
				XFS_TRANS_PERM_LOG_RES, XFS_CREATE_LOG_COUNT))) {
			do_warn(
	_("space reservation failed (%d), filesystem may be out of space\n"),
				error);
			break;
		}

		libxfs_trans_ijoin(tp, ip, 0);
		libxfs_trans_ihold(tp, ip);

		XFS_BMAP_INIT(&flist, &firstblock);
		if ((error = libxfs_dir2_createname(tp, ip, (uchar_t *)p->name,
				p->namelen, p->inum, &firstblock, &flist,
				nres))) {
			do_warn(
_("name create failed in ino %llu (%d), filesystem may be out of space\n"),
				ino, error);
			libxfs_trans_cancel(tp, XFS_TRANS_RELEASE_LOG_RES |
						XFS_TRANS_ABORT);
			break;
		}

		if ((error = libxfs_bmap_finish(&tp, &flist, firstblock,
				&committed))) {
			do_warn(
	_("bmap finish failed (%d), filesystem may be out of space\n"),
				error);
			libxfs_bmap_cancel(&flist);
			libxfs_trans_cancel(tp, XFS_TRANS_RELEASE_LOG_RES |
						XFS_TRANS_ABORT);
			break;
		}


		libxfs_trans_commit(tp,
				XFS_TRANS_RELEASE_LOG_RES|XFS_TRANS_SYNC, 0);
	}
}


/*
 * Kill a block in a version 2 inode.
 * Makes its own transaction.
 */
static void
dir2_kill_block(
	xfs_mount_t	*mp,
	xfs_inode_t	*ip,
	xfs_dablk_t	da_bno,
	xfs_dabuf_t	*bp)
{
	xfs_da_args_t	args;
	int		committed;
	int		error;
	xfs_fsblock_t	firstblock;
	xfs_bmap_free_t	flist;
	int		nres;
	xfs_trans_t	*tp;

	tp = libxfs_trans_alloc(mp, 0);
	nres = XFS_REMOVE_SPACE_RES(mp);
	error = libxfs_trans_reserve(tp, nres, XFS_REMOVE_LOG_RES(mp), 0,
			XFS_TRANS_PERM_LOG_RES, XFS_REMOVE_LOG_COUNT);
	if (error)
		res_failed(error);
	libxfs_trans_ijoin(tp, ip, 0);
	libxfs_trans_ihold(tp, ip);
	libxfs_da_bjoin(tp, bp);
	memset(&args, 0, sizeof(args));
	XFS_BMAP_INIT(&flist, &firstblock);
	args.dp = ip;
	args.trans = tp;
	args.firstblock = &firstblock;
	args.flist = &flist;
	args.whichfork = XFS_DATA_FORK;
	if (da_bno >= mp->m_dirleafblk && da_bno < mp->m_dirfreeblk)
		error = libxfs_da_shrink_inode(&args, da_bno, bp);
	else
		error = libxfs_dir2_shrink_inode(&args,
				XFS_DIR2_DA_TO_DB(mp, da_bno), bp);
	if (error)
		do_error(_("shrink_inode failed inode %llu block %u\n"),
			ip->i_ino, da_bno);
	libxfs_bmap_finish(&tp, &flist, firstblock, &committed);
	libxfs_trans_commit(tp, 0, 0);
}

/*
 * process a data block, also checks for .. entry
 * and corrects it to match what we think .. should be
 */
static void
longform_dir2_entry_check_data(
	xfs_mount_t		*mp,
	xfs_inode_t		*ip,
	int			*num_illegal,
	int			*need_dot,
	ino_tree_node_t		*current_irec,
	int			current_ino_offset,
	xfs_dabuf_t		**bpp,
	dir_hash_tab_t		*hashtab,
	freetab_t		**freetabp,
	xfs_dablk_t		da_bno,
	int			isblock)
{
	xfs_dir2_dataptr_t	addr;
	xfs_dir2_leaf_entry_t	*blp;
	xfs_dabuf_t		*bp;
	xfs_dir2_block_tail_t	*btp;
	int			committed;
	xfs_dir2_data_t		*d;
	xfs_dir2_db_t		db;
	xfs_dir2_data_entry_t	*dep;
	xfs_dir2_data_unused_t	*dup;
	char			*endptr;
	int			error;
	xfs_fsblock_t		firstblock;
	xfs_bmap_free_t		flist;
	char			fname[MAXNAMELEN + 1];
	freetab_t		*freetab;
	int			i;
	int			ino_offset;
	xfs_ino_t		inum;
	ino_tree_node_t		*irec;
	int			junkit;
	int			lastfree;
	int			len;
	int			nbad;
	int			needlog;
	int			needscan;
	xfs_ino_t		parent;
	char			*ptr;
	xfs_trans_t		*tp;
	int			wantmagic;

	bp = *bpp;
	d = bp->data;
	ptr = (char *)d->u;
	nbad = 0;
	needscan = needlog = 0;
	junkit = 0;
	freetab = *freetabp;
	if (isblock) {
		btp = XFS_DIR2_BLOCK_TAIL_P(mp, (xfs_dir2_block_t *)d);
		blp = XFS_DIR2_BLOCK_LEAF_P(btp);
		endptr = (char *)blp;
		if (endptr > (char *)btp)
			endptr = (char *)btp;
		wantmagic = XFS_DIR2_BLOCK_MAGIC;
	} else {
		endptr = (char *)d + mp->m_dirblksize;
		wantmagic = XFS_DIR2_DATA_MAGIC;
	}
	db = XFS_DIR2_DA_TO_DB(mp, da_bno);

	/* check for data block beyond expected end */
	if (freetab->naents <= db) {
		struct freetab_ent e;

		*freetabp = freetab = realloc(freetab, FREETAB_SIZE(db + 1));
		if (!freetab) {
			do_error(
	_("realloc failed in longform_dir2_entry_check_data (%u bytes)\n"),
				FREETAB_SIZE(db + 1));
		}
		e.v = NULLDATAOFF;
		e.s = 0;
		for (i = freetab->naents; i < db; i++)
			freetab->ents[i] = e;
		freetab->naents = db + 1;
	}

	/* check the data block */
	while (ptr < endptr) {

		/* check for freespace */
		dup = (xfs_dir2_data_unused_t *)ptr;
		if (XFS_DIR2_DATA_FREE_TAG ==
		    INT_GET(dup->freetag, ARCH_CONVERT)) {

			/* check for invalid freespace length */
			if (ptr + INT_GET(dup->length, ARCH_CONVERT) > endptr ||
			    INT_GET(dup->length, ARCH_CONVERT) == 0 ||
			    (INT_GET(dup->length, ARCH_CONVERT) &
			     (XFS_DIR2_DATA_ALIGN - 1)))
				break;

			/* check for invalid tag */
			if (INT_GET(*XFS_DIR2_DATA_UNUSED_TAG_P(dup),
				    ARCH_CONVERT) != (char *)dup - (char *)d)
				break;

			/* check for block with no data entries */
			if ((ptr == (char *)d->u) &&
			    (ptr + INT_GET(dup->length, ARCH_CONVERT) >=
			     endptr)) {
				junkit = 1;
				*num_illegal += 1;
				break;
			}

			/* continue at the end of the freespace */
			ptr += INT_GET(dup->length, ARCH_CONVERT);
			if (ptr >= endptr)
				break;
		}

		/* validate data entry size */
		dep = (xfs_dir2_data_entry_t *)ptr;
		if (ptr + XFS_DIR2_DATA_ENTSIZE(dep->namelen) > endptr)
			break;
		if (INT_GET(*XFS_DIR2_DATA_ENTRY_TAG_P(dep), ARCH_CONVERT) !=
		    (char *)dep - (char *)d)
			break;
		ptr += XFS_DIR2_DATA_ENTSIZE(dep->namelen);
	}

	/* did we find an empty or corrupt block? */
	if (ptr != endptr) {
		if (junkit) {
			do_warn(
			_("empty data block %u in directory inode %llu: "),
				da_bno, ip->i_ino);
		} else {
			do_warn(_("corrupt block %u in directory inode %llu: "),
				da_bno, ip->i_ino);
		}
		if (!no_modify) {
			do_warn(_("junking block\n"));
			dir2_kill_block(mp, ip, da_bno, bp);
		} else {
			do_warn(_("would junk block\n"));
			libxfs_da_brelse(NULL, bp);
		}
		freetab->ents[db].v = NULLDATAOFF;
		*bpp = NULL;
		return;
	}

	/* update number of data blocks processed */
	if (freetab->nents < db + 1)
		freetab->nents = db + 1;

	tp = libxfs_trans_alloc(mp, 0);
	error = libxfs_trans_reserve(tp, 0, XFS_REMOVE_LOG_RES(mp), 0,
		XFS_TRANS_PERM_LOG_RES, XFS_REMOVE_LOG_COUNT);
	if (error)
		res_failed(error);
	libxfs_trans_ijoin(tp, ip, 0);
	libxfs_trans_ihold(tp, ip);
	libxfs_da_bjoin(tp, bp);
	libxfs_da_bhold(tp, bp);
	XFS_BMAP_INIT(&flist, &firstblock);
	if (INT_GET(d->hdr.magic, ARCH_CONVERT) != wantmagic) {
		do_warn(_("bad directory block magic # %#x for directory inode "
			"%llu block %d: "),
			INT_GET(d->hdr.magic, ARCH_CONVERT), ip->i_ino, da_bno);
		if (!no_modify) {
			do_warn(_("fixing magic # to %#x\n"), wantmagic);
			INT_SET(d->hdr.magic, ARCH_CONVERT, wantmagic);
			needlog = 1;
		} else
			do_warn(_("would fix magic # to %#x\n"), wantmagic);
	}
	lastfree = 0;
	ptr = (char *)d->u;
	/*
	 * look at each entry.  reference inode pointed to by each
	 * entry in the incore inode tree.
	 * if not a directory, set reached flag, increment link count
	 * if a directory and reached, mark entry as to be deleted.
	 * if a directory, check to see if recorded parent
	 *	matches current inode #,
	 *	if so, then set reached flag, increment link count
	 *		of current and child dir inodes, push the child
	 *		directory inode onto the directory stack.
	 *	if current inode != parent, then mark entry to be deleted.
	 */
	while (ptr < endptr) {
		dup = (xfs_dir2_data_unused_t *)ptr;
		if (INT_GET(dup->freetag, ARCH_CONVERT) ==
		    				XFS_DIR2_DATA_FREE_TAG) {
			if (lastfree) {
				do_warn(_("directory inode %llu block %u has "
					  "consecutive free entries: "),
					ip->i_ino, da_bno);
				if (!no_modify) {
					do_warn(_("joining together\n"));
					len = INT_GET(dup->length, ARCH_CONVERT);
					libxfs_dir2_data_use_free(tp, bp, dup,
						ptr - (char *)d, len, &needlog,
						&needscan);
					libxfs_dir2_data_make_free(tp, bp,
						ptr - (char *)d, len, &needlog,
						&needscan);
				} else
					do_warn(_("would join together\n"));
			}
			ptr += INT_GET(dup->length, ARCH_CONVERT);
			lastfree = 1;
			continue;
		}
		addr = XFS_DIR2_DB_OFF_TO_DATAPTR(mp, db, ptr - (char *)d);
		dep = (xfs_dir2_data_entry_t *)ptr;
		ptr += XFS_DIR2_DATA_ENTSIZE(dep->namelen);
		inum = INT_GET(dep->inumber, ARCH_CONVERT);
		lastfree = 0;
		/*
		 * skip bogus entries (leading '/').  they'll be deleted
		 * later.  must still log it, else we leak references to
		 * buffers.
		 */
		if (dep->name[0] == '/')  {
			nbad++;
			if (!no_modify)
				libxfs_dir2_data_log_entry(tp, bp, dep);
			continue;
		}

		memmove(fname, dep->name, dep->namelen);
		fname[dep->namelen] = '\0';
		ASSERT(inum != NULLFSINO);

		irec = find_inode_rec(XFS_INO_TO_AGNO(mp, inum),
					XFS_INO_TO_AGINO(mp, inum));
		if (irec == NULL)  {
			nbad++;
			if (entry_junked(_("entry \"%s\" in directory inode "
					"%llu points to non-existent inode %llu"),
					fname, ip->i_ino, inum)) {
				dep->name[0] = '/';
				libxfs_dir2_data_log_entry(tp, bp, dep);
			}
			continue;
		}
		ino_offset = XFS_INO_TO_AGINO(mp, inum) - irec->ino_startnum;

		/*
		 * if it's a free inode, blow out the entry.
		 * by now, any inode that we think is free
		 * really is free.
		 */
		if (is_inode_free(irec, ino_offset))  {
			nbad++;
			if (entry_junked(_("entry \"%s\" in directory inode "
					"%llu points to free inode %llu"),
					fname, ip->i_ino, inum)) {
				dep->name[0] = '/';
				libxfs_dir2_data_log_entry(tp, bp, dep);
			}
			continue;
		}

		/*
		 * check if this inode is lost+found dir in the root
		 */
		if (inum == mp->m_sb.sb_rootino && strcmp(fname, ORPHANAGE) == 0) {
			/*
			 * if it's not a directory, trash it
			 */
			if (!inode_isadir(irec, ino_offset)) {
				nbad++;
				if (entry_junked(_("%s (ino %llu) in root "
						"(%llu) is not a directory"),
						ORPHANAGE, inum, ip->i_ino)) {
					dep->name[0] = '/';
					libxfs_dir2_data_log_entry(tp, bp, dep);
				}
				continue;
			}
			/*
			 * if this is a dup, it will be picked up below,
			 * otherwise, mark it as the orphanage for later.
			 */
			if (!orphanage_ino)
				orphanage_ino = inum;
		}
		/*
		 * check for duplicate names in directory.
		 */
		if (!dir_hash_add(hashtab, addr, inum, dep->namelen,
				dep->name)) {
			nbad++;
			if (entry_junked(_("entry \"%s\" (ino %llu) in dir "
					"%llu is a duplicate name"),
					fname, inum, ip->i_ino)) {
				dep->name[0] = '/';
				libxfs_dir2_data_log_entry(tp, bp, dep);
			}
			if (inum == orphanage_ino)
				orphanage_ino = 0;
			continue;
		}

		/*
		 * skip the '..' entry since it's checked when the
		 * directory is reached by something else.  if it never
		 * gets reached, it'll be moved to the orphanage and we'll
		 * take care of it then. If it doesn't exist at all, the
		 * directory needs to be rebuilt first before being added
		 * to the orphanage.
		 */
		if (dep->namelen == 2 && dep->name[0] == '.' &&
				dep->name[1] == '.') {
			if (da_bno != 0) {
				/* ".." should be in the first block */
				nbad++;
				if (entry_junked(_("entry \"%s\" (ino %llu) "
						"in dir %llu is not in the "
						"the first block"), fname,
						inum, ip->i_ino)) {
					dep->name[0] = '/';
					libxfs_dir2_data_log_entry(tp, bp, dep);
				}
			}
			continue;
		}
		ASSERT(no_modify || !verify_inum(mp, inum));
		/*
		 * special case the . entry.  we know there's only one
		 * '.' and only '.' points to itself because bogus entries
		 * got trashed in phase 3 if there were > 1.
		 * bump up link count for '.' but don't set reached
		 * until we're actually reached by another directory
		 * '..' is already accounted for or will be taken care
		 * of when directory is moved to orphanage.
		 */
		if (ip->i_ino == inum)  {
			ASSERT(dep->name[0] == '.' && dep->namelen == 1);
			add_inode_ref(current_irec, current_ino_offset);
			if (da_bno != 0 || dep != (xfs_dir2_data_entry_t *)d->u) {
				/* "." should be the first entry */
				nbad++;
				if (entry_junked(_("entry \"%s\" in dir %llu is "
						"not the first entry"),
						fname, inum, ip->i_ino)) {
					dep->name[0] = '/';
					libxfs_dir2_data_log_entry(tp, bp, dep);
				}
			}
			*need_dot = 0;
			continue;
		}
		/*
		 * skip entries with bogus inumbers if we're in no modify mode
		 */
		if (no_modify && verify_inum(mp, inum))
			continue;
		/*
		 * check easy case first, regular inode, just bump
		 * the link count and continue
		 */
		if (!inode_isadir(irec, ino_offset))  {
			add_inode_reached(irec, ino_offset);
			continue;
		}
		parent = get_inode_parent(irec, ino_offset);
		ASSERT(parent != 0);
		junkit = 0;
		/*
		 * bump up the link counts in parent and child
		 * directory but if the link doesn't agree with
		 * the .. in the child, blow out the entry.
		 * if the directory has already been reached,
		 * blow away the entry also.
		 */
		if (is_inode_reached(irec, ino_offset))  {
			junkit = 1;
			do_warn(
_("entry \"%s\" in dir %llu points to an already connected directory inode %llu\n"),
				fname, ip->i_ino, inum);
		} else if (parent == ip->i_ino)  {
			add_inode_reached(irec, ino_offset);
			add_inode_ref(current_irec, current_ino_offset);
		} else if (parent == NULLFSINO) {
			/* ".." was missing, but this entry refers to it,
			   so, set it as the parent and mark for rebuild */
			do_warn(_("entry \"%s\" in dir ino %llu doesn't have a"
				" .. entry, will set it in ino %llu.\n"),
				fname, ip->i_ino, inum);
			set_inode_parent(irec, ino_offset, ip->i_ino);
			add_inode_reached(irec, ino_offset);
			add_inode_ref(current_irec, current_ino_offset);
		} else  {
			junkit = 1;
			do_warn(
_("entry \"%s\" in dir inode %llu inconsistent with .. value (%llu) in ino %llu\n"),
				fname, ip->i_ino, parent, inum);
		}
		if (junkit)  {
			if (inum == orphanage_ino)
				orphanage_ino = 0;
			junkit = 0;
			nbad++;
			if (!no_modify)  {
				dep->name[0] = '/';
				libxfs_dir2_data_log_entry(tp, bp, dep);
				if (verbose)
					do_warn(
					_("\twill clear entry \"%s\"\n"),
						fname);
			} else  {
				do_warn(_("\twould clear entry \"%s\"\n"),
					fname);
			}
		}
	}
	*num_illegal += nbad;
	if (needscan)
		libxfs_dir2_data_freescan(mp, d, &needlog, NULL);
	if (needlog)
		libxfs_dir2_data_log_header(tp, bp);
	libxfs_bmap_finish(&tp, &flist, firstblock, &committed);
	libxfs_trans_commit(tp, 0, 0);
	freetab->ents[db].v = INT_GET(d->hdr.bestfree[0].length, ARCH_CONVERT);
	freetab->ents[db].s = 0;
}

/*
 * Check contents of leaf-form block.
 */
static int
longform_dir2_check_leaf(
	xfs_mount_t		*mp,
	xfs_inode_t		*ip,
	dir_hash_tab_t		*hashtab,
	freetab_t		*freetab)
{
	int			badtail;
	xfs_dir2_data_off_t	*bestsp;
	xfs_dabuf_t		*bp;
	xfs_dablk_t		da_bno;
	int			i;
	xfs_dir2_leaf_t		*leaf;
	xfs_dir2_leaf_tail_t	*ltp;
	int			seeval;

	da_bno = mp->m_dirleafblk;
	if (libxfs_da_read_bufr(NULL, ip, da_bno, -1, &bp, XFS_DATA_FORK)) {
		do_error(_("can't read block %u for directory inode %llu\n"),
			da_bno, ip->i_ino);
		/* NOTREACHED */
	}
	leaf = bp->data;
	ltp = XFS_DIR2_LEAF_TAIL_P(mp, leaf);
	bestsp = XFS_DIR2_LEAF_BESTS_P(ltp);
	if (INT_GET(leaf->hdr.info.magic, ARCH_CONVERT) !=
			XFS_DIR2_LEAF1_MAGIC ||
	    INT_GET(leaf->hdr.info.forw, ARCH_CONVERT) ||
			INT_GET(leaf->hdr.info.back, ARCH_CONVERT) ||
	    INT_GET(leaf->hdr.count, ARCH_CONVERT) <
			INT_GET(leaf->hdr.stale, ARCH_CONVERT) ||
	    INT_GET(leaf->hdr.count, ARCH_CONVERT) >
			XFS_DIR2_MAX_LEAF_ENTS(mp) ||
	    (char *)&leaf->ents[INT_GET(leaf->hdr.count, ARCH_CONVERT)] >
			(char *)bestsp) {
		do_warn(
		_("leaf block %u for directory inode %llu bad header\n"),
			da_bno, ip->i_ino);
		libxfs_da_brelse(NULL, bp);
		return 1;
	}
	seeval = dir_hash_see_all(hashtab, leaf->ents,
				INT_GET(leaf->hdr.count, ARCH_CONVERT),
		INT_GET(leaf->hdr.stale, ARCH_CONVERT));
	if (dir_hash_check(hashtab, ip, seeval)) {
		libxfs_da_brelse(NULL, bp);
		return 1;
	}
	badtail = freetab->nents != INT_GET(ltp->bestcount, ARCH_CONVERT);
	for (i = 0; !badtail && i < INT_GET(ltp->bestcount, ARCH_CONVERT); i++) {
		freetab->ents[i].s = 1;
		badtail = freetab->ents[i].v != INT_GET(bestsp[i], ARCH_CONVERT);
	}
	if (badtail) {
		do_warn(_("leaf block %u for directory inode %llu bad tail\n"),
			da_bno, ip->i_ino);
		libxfs_da_brelse(NULL, bp);
		return 1;
	}
	libxfs_da_brelse(NULL, bp);
	return 0;
}

/*
 * Check contents of the node blocks (leaves)
 * Looks for matching hash values for the data entries.
 */
static int
longform_dir2_check_node(
	xfs_mount_t		*mp,
	xfs_inode_t		*ip,
	dir_hash_tab_t		*hashtab,
	freetab_t		*freetab)
{
	xfs_dabuf_t		*bp;
	xfs_dablk_t		da_bno;
	xfs_dir2_db_t		fdb;
	xfs_dir2_free_t		*free;
	int			i;
	xfs_dir2_leaf_t		*leaf;
	xfs_fileoff_t		next_da_bno;
	int			seeval = 0;
	int			used;

	for (da_bno = mp->m_dirleafblk, next_da_bno = 0;
	     next_da_bno != NULLFILEOFF && da_bno < mp->m_dirfreeblk;
	     da_bno = (xfs_dablk_t)next_da_bno) {
		next_da_bno = da_bno + mp->m_dirblkfsbs - 1;
		if (libxfs_bmap_next_offset(NULL, ip, &next_da_bno, XFS_DATA_FORK))
			break;
		if (libxfs_da_read_bufr(NULL, ip, da_bno, -1, &bp,
				XFS_DATA_FORK)) {
			do_warn(
			_("can't read leaf block %u for directory inode %llu\n"),
				da_bno, ip->i_ino);
			return 1;
		}
		leaf = bp->data;
		if (INT_GET(leaf->hdr.info.magic, ARCH_CONVERT) !=
		    XFS_DIR2_LEAFN_MAGIC) {
			if (INT_GET(leaf->hdr.info.magic, ARCH_CONVERT) ==
			    XFS_DA_NODE_MAGIC) {
				libxfs_da_brelse(NULL, bp);
				continue;
			}
			do_warn(_("unknown magic number %#x for block %u in "
				  "directory inode %llu\n"),
				INT_GET(leaf->hdr.info.magic, ARCH_CONVERT),
				da_bno, ip->i_ino);
			libxfs_da_brelse(NULL, bp);
			return 1;
		}
		if (INT_GET(leaf->hdr.count, ARCH_CONVERT) <
				INT_GET(leaf->hdr.stale, ARCH_CONVERT) ||
		    INT_GET(leaf->hdr.count, ARCH_CONVERT) >
				XFS_DIR2_MAX_LEAF_ENTS(mp)) {
			do_warn(_("leaf block %u for directory inode %llu bad "
				  "header\n"),
				da_bno, ip->i_ino);
			libxfs_da_brelse(NULL, bp);
			return 1;
		}
		seeval = dir_hash_see_all(hashtab, leaf->ents, INT_GET(leaf->hdr.count, ARCH_CONVERT),
			INT_GET(leaf->hdr.stale, ARCH_CONVERT));
		libxfs_da_brelse(NULL, bp);
		if (seeval != DIR_HASH_CK_OK)
			return 1;
	}
	if (dir_hash_check(hashtab, ip, seeval))
		return 1;

	for (da_bno = mp->m_dirfreeblk, next_da_bno = 0;
	     next_da_bno != NULLFILEOFF;
	     da_bno = (xfs_dablk_t)next_da_bno) {
		next_da_bno = da_bno + mp->m_dirblkfsbs - 1;
		if (libxfs_bmap_next_offset(NULL, ip, &next_da_bno, XFS_DATA_FORK))
			break;
		if (libxfs_da_read_bufr(NULL, ip, da_bno, -1, &bp,
				XFS_DATA_FORK)) {
			do_warn(
		_("can't read freespace block %u for directory inode %llu\n"),
				da_bno, ip->i_ino);
			return 1;
		}
		free = bp->data;
		fdb = XFS_DIR2_DA_TO_DB(mp, da_bno);
		if (INT_GET(free->hdr.magic, ARCH_CONVERT) !=
				XFS_DIR2_FREE_MAGIC ||
		    INT_GET(free->hdr.firstdb, ARCH_CONVERT) !=
				(fdb - XFS_DIR2_FREE_FIRSTDB(mp)) *
				XFS_DIR2_MAX_FREE_BESTS(mp) ||
		    INT_GET(free->hdr.nvalid, ARCH_CONVERT) <
				INT_GET(free->hdr.nused, ARCH_CONVERT)) {
			do_warn(_("free block %u for directory inode %llu bad "
				  "header\n"),
				da_bno, ip->i_ino);
			libxfs_da_brelse(NULL, bp);
			return 1;
		}
		for (i = used = 0; i < INT_GET(free->hdr.nvalid, ARCH_CONVERT); i++) {
			if (i + INT_GET(free->hdr.firstdb, ARCH_CONVERT) >=
					freetab->nents ||
			    freetab->ents[i + INT_GET(free->hdr.firstdb,
					ARCH_CONVERT)].v !=
			    INT_GET(free->bests[i], ARCH_CONVERT)) {
				do_warn(
		_("free block %u entry %i for directory ino %llu bad\n"),
					da_bno, i, ip->i_ino);
				libxfs_da_brelse(NULL, bp);
				return 1;
			}
			used += INT_GET(free->bests[i], ARCH_CONVERT) != NULLDATAOFF;
			freetab->ents[i + INT_GET(free->hdr.firstdb, ARCH_CONVERT)].s = 1;
		}
		if (used != INT_GET(free->hdr.nused, ARCH_CONVERT)) {
			do_warn(_("free block %u for directory inode %llu bad "
				  "nused\n"),
				da_bno, ip->i_ino);
			libxfs_da_brelse(NULL, bp);
			return 1;
		}
		libxfs_da_brelse(NULL, bp);
	}
	for (i = 0; i < freetab->nents; i++) {
		if ((freetab->ents[i].s == 0) &&
		    (freetab->ents[i].v != NULLDATAOFF)) {
			do_warn(_("missing freetab entry %u for "
				  "directory inode %llu\n"),
				i, ip->i_ino);
			return 1;
		}
	}
	return 0;
}

/*
 * If a directory is corrupt, we need to read in as many entries as possible,
 * destroy the entry and create a new one with recovered name/inode pairs.
 * (ie. get libxfs to do all the grunt work)
 */
static void
longform_dir2_entry_check(xfs_mount_t	*mp,
			xfs_ino_t	ino,
			xfs_inode_t	*ip,
			int		*num_illegal,
			int		*need_dot,
			ino_tree_node_t	*irec,
			int		ino_offset,
			dir_hash_tab_t	*hashtab)
{
	xfs_dir2_block_t	*block;
	xfs_dir2_leaf_entry_t	*blp;
	xfs_dabuf_t		**bplist;
	xfs_dir2_block_tail_t	*btp;
	xfs_dablk_t		da_bno;
	freetab_t		*freetab;
	int			num_bps;
	int			i;
	int			isblock;
	int			isleaf;
	xfs_fileoff_t		next_da_bno;
	int			seeval;
	int			fixit;
	xfs_dir2_db_t		db;

	*need_dot = 1;
	freetab = malloc(FREETAB_SIZE(ip->i_d.di_size / mp->m_dirblksize));
	if (!freetab) {
		do_error(
		_("malloc failed in longform_dir2_entry_check (%u bytes)\n"),
			FREETAB_SIZE(ip->i_d.di_size / mp->m_dirblksize));
		exit(1);
	}
	freetab->naents = ip->i_d.di_size / mp->m_dirblksize;
	freetab->nents = 0;
	for (i = 0; i < freetab->naents; i++) {
		freetab->ents[i].v = NULLDATAOFF;
		freetab->ents[i].s = 0;
	}
	num_bps = freetab->naents;
	bplist = calloc(num_bps, sizeof(xfs_dabuf_t*));
	/* is this a block, leaf, or node directory? */
	libxfs_dir2_isblock(NULL, ip, &isblock);
	libxfs_dir2_isleaf(NULL, ip, &isleaf);

	/* check directory "data" blocks (ie. name/inode pairs) */
	for (da_bno = 0, next_da_bno = 0;
	     next_da_bno != NULLFILEOFF && da_bno < mp->m_dirleafblk;
	     da_bno = (xfs_dablk_t)next_da_bno) {
		next_da_bno = da_bno + mp->m_dirblkfsbs - 1;
		if (libxfs_bmap_next_offset(NULL, ip, &next_da_bno, XFS_DATA_FORK))
			break;
		db = XFS_DIR2_DA_TO_DB(mp, da_bno);
		if (db >= num_bps) {
			/* more data blocks than expected */
			num_bps = db + 1;
			bplist = realloc(bplist, num_bps * sizeof(xfs_dabuf_t*));
			if (!bplist)
				do_error(
		_("realloc failed in longform_dir2_entry_check (%u bytes)\n"),
					num_bps * sizeof(xfs_dabuf_t*));
		}
		if (libxfs_da_read_bufr(NULL, ip, da_bno, -1, &bplist[db],
				XFS_DATA_FORK)) {
			do_warn(_(
			"can't read data block %u for directory inode %llu\n"),
				da_bno, ino);
			*num_illegal += 1;
			continue;	/* try and read all "data" blocks */
		}
		longform_dir2_entry_check_data(mp, ip, num_illegal, need_dot,
				irec, ino_offset, &bplist[db], hashtab,
				&freetab, da_bno, isblock);
	}
	fixit = (*num_illegal != 0) || dir2_is_badino(ino) || *need_dot;

	/* check btree and freespace */
	if (isblock) {
		block = bplist[0]->data;
		btp = XFS_DIR2_BLOCK_TAIL_P(mp, block);
		blp = XFS_DIR2_BLOCK_LEAF_P(btp);
		seeval = dir_hash_see_all(hashtab, blp,
				INT_GET(btp->count, ARCH_CONVERT),
				INT_GET(btp->stale, ARCH_CONVERT));
		if (dir_hash_check(hashtab, ip, seeval))
			fixit |= 1;
	} else if (isleaf) {
		fixit |= longform_dir2_check_leaf(mp, ip, hashtab, freetab);
	} else {
		fixit |= longform_dir2_check_node(mp, ip, hashtab, freetab);
	}
	if (!no_modify && fixit) {
		dir_hash_dup_names(hashtab);
		for (i = 0; i < freetab->naents; i++)
			if (bplist[i])
				libxfs_da_brelse(NULL, bplist[i]);
		longform_dir2_rebuild(mp, ino, ip, irec, ino_offset, hashtab);
		*num_illegal = 0;
		*need_dot = 0;
	} else {
		for (i = 0; i < freetab->naents; i++)
			if (bplist[i])
				libxfs_da_brelse(NULL, bplist[i]);
	}

	free(bplist);
	free(freetab);
}

/*
 * shortform directory processing routines -- entry verification and
 * bad entry deletion (pruning).
 */
static void
shortform_dir_entry_check(xfs_mount_t	*mp,
			xfs_ino_t	ino,
			xfs_inode_t	*ip,
			int		*ino_dirty,
			ino_tree_node_t	*current_irec,
			int		current_ino_offset,
			dir_hash_tab_t	*hashtab)
{
	xfs_ino_t		lino;
	xfs_ino_t		parent;
	xfs_dir_shortform_t	*sf;
	xfs_dir_sf_entry_t	*sf_entry, *next_sfe, *tmp_sfe;
	xfs_ifork_t		*ifp;
	ino_tree_node_t		*irec;
	int			max_size;
	int			ino_offset;
	int			i;
	int			junkit;
	int			tmp_len;
	int			tmp_elen;
	int			bad_sfnamelen;
	int			namelen;
	int			bytes_deleted;
	char			fname[MAXNAMELEN + 1];

	ifp = &ip->i_df;
	sf = (xfs_dir_shortform_t *) ifp->if_u1.if_data;
	*ino_dirty = 0;
	bytes_deleted = 0;

	max_size = ifp->if_bytes;
	ASSERT(ip->i_d.di_size <= ifp->if_bytes);

	/*
	 * no '.' entry in shortform dirs, just bump up ref count by 1
	 * '..' was already (or will be) accounted for and checked when
	 * the directory is reached or will be taken care of when the
	 * directory is moved to orphanage.
	 */
	add_inode_ref(current_irec, current_ino_offset);

	/*
	 * now run through entries, stop at first bad entry, don't need
	 * to skip over '..' since that's encoded in its own field and
	 * no need to worry about '.' since it doesn't exist.
	 */
	sf_entry = next_sfe = &sf->list[0];
	if (sf == NULL) {
		junkit = 1;
		do_warn(_("shortform dir inode %llu has null data entries \n"),
			ino);

		}
	else {
	   for (i = 0; i < INT_GET(sf->hdr.count, ARCH_CONVERT) && max_size >
					(__psint_t)next_sfe - (__psint_t)sf;
			sf_entry = next_sfe, i++)  {
		junkit = 0;
		bad_sfnamelen = 0;
		tmp_sfe = NULL;

		XFS_DIR_SF_GET_DIRINO(&sf_entry->inumber, &lino);

		namelen = sf_entry->namelen;

		ASSERT(no_modify || namelen > 0);

		if (no_modify && namelen == 0)  {
			/*
			 * if we're really lucky, this is
			 * the last entry in which case we
			 * can use the dir size to set the
			 * namelen value.  otherwise, forget
			 * it because we're not going to be
			 * able to find the next entry.
			 */
			bad_sfnamelen = 1;

			if (i == INT_GET(sf->hdr.count, ARCH_CONVERT) - 1)  {
				namelen = ip->i_d.di_size -
					((__psint_t) &sf_entry->name[0] -
					 (__psint_t) sf);
			} else  {
				/*
				 * don't process the rest of the directory,
				 * break out of processing looop
				 */
				break;
			}
		} else if (no_modify && (__psint_t) sf_entry - (__psint_t) sf +
				+ XFS_DIR_SF_ENTSIZE_BYENTRY(sf_entry)
				> ip->i_d.di_size)  {
			bad_sfnamelen = 1;

			if (i == INT_GET(sf->hdr.count, ARCH_CONVERT) - 1)  {
				namelen = ip->i_d.di_size -
					((__psint_t) &sf_entry->name[0] -
					 (__psint_t) sf);
			} else  {
				/*
				 * don't process the rest of the directory,
				 * break out of processing looop
				 */
				break;
			}
		}

		memmove(fname, sf_entry->name, sf_entry->namelen);
		fname[sf_entry->namelen] = '\0';

		ASSERT(no_modify || lino != NULLFSINO);
		ASSERT(no_modify || !verify_inum(mp, lino));

		irec = find_inode_rec(XFS_INO_TO_AGNO(mp, lino),
					XFS_INO_TO_AGINO(mp, lino));
		if (irec == NULL) {
			do_warn(_("entry \"%s\" in shortform dir %llu "
				"references non-existent ino %llu"),
				fname, ino, lino);
			goto do_junkit;
		}
		ino_offset = XFS_INO_TO_AGINO(mp, lino) - irec->ino_startnum;

		/*
		 * if it's a free inode, blow out the entry.
		 * by now, any inode that we think is free
		 * really is free.
		 */
		if (!is_inode_free(irec, ino_offset))  {
			do_warn(_("entry \"%s\" in shortform dir inode %llu "
				"points to free inode %llu"), fname, ino, lino);
			goto do_junkit;
		}
		/*
		 * check if this inode is lost+found dir in the root
		 */
		if (ino == mp->m_sb.sb_rootino && strcmp(fname, ORPHANAGE) == 0) {
			/*
			 * if it's not a directory, trash it
			 */
			if (!inode_isadir(irec, ino_offset)) {
				do_warn(_("%s (ino %llu) in root (%llu) is not "
					"a directory"), ORPHANAGE, lino, ino);
				goto do_junkit;
			}
			/*
			 * if this is a dup, it will be picked up below,
			 * otherwise, mark it as the orphanage for later.
			 */
			if (!orphanage_ino)
				orphanage_ino = lino;
		}
		/*
		 * check for duplicate names in directory.
		 */
		if (!dir_hash_add(hashtab,
				(xfs_dir2_dataptr_t)(sf_entry - &sf->list[0]),
				lino, sf_entry->namelen, sf_entry->name)) {
			do_warn(_("entry \"%s\" (ino %llu) in dir %llu is a "
				"duplicate name"), fname, lino, ino);
			goto do_junkit;
		}
		if (!inode_isadir(irec, ino_offset))  {
			/*
			 * check easy case first, regular inode, just bump
			 * the link count and continue
			 */
			add_inode_reached(irec, ino_offset);

			next_sfe = (xfs_dir_sf_entry_t *)
				((__psint_t) sf_entry +
				XFS_DIR_SF_ENTSIZE_BYENTRY(sf_entry));
			continue;
		} else  {
			parent = get_inode_parent(irec, ino_offset);

			/*
			 * bump up the link counts in parent and child.
			 * directory but if the link doesn't agree with
			 * the .. in the child, blow out the entry
			 */
			if (is_inode_reached(irec, ino_offset))  {
				junkit = 1;
				do_warn(_("entry \"%s\" in dir %llu references "
					"already connected dir ino %llu,\n"),
					fname, ino, lino);
			} else if (parent == ino)  {
				add_inode_reached(irec, ino_offset);
				add_inode_ref(current_irec, current_ino_offset);
			} else if (parent == NULLFSINO) {
				/* ".." was missing, but this entry refers to it,
				so, set it as the parent and mark for rebuild */
				do_warn(_("entry \"%s\" in dir ino %llu doesn't have a"
					" .. entry, will set it in ino %llu.\n"),
					fname, ino, lino);
				set_inode_parent(irec, ino_offset, ino);
				add_inode_reached(irec, ino_offset);
				add_inode_ref(current_irec, current_ino_offset);
			} else  {
				junkit = 1;
				do_warn(_("entry \"%s\" in dir %llu not "
					"consistent with .. value (%llu) in "
					"dir ino %llu"),
					fname, ino, parent, lino);
			}
		}
		if (junkit)  {
do_junkit:
			if (lino == orphanage_ino)
				orphanage_ino = 0;
			if (!no_modify)  {
				tmp_elen = XFS_DIR_SF_ENTSIZE_BYENTRY(sf_entry);
				tmp_sfe = (xfs_dir_sf_entry_t *)
					((__psint_t) sf_entry + tmp_elen);
				tmp_len = max_size - ((__psint_t) tmp_sfe
							- (__psint_t) sf);
				max_size -= tmp_elen;
				bytes_deleted += tmp_elen;

				memmove(sf_entry, tmp_sfe, tmp_len);

				INT_MOD(sf->hdr.count, ARCH_CONVERT, -1);
				memset((void *) ((__psint_t) sf_entry + tmp_len), 0,
						tmp_elen);

				/*
				 * set the tmp value to the current
				 * pointer so we'll process the entry
				 * we just moved up
				 */
				tmp_sfe = sf_entry;

				/*
				 * WARNING:  drop the index i by one
				 * so it matches the decremented count for
				 * accurate comparisons in the loop test
				 */
				i--;

				*ino_dirty = 1;

				if (verbose)
					do_warn(_("junking entry\n"));
				else
					do_warn("\n");
			} else  {
				do_warn(_("would junk entry\n"), fname);
			}
		}

		/*
		 * go onto next entry unless we've just junked an
		 * entry in which the current entry pointer points
		 * to an unprocessed entry.  have to take into entries
		 * with bad namelen into account in no modify mode since we
		 * calculate size based on next_sfe.
		 */
		ASSERT(no_modify || bad_sfnamelen == 0);

		next_sfe = (tmp_sfe == NULL)
			? (xfs_dir_sf_entry_t *) ((__psint_t) sf_entry
				+ ((!bad_sfnamelen)
					? XFS_DIR_SF_ENTSIZE_BYENTRY(sf_entry)
					: sizeof(xfs_dir_sf_entry_t) - 1
						+ namelen))
			: tmp_sfe;
	    }
	}

	/*
	 * sync up sizes if required
	 */
	if (*ino_dirty)  {
		ASSERT(bytes_deleted > 0);
		ASSERT(!no_modify);
		libxfs_idata_realloc(ip, -bytes_deleted, XFS_DATA_FORK);
		ip->i_d.di_size -= bytes_deleted;
	}

	if (ip->i_d.di_size != ip->i_df.if_bytes)  {
		ASSERT(ip->i_df.if_bytes == (xfs_fsize_t)
				((__psint_t) next_sfe - (__psint_t) sf));
		ip->i_d.di_size = (xfs_fsize_t)
				((__psint_t) next_sfe - (__psint_t) sf);
		do_warn(
		_("setting size to %lld bytes to reflect junked entries\n"),
				ip->i_d.di_size);
		*ino_dirty = 1;
	}
}

/* ARGSUSED */
static void
prune_sf_dir_entry(xfs_mount_t *mp, xfs_ino_t ino, xfs_inode_t *ip)
{
				/* REFERENCED */
	xfs_ino_t		lino;
	xfs_dir_shortform_t	*sf;
	xfs_dir_sf_entry_t	*sf_entry, *next_sfe, *tmp_sfe;
	xfs_ifork_t		*ifp;
	int			max_size;
	int			i;
	int			tmp_len;
	int			tmp_elen;
	int			bytes_deleted;
	char			fname[MAXNAMELEN + 1];

	ifp = &ip->i_df;
	sf = (xfs_dir_shortform_t *) ifp->if_u1.if_data;
	bytes_deleted = 0;

	max_size = ifp->if_bytes;
	ASSERT(ip->i_d.di_size <= ifp->if_bytes);

	/*
	 * now run through entries and delete every bad entry
	 */
	sf_entry = next_sfe = &sf->list[0];

	for (i = 0; i < INT_GET(sf->hdr.count, ARCH_CONVERT) && max_size >
					(__psint_t)next_sfe - (__psint_t)sf;
			sf_entry = next_sfe, i++)  {
		tmp_sfe = NULL;

		XFS_DIR_SF_GET_DIRINO(&sf_entry->inumber, &lino);

		memmove(fname, sf_entry->name, sf_entry->namelen);
		fname[sf_entry->namelen] = '\0';

		if (sf_entry->name[0] == '/')  {
			if (!no_modify)  {
				tmp_elen = XFS_DIR_SF_ENTSIZE_BYENTRY(sf_entry);
				tmp_sfe = (xfs_dir_sf_entry_t *)
					((__psint_t) sf_entry + tmp_elen);
				tmp_len = max_size - ((__psint_t) tmp_sfe
							- (__psint_t) sf);
				max_size -= tmp_elen;
				bytes_deleted += tmp_elen;

				memmove(sf_entry, tmp_sfe, tmp_len);

				INT_MOD(sf->hdr.count, ARCH_CONVERT, -1);
				memset((void *) ((__psint_t) sf_entry + tmp_len), 0,
						tmp_elen);

				/*
				 * set the tmp value to the current
				 * pointer so we'll process the entry
				 * we just moved up
				 */
				tmp_sfe = sf_entry;

				/*
				 * WARNING:  drop the index i by one
				 * so it matches the decremented count for
				 * accurate comparisons in the loop test
				 */
				i--;
			}
		}
		next_sfe = (tmp_sfe == NULL)
			? (xfs_dir_sf_entry_t *) ((__psint_t) sf_entry +
				XFS_DIR_SF_ENTSIZE_BYENTRY(sf_entry))
			: tmp_sfe;
	}

	/*
	 * sync up sizes if required
	 */
	if (bytes_deleted > 0)  {
		libxfs_idata_realloc(ip, -bytes_deleted, XFS_DATA_FORK);
		ip->i_d.di_size -= bytes_deleted;
	}

	if (ip->i_d.di_size != ip->i_df.if_bytes)  {
		ASSERT(ip->i_df.if_bytes == (xfs_fsize_t)
				((__psint_t) next_sfe - (__psint_t) sf));
		ip->i_d.di_size = (xfs_fsize_t)
				((__psint_t) next_sfe - (__psint_t) sf);
		do_warn(
		_("setting size to %lld bytes to reflect junked entries\n"),
				ip->i_d.di_size);
	}
}

/*
 * shortform directory v2 processing routines -- entry verification and
 * bad entry deletion (pruning).
 */
static void
shortform_dir2_entry_check(xfs_mount_t	*mp,
			xfs_ino_t	ino,
			xfs_inode_t	*ip,
			int		*ino_dirty,
			ino_tree_node_t	*current_irec,
			int		current_ino_offset,
			dir_hash_tab_t	*hashtab)
{
	xfs_ino_t		lino;
	xfs_ino_t		parent;
	xfs_dir2_sf_t		*sfp;
	xfs_dir2_sf_entry_t	*sfep, *next_sfep, *tmp_sfep;
	xfs_ifork_t		*ifp;
	ino_tree_node_t		*irec;
	int			max_size;
	int			ino_offset;
	int			i;
	int			junkit;
	int			tmp_len;
	int			tmp_elen;
	int			bad_sfnamelen;
	int			namelen;
	int			bytes_deleted;
	char			fname[MAXNAMELEN + 1];
	int			i8;

	ifp = &ip->i_df;
	sfp = (xfs_dir2_sf_t *) ifp->if_u1.if_data;
	*ino_dirty = 0;
	bytes_deleted = 0;

	max_size = ifp->if_bytes;
	ASSERT(ip->i_d.di_size <= ifp->if_bytes);

	/*
	 * no '.' entry in shortform dirs, just bump up ref count by 1
	 * '..' was already (or will be) accounted for and checked when
	 * the directory is reached or will be taken care of when the
	 * directory is moved to orphanage.
	 */
	add_inode_ref(current_irec, current_ino_offset);

	/*
	 * Initialise i8 counter -- the parent inode number counts as well.
	 */
	i8 = (XFS_DIR2_SF_GET_INUMBER(sfp, &sfp->hdr.parent) > XFS_DIR2_MAX_SHORT_INUM);

	/*
	 * now run through entries, stop at first bad entry, don't need
	 * to skip over '..' since that's encoded in its own field and
	 * no need to worry about '.' since it doesn't exist.
	 */
	sfep = next_sfep = XFS_DIR2_SF_FIRSTENTRY(sfp);

	for (i = 0; i < INT_GET(sfp->hdr.count, ARCH_CONVERT) && max_size >
					(__psint_t)next_sfep - (__psint_t)sfp;
			sfep = next_sfep, i++)  {
		junkit = 0;
		bad_sfnamelen = 0;
		tmp_sfep = NULL;

		lino = XFS_DIR2_SF_GET_INUMBER(sfp, XFS_DIR2_SF_INUMBERP(sfep));

		namelen = sfep->namelen;

		ASSERT(no_modify || namelen > 0);

		if (no_modify && namelen == 0)  {
			/*
			 * if we're really lucky, this is
			 * the last entry in which case we
			 * can use the dir size to set the
			 * namelen value.  otherwise, forget
			 * it because we're not going to be
			 * able to find the next entry.
			 */
			bad_sfnamelen = 1;

			if (i == INT_GET(sfp->hdr.count, ARCH_CONVERT) - 1)  {
				namelen = ip->i_d.di_size -
					((__psint_t) &sfep->name[0] -
					 (__psint_t) sfp);
			} else  {
				/*
				 * don't process the rest of the directory,
				 * break out of processing loop
				 */
				break;
			}
		} else if (no_modify && (__psint_t) sfep - (__psint_t) sfp +
				+ XFS_DIR2_SF_ENTSIZE_BYENTRY(sfp, sfep)
				> ip->i_d.di_size)  {
			bad_sfnamelen = 1;

			if (i == INT_GET(sfp->hdr.count, ARCH_CONVERT) - 1)  {
				namelen = ip->i_d.di_size -
					((__psint_t) &sfep->name[0] -
					 (__psint_t) sfp);
			} else  {
				/*
				 * don't process the rest of the directory,
				 * break out of processing loop
				 */
				break;
			}
		}

		memmove(fname, sfep->name, sfep->namelen);
		fname[sfep->namelen] = '\0';

		ASSERT(no_modify || (lino != NULLFSINO && lino != 0));
		ASSERT(no_modify || !verify_inum(mp, lino));

		/*
		 * Also skip entries with bogus inode numbers if we're
		 * in no modify mode.
		 */

		if (no_modify && verify_inum(mp, lino))  {
			next_sfep = (xfs_dir2_sf_entry_t *)
				((__psint_t) sfep +
				XFS_DIR2_SF_ENTSIZE_BYENTRY(sfp, sfep));
			continue;
		}

		irec = find_inode_rec(XFS_INO_TO_AGNO(mp, lino),
					XFS_INO_TO_AGINO(mp, lino));

		if (irec == NULL)  {
			do_warn(_("entry \"%s\" in shortform directory %llu "
				  "references non-existent inode %llu"),
				fname, ino, lino);
			goto do_junkit;
		}

		ino_offset = XFS_INO_TO_AGINO(mp, lino) - irec->ino_startnum;

		/*
		 * if it's a free inode, blow out the entry.
		 * by now, any inode that we think is free
		 * really is free.
		 */
		if (is_inode_free(irec, ino_offset))  {
			do_warn(_("entry \"%s\" in shortform directory "
				  "inode %llu points to free inode %llu"),
				fname, ino, lino);
			goto do_junkit;
		}
		/*
		 * check if this inode is lost+found dir in the root
		 */
		if (ino == mp->m_sb.sb_rootino && strcmp(fname, ORPHANAGE) == 0) {
			/*
			 * if it's not a directory, trash it
			 */
			if (!inode_isadir(irec, ino_offset)) {
				do_warn(_("%s (ino %llu) in root (%llu) is not "
					"a directory"), ORPHANAGE, lino, ino);
				goto do_junkit;
			}
			/*
			 * if this is a dup, it will be picked up below,
			 * otherwise, mark it as the orphanage for later.
			 */
			if (!orphanage_ino)
				orphanage_ino = lino;
		}
		/*
		 * check for duplicate names in directory.
		 */
		if (!dir_hash_add(hashtab, (xfs_dir2_dataptr_t)
					(sfep - XFS_DIR2_SF_FIRSTENTRY(sfp)),
				lino, sfep->namelen, sfep->name)) {
			do_warn(_("entry \"%s\" (ino %llu) in dir %llu is a "
				"duplicate name"), fname, lino, ino);
			goto do_junkit;
		}
		if (!inode_isadir(irec, ino_offset))  {
			/*
			 * check easy case first, regular inode, just bump
			 * the link count
			 */
			add_inode_reached(irec, ino_offset);
		} else  {
			parent = get_inode_parent(irec, ino_offset);

			/*
			 * bump up the link counts in parent and child.
			 * directory but if the link doesn't agree with
			 * the .. in the child, blow out the entry
			 */
			if (is_inode_reached(irec, ino_offset))  {
				junkit = 1;
				do_warn(_("entry \"%s\" in directory inode %llu"
					  " references already connected inode "
					  "%llu,\n"),
					fname, ino, lino);
			} else if (parent == ino)  {
				add_inode_reached(irec, ino_offset);
				add_inode_ref(current_irec, current_ino_offset);
			} else if (parent == NULLFSINO) {
				/* ".." was missing, but this entry refers to it,
				so, set it as the parent and mark for rebuild */
				do_warn(_("entry \"%s\" in dir ino %llu doesn't have a"
					" .. entry, will set it in ino %llu.\n"),
					fname, ino, lino);
				set_inode_parent(irec, ino_offset, ino);
				add_inode_reached(irec, ino_offset);
				add_inode_ref(current_irec, current_ino_offset);
			} else  {
				junkit = 1;
				do_warn(_("entry \"%s\" in directory inode %llu"
					  " not consistent with .. value (%llu)"
					  " in inode %llu,\n"),
					fname, ino, parent, lino);
			}
		}

		if (junkit)  {
do_junkit:
			if (lino == orphanage_ino)
				orphanage_ino = 0;
			if (!no_modify)  {
				tmp_elen = XFS_DIR2_SF_ENTSIZE_BYENTRY(sfp, sfep);
				tmp_sfep = (xfs_dir2_sf_entry_t *)
					((__psint_t) sfep + tmp_elen);
				tmp_len = max_size - ((__psint_t) tmp_sfep
							- (__psint_t) sfp);
				max_size -= tmp_elen;
				bytes_deleted += tmp_elen;

				memmove(sfep, tmp_sfep, tmp_len);

				INT_MOD(sfp->hdr.count, ARCH_CONVERT, -1);
				memset((void *) ((__psint_t) sfep + tmp_len), 0,
						tmp_elen);

				/*
				 * set the tmp value to the current
				 * pointer so we'll process the entry
				 * we just moved up
				 */
				tmp_sfep = sfep;

				/*
				 * WARNING:  drop the index i by one
				 * so it matches the decremented count for
				 * accurate comparisons in the loop test
				 */
				i--;

				*ino_dirty = 1;

				if (verbose)
					do_warn(_("junking entry\n"));
				else
					do_warn("\n");
			} else  {
				do_warn(_("would junk entry\n"));
			}
		} else if (lino > XFS_DIR2_MAX_SHORT_INUM)
			i8++;

		/*
		 * go onto next entry unless we've just junked an
		 * entry in which the current entry pointer points
		 * to an unprocessed entry.  have to take into entries
		 * with bad namelen into account in no modify mode since we
		 * calculate size based on next_sfep.
		 */
		ASSERT(no_modify || bad_sfnamelen == 0);

		next_sfep = (tmp_sfep == NULL)
			? (xfs_dir2_sf_entry_t *) ((__psint_t) sfep
				+ ((!bad_sfnamelen)
					? XFS_DIR2_SF_ENTSIZE_BYENTRY(sfp, sfep)
					: XFS_DIR2_SF_ENTSIZE_BYNAME(sfp, namelen)))
			: tmp_sfep;
	}

	if (sfp->hdr.i8count != i8) {
		if (no_modify) {
			do_warn(_("would fix i8count in inode %llu\n"), ino);
		} else {
			if (i8 == 0) {
				tmp_sfep = next_sfep;
				process_sf_dir2_fixi8(sfp, &tmp_sfep);
				bytes_deleted +=
					(__psint_t)next_sfep -
					(__psint_t)tmp_sfep;
				next_sfep = tmp_sfep;
			} else
				sfp->hdr.i8count = i8;
			*ino_dirty = 1;
			do_warn(_("fixing i8count in inode %llu\n"), ino);
		}
	}

	/*
	 * sync up sizes if required
	 */
	if (*ino_dirty)  {
		ASSERT(bytes_deleted > 0);
		ASSERT(!no_modify);
		libxfs_idata_realloc(ip, -bytes_deleted, XFS_DATA_FORK);
		ip->i_d.di_size -= bytes_deleted;
	}

	if (ip->i_d.di_size != ip->i_df.if_bytes)  {
		ASSERT(ip->i_df.if_bytes == (xfs_fsize_t)
				((__psint_t) next_sfep - (__psint_t) sfp));
		ip->i_d.di_size = (xfs_fsize_t)
				((__psint_t) next_sfep - (__psint_t) sfp);
		do_warn(_("setting size to %lld bytes to reflect junked "
			  "entries\n"),
			ip->i_d.di_size);
		*ino_dirty = 1;
	}
}

/*
 * processes all reachable inodes in directories
 */
static void
process_dir_inode(
	xfs_mount_t 		*mp,
	xfs_ino_t		ino,
	ino_tree_node_t		*irec,
	int			ino_offset)
{
	xfs_bmap_free_t		flist;
	xfs_fsblock_t		first;
	xfs_inode_t		*ip;
	xfs_trans_t		*tp;
	xfs_dahash_t		hashval;
	dir_hash_tab_t		*hashtab;
	int			need_dot, committed;
	int			dirty, num_illegal, error, nres;

	/*
	 * open up directory inode, check all entries,
	 * then call prune_dir_entries to remove all
	 * remaining illegal directory entries.
	 */

	ASSERT(!is_inode_refchecked(ino, irec, ino_offset));

	error = libxfs_iget(mp, NULL, ino, 0, &ip, 0);
	if (error) {
		if (!no_modify)
			do_error(_("couldn't map inode %llu, err = %d\n"),
				ino, error);
		else  {
			do_warn(_("couldn't map inode %llu, err = %d\n"),
				ino, error);
			/*
			 * see below for what we're doing if this
			 * is root.  Why do we need to do this here?
			 * to ensure that the root doesn't show up
			 * as being disconnected in the no_modify case.
			 */
			if (mp->m_sb.sb_rootino == ino)  {
				add_inode_reached(irec, 0);
				add_inode_ref(irec, 0);
			}
		}

		add_inode_refchecked(ino, irec, 0);
		return;
	}

	need_dot = dirty = num_illegal = 0;

	if (mp->m_sb.sb_rootino == ino)  {
		/*
		 * mark root inode reached and bump up
		 * link count for root inode to account
		 * for '..' entry since the root inode is
		 * never reached by a parent.  we know
		 * that root's '..' is always good --
		 * guaranteed by phase 3 and/or below.
		 */
		add_inode_reached(irec, ino_offset);
	}

	add_inode_refchecked(ino, irec, ino_offset);

	hashtab = dir_hash_init(ip->i_d.di_size);

	/*
	 * look for bogus entries
	 */
	switch (ip->i_d.di_format)  {
		case XFS_DINODE_FMT_EXTENTS:
		case XFS_DINODE_FMT_BTREE:
			/*
			 * also check for missing '.' in longform dirs.
			 * missing .. entries are added if required when
			 * the directory is connected to lost+found. but
			 * we need to create '.' entries here.
			 */
			if (XFS_SB_VERSION_HASDIRV2(&mp->m_sb))
				longform_dir2_entry_check(mp, ino, ip,
							&num_illegal, &need_dot,
							irec, ino_offset,
							hashtab);
			else
				longform_dir_entry_check(mp, ino, ip,
							&num_illegal, &need_dot,
							irec, ino_offset,
							hashtab);
			break;

		case XFS_DINODE_FMT_LOCAL:
			tp = libxfs_trans_alloc(mp, 0);
			/*
			 * using the remove reservation is overkill
			 * since at most we'll only need to log the
			 * inode but it's easier than wedging a
			 * new define in ourselves.
			 */
			nres = no_modify ? 0 : XFS_REMOVE_SPACE_RES(mp);
			error = libxfs_trans_reserve(tp, nres,
					XFS_REMOVE_LOG_RES(mp), 0,
					XFS_TRANS_PERM_LOG_RES,
					XFS_REMOVE_LOG_COUNT);
			if (error)
				res_failed(error);

			libxfs_trans_ijoin(tp, ip, 0);
			libxfs_trans_ihold(tp, ip);

			if (XFS_SB_VERSION_HASDIRV2(&mp->m_sb))
				shortform_dir2_entry_check(mp, ino, ip, &dirty,
							irec, ino_offset,
							hashtab);
			else
				shortform_dir_entry_check(mp, ino, ip, &dirty,
							irec, ino_offset,
							hashtab);

			ASSERT(dirty == 0 || (dirty && !no_modify));
			if (dirty)  {
				libxfs_trans_log_inode(tp, ip,
					XFS_ILOG_CORE | XFS_ILOG_DDATA);
				libxfs_trans_commit(tp,
					XFS_TRANS_RELEASE_LOG_RES |
					XFS_TRANS_SYNC, 0);
			} else  {
				libxfs_trans_cancel(tp,
					XFS_TRANS_RELEASE_LOG_RES);
			}
			break;

		default:
			break;
	}
	dir_hash_done(hashtab);

	hashval = 0;

	/*
	 * if we have to create a .. for /, do it now *before*
	 * we delete the bogus entries, otherwise the directory
	 * could transform into a shortform dir which would
	 * probably cause the simulation to choke.  Even
	 * if the illegal entries get shifted around, it's ok
	 * because the entries are structurally intact and in
	 * in hash-value order so the simulation won't get confused
	 * if it has to move them around.
	 */
	if (!no_modify && need_root_dotdot && ino == mp->m_sb.sb_rootino)  {
		ASSERT(ip->i_d.di_format != XFS_DINODE_FMT_LOCAL);

		do_warn(_("recreating root directory .. entry\n"));

		tp = libxfs_trans_alloc(mp, 0);
		ASSERT(tp != NULL);

		nres = XFS_MKDIR_SPACE_RES(mp, 2);
		error = libxfs_trans_reserve(tp, nres, XFS_MKDIR_LOG_RES(mp),
				0, XFS_TRANS_PERM_LOG_RES, XFS_MKDIR_LOG_COUNT);
		if (error)
			res_failed(error);

		libxfs_trans_ijoin(tp, ip, 0);
		libxfs_trans_ihold(tp, ip);

		XFS_BMAP_INIT(&flist, &first);

		error = dir_createname(mp, tp, ip, "..", 2, ip->i_ino, &first,
				&flist, nres);
		if (error)
			do_error(_("can't make \"..\" entry in root inode "
				"%llu, createname error %d\n"), ino, error);

		libxfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);

		error = libxfs_bmap_finish(&tp, &flist, first, &committed);
		ASSERT(error == 0);
		libxfs_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES |
				XFS_TRANS_SYNC, 0);

		need_root_dotdot = 0;
	} else if (need_root_dotdot && ino == mp->m_sb.sb_rootino)  {
		do_warn(_("would recreate root directory .. entry\n"));
	}

	/*
	 * delete any illegal entries -- which should only exist
	 * if the directory is a longform directory.  bogus
	 * shortform directory entries were deleted in phase 4.
	 */
	if (!no_modify && num_illegal > 0)  {
		ASSERT(ip->i_d.di_format != XFS_DINODE_FMT_LOCAL);
		ASSERT(!XFS_SB_VERSION_HASDIRV2(&mp->m_sb));

		while (num_illegal > 0 && ip->i_d.di_format !=
				XFS_DINODE_FMT_LOCAL)  {
			prune_lf_dir_entry(mp, ino, ip, &hashval);
			num_illegal--;
		}

		/*
		* handle case where we've deleted so many
		* entries that the directory has changed from
		* a longform to a shortform directory.  have
		* to allocate a transaction since we're working
		* with the incore data fork.
		*/
		if (num_illegal > 0)  {
			ASSERT(ip->i_d.di_format == XFS_DINODE_FMT_LOCAL);
			tp = libxfs_trans_alloc(mp, 0);
			/*
			* using the remove reservation is overkill
			* since at most we'll only need to log the
			* inode but it's easier than wedging a
			* new define in ourselves.  10 block fs
			* space reservation is also overkill but
			* what the heck...
			*/
			nres = XFS_REMOVE_SPACE_RES(mp);
			error = libxfs_trans_reserve(tp, nres,
					XFS_REMOVE_LOG_RES(mp), 0,
					XFS_TRANS_PERM_LOG_RES,
					XFS_REMOVE_LOG_COUNT);
			if (error)
				res_failed(error);

			libxfs_trans_ijoin(tp, ip, 0);
			libxfs_trans_ihold(tp, ip);

			prune_sf_dir_entry(mp, ino, ip);

			libxfs_trans_log_inode(tp, ip,
					XFS_ILOG_CORE | XFS_ILOG_DDATA);
			ASSERT(error == 0);
			libxfs_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES
					|XFS_TRANS_SYNC, 0);
		}
	}

	/*
	 * if we need to create the '.' entry, do so only if
	 * the directory is a longform dir.  it it's been
	 * turned into a shortform dir, then the inode is ok
	 * since shortform dirs have no '.' entry and the inode
	 * has already been committed by prune_lf_dir_entry().
	 */
	if (need_dot)  {
		/*
		 * bump up our link count but don't
		 * bump up the inode link count.  chances
		 * are good that even though we lost '.'
		 * the inode link counts reflect '.' so
		 * leave the inode link count alone and if
		 * it turns out to be wrong, we'll catch
		 * that in phase 7.
		 */
		add_inode_ref(irec, ino_offset);

		if (no_modify)  {
			do_warn(_("would create missing \".\" entry in dir ino %llu\n"),
				ino);
		} else if (ip->i_d.di_format != XFS_DINODE_FMT_LOCAL)  {
			/*
			 * need to create . entry in longform dir.
			 */
			do_warn(_("creating missing \".\" entry in dir ino %llu\n"),
				ino);

			tp = libxfs_trans_alloc(mp, 0);
			ASSERT(tp != NULL);

			nres = XFS_MKDIR_SPACE_RES(mp, 1);
			error = libxfs_trans_reserve(tp, nres,
					XFS_MKDIR_LOG_RES(mp),
					0,
					XFS_TRANS_PERM_LOG_RES,
					XFS_MKDIR_LOG_COUNT);

			if (error)
				res_failed(error);

			libxfs_trans_ijoin(tp, ip, 0);
			libxfs_trans_ihold(tp, ip);

			XFS_BMAP_INIT(&flist, &first);

			if ((error = dir_createname(mp, tp, ip, ".",
					1, ip->i_ino, &first, &flist,
					nres)))
				do_error(_("can't make \".\" entry in dir ino "
					"%llu, createname error %d\n"),
					ino, error);

			libxfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);

			error = libxfs_bmap_finish(&tp, &flist, first,
					&committed);
			ASSERT(error == 0);
			libxfs_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES
					|XFS_TRANS_SYNC, 0);
		}
	}

	libxfs_iput(ip, 0);
}

/*
 * mark realtime bitmap and summary inodes as reached.
 * quota inode will be marked here as well
 */
void
mark_standalone_inodes(xfs_mount_t *mp)
{
	ino_tree_node_t		*irec;
	int			offset;

	irec = find_inode_rec(XFS_INO_TO_AGNO(mp, mp->m_sb.sb_rbmino),
			XFS_INO_TO_AGINO(mp, mp->m_sb.sb_rbmino));

	ASSERT(irec != NULL);

	offset = XFS_INO_TO_AGINO(mp, mp->m_sb.sb_rbmino) -
			irec->ino_startnum;

	add_inode_reached(irec, offset);

	irec = find_inode_rec(XFS_INO_TO_AGNO(mp, mp->m_sb.sb_rsumino),
			XFS_INO_TO_AGINO(mp, mp->m_sb.sb_rsumino));

	offset = XFS_INO_TO_AGINO(mp, mp->m_sb.sb_rsumino) -
			irec->ino_startnum;

	ASSERT(irec != NULL);

	add_inode_reached(irec, offset);

	if (fs_quotas)  {
		if (mp->m_sb.sb_uquotino
				&& mp->m_sb.sb_uquotino != NULLFSINO)  {
			irec = find_inode_rec(XFS_INO_TO_AGNO(mp,
						mp->m_sb.sb_uquotino),
				XFS_INO_TO_AGINO(mp, mp->m_sb.sb_uquotino));
			offset = XFS_INO_TO_AGINO(mp, mp->m_sb.sb_uquotino)
					- irec->ino_startnum;
			add_inode_reached(irec, offset);
		}
		if (mp->m_sb.sb_gquotino
				&& mp->m_sb.sb_gquotino != NULLFSINO)  {
			irec = find_inode_rec(XFS_INO_TO_AGNO(mp,
						mp->m_sb.sb_gquotino),
				XFS_INO_TO_AGINO(mp, mp->m_sb.sb_gquotino));
			offset = XFS_INO_TO_AGINO(mp, mp->m_sb.sb_gquotino)
					- irec->ino_startnum;
			add_inode_reached(irec, offset);
		}
	}
}

static void
check_for_orphaned_inodes(
	xfs_mount_t		*mp,
	xfs_agnumber_t		agno,
	ino_tree_node_t		*irec)
{
	int			i;
	xfs_ino_t		ino;

	for (i = 0; i < XFS_INODES_PER_CHUNK; i++)  {
		ASSERT(is_inode_confirmed(irec, i));
		if (is_inode_free(irec, i))
			continue;

		if (is_inode_reached(irec, i))
			continue;

		ASSERT(inode_isadir(irec, i) ||
			num_inode_references(irec, i) == 0);

		ino = XFS_AGINO_TO_INO(mp, agno, i + irec->ino_startnum);
		if (inode_isadir(irec, i))
			do_warn(_("disconnected dir inode %llu, "), ino);
		else
			do_warn(_("disconnected inode %llu, "), ino);
		if (!no_modify)  {
		    	if (!orphanage_ino)
				orphanage_ino = mk_orphanage(mp);
			do_warn(_("moving to %s\n"), ORPHANAGE);
			mv_orphanage(mp, ino, inode_isadir(irec, i));
		} else  {
			do_warn(_("would move to %s\n"), ORPHANAGE);
		}
		/*
		 * for read-only case, even though the inode isn't
		 * really reachable, set the flag (and bump our link
		 * count) anyway to fool phase 7
		 */
		add_inode_reached(irec, i);
	}
}

static void
traverse_function(
	work_queue_t		*wq,
	xfs_agnumber_t 		agno,
	void			*arg)
{
	ino_tree_node_t 	*irec;
	int			i;
	prefetch_args_t		*pf_args = arg;

	wait_for_inode_prefetch(pf_args);

	if (verbose)
		do_log(_("        - agno = %d\n"), agno);

	for (irec = findfirst_inode_rec(agno); irec; irec = next_ino_rec(irec)) {
		if (irec->ino_isa_dir == 0)
			continue;

		if (pf_args)
			sem_post(&pf_args->ra_count);

		for (i = 0; i < XFS_INODES_PER_CHUNK; i++)  {
			if (inode_isadir(irec, i))
				process_dir_inode(wq->mp,
					XFS_AGINO_TO_INO(wq->mp, agno,
					irec->ino_startnum + i), irec, i);
		}
	}
	cleanup_inode_prefetch(pf_args);
}

static void
traverse_ags(
	xfs_mount_t 		*mp)
{
	int			i;
	work_queue_t		queue;
	prefetch_args_t		*pf_args[2];

	/*
	 * we always do prefetch for phase 6 as it will fill in the gaps
	 * not read during phase 3 prefetch.
	 */
	queue.mp = mp;
	pf_args[0] = start_inode_prefetch(0, 1, NULL);
	for (i = 0; i < glob_agcount; i++) {
		pf_args[(~i) & 1] = start_inode_prefetch(i + 1, 1,
				pf_args[i & 1]);
		traverse_function(&queue, i, pf_args[i & 1]);
	}
}

void
phase6(xfs_mount_t *mp)
{
	ino_tree_node_t		*irec;
	int			i;

	memset(&zerocr, 0, sizeof(struct cred));
	memset(&zerofsx, 0, sizeof(struct fsxattr));
	orphanage_ino = 0;

	do_log(_("Phase 6 - check inode connectivity...\n"));

	if (!no_modify)
		teardown_bmap_finish(mp);
	else
		teardown_bmap(mp);

	incore_ext_teardown(mp);

	add_ino_ex_data(mp);

	/*
	 * verify existence of root directory - if we have to
	 * make one, it's ok for the incore data structs not to
	 * know about it since everything about it (and the other
	 * inodes in its chunk if a new chunk was created) are ok
	 */
	if (need_root_inode)  {
		if (!no_modify)  {
			do_warn(_("reinitializing root directory\n"));
			mk_root_dir(mp);
			need_root_inode = 0;
			need_root_dotdot = 0;
		} else  {
			do_warn(_("would reinitialize root directory\n"));
		}
	}

	if (need_rbmino)  {
		if (!no_modify)  {
			do_warn(_("reinitializing realtime bitmap inode\n"));
			mk_rbmino(mp);
			need_rbmino = 0;
		} else  {
			do_warn(_("would reinitialize realtime bitmap inode\n"));
		}
	}

	if (need_rsumino)  {
		if (!no_modify)  {
			do_warn(_("reinitializing realtime summary inode\n"));
			mk_rsumino(mp);
			need_rsumino = 0;
		} else  {
			do_warn(_("would reinitialize realtime summary inode\n"));
		}
	}

	if (!no_modify)  {
		do_log(
_("        - resetting contents of realtime bitmap and summary inodes\n"));
		if (fill_rbmino(mp))  {
			do_warn(
			_("Warning:  realtime bitmap may be inconsistent\n"));
		}

		if (fill_rsumino(mp))  {
			do_warn(
			_("Warning:  realtime bitmap may be inconsistent\n"));
		}
	}

	mark_standalone_inodes(mp);

	do_log(_("        - traversing filesystem ...\n"));

	irec = find_inode_rec(XFS_INO_TO_AGNO(mp, mp->m_sb.sb_rootino),
				XFS_INO_TO_AGINO(mp, mp->m_sb.sb_rootino));

	/*
	 * we always have a root inode, even if it's free...
	 * if the root is free, forget it, lost+found is already gone
	 */
	if (is_inode_free(irec, 0) || !inode_isadir(irec, 0))  {
		need_root_inode = 1;
	}

	/*
	 * then process all inodes by walking incore inode tree
	 */
	traverse_ags(mp);

	do_log(_("        - traversal finished ...\n"));
	do_log(_("        - moving disconnected inodes to %s ...\n"),
		ORPHANAGE);

	/*
	 * move all disconnected inodes to the orphanage
	 */
	for (i = 0; i < glob_agcount; i++)  {
		irec = findfirst_inode_rec(i);
		while (irec != NULL)  {
			check_for_orphaned_inodes(mp, i, irec);
			irec = next_ino_rec(irec);
		}
	}
}
