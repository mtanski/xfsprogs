#include <libxfs.h>
#include "prefetch.h"
#include "aio.h"
#include "avl.h"
#include "globals.h"
#include "agheader.h"
#include "incore.h"
#include "dir.h"
#include "dir2.h"
#include "dir_stack.h"
#include "protos.h"
#include "err_protos.h"
#include "dinode.h"
#include "bmap.h"
#include "versions.h"

int do_prefetch = 1;

ino_tree_node_t *
prefetch_inode_chunks(xfs_mount_t *mp,
		xfs_agnumber_t agno,
		ino_tree_node_t *ino_ra)
{
	xfs_agblock_t agbno;
	libxfs_lio_req_t *liop;
	int i;

	if (libxfs_lio_ino_count == 0)
		return NULL;

	liop = (libxfs_lio_req_t *) libxfs_get_lio_buffer(LIBXFS_LIO_TYPE_INO);
	if (liop == NULL) {
		do_prefetch = 0;
		return NULL;
	}

	if (ino_ra == NULL)
		ino_ra = findfirst_inode_rec(agno);

	i = 0;
	while (ino_ra) {
		agbno = XFS_AGINO_TO_AGBNO(mp, ino_ra->ino_startnum);
		liop[i].blkno = XFS_AGB_TO_DADDR(mp, agno, agbno);
		liop[i].len = (int) XFS_FSB_TO_BB(mp, XFS_IALLOC_BLOCKS(mp));
		i++;
		ino_ra = next_ino_rec(ino_ra);
		if (i >= libxfs_lio_ino_count)
			break;
	}
	if (i) {
		if (libxfs_readbuf_list(mp->m_dev, i, (void *) liop, LIBXFS_LIO_TYPE_INO) == -1)
			do_prefetch = 0;
	}
	libxfs_put_lio_buffer((void *) liop);
	return (ino_ra);
}

static void
prefetch_node(
	xfs_mount_t		*mp,
	xfs_buf_t		*bp,
	da_bt_cursor_t		*da_cursor)
{
	xfs_da_intnode_t	*node;
	libxfs_lio_req_t	*liop;
	int			i;
	xfs_dfsbno_t		fsbno;

	node = (xfs_da_intnode_t *)XFS_BUF_PTR(bp);
	if (INT_GET(node->hdr.count, ARCH_CONVERT) <= 1)
		return;

	if ((liop = (libxfs_lio_req_t *) libxfs_get_lio_buffer(LIBXFS_LIO_TYPE_DIR)) == NULL) {
		return;
	}

	for (i = 0; i < INT_GET(node->hdr.count, ARCH_CONVERT); i++) {
		if (i == libxfs_lio_dir_count)
			break;

		fsbno = blkmap_get(da_cursor->blkmap, INT_GET(node->btree[i].before, ARCH_CONVERT));
		if (fsbno == NULLDFSBNO) {
			libxfs_put_lio_buffer((void *) liop);
			return;
		}

		liop[i].blkno = XFS_FSB_TO_DADDR(mp, fsbno);
		liop[i].len =  XFS_FSB_TO_BB(mp, 1);
	}

	if (i > 1) {
		if (libxfs_readbuf_list(mp->m_dev, i, (void *) liop, LIBXFS_LIO_TYPE_DIR) == -1)
			do_prefetch = 0;
	}

	libxfs_put_lio_buffer((void *) liop);
	return;
}

void
prefetch_dir1(
	xfs_mount_t		*mp,
	xfs_dablk_t		bno,
	da_bt_cursor_t		*da_cursor)
{
	xfs_da_intnode_t	*node;
	xfs_buf_t		*bp;
	xfs_dfsbno_t		fsbno;
	int			i;

	fsbno = blkmap_get(da_cursor->blkmap, bno);
	if (fsbno == NULLDFSBNO)
		return;

	bp = libxfs_readbuf(mp->m_dev, XFS_FSB_TO_DADDR(mp, fsbno),
			XFS_FSB_TO_BB(mp, 1), 0);

	if (bp == NULL)
	 	return;


	node = (xfs_da_intnode_t *)XFS_BUF_PTR(bp);
	if (INT_GET(node->hdr.info.magic, ARCH_CONVERT) != XFS_DA_NODE_MAGIC)  {
		libxfs_putbuf(bp);
		return;
	}

	prefetch_node(mp, bp, da_cursor);

	/* skip prefetching if next level is leaf level */
	if (INT_GET(node->hdr.level, ARCH_CONVERT) > 1) {
		for (i = 0; i < INT_GET(node->hdr.count, ARCH_CONVERT); i++) {
			prefetch_dir1(mp,
				INT_GET(node->btree[i].before, ARCH_CONVERT),
				da_cursor);
		}
	}
	
	libxfs_putbuf(bp);
	return;
}

void
prefetch_dir2(
	xfs_mount_t     *mp,
	blkmap_t        *blkmap)
{
	xfs_dfiloff_t		dbno;
	xfs_dfiloff_t		pdbno;
	bmap_ext_t		*bmp;	
	int			nex;
	int			i, j, t;
	libxfs_lio_req_t	*liop;

	liop = (libxfs_lio_req_t *) libxfs_get_lio_buffer(LIBXFS_LIO_TYPE_DIR);
	if (liop == NULL)
		return;

	pdbno = NULLDFILOFF;	/* previous dbno is NULLDFILOFF */
	i = 0;
	while ((dbno = blkmap_next_off(blkmap, pdbno, &t)) < mp->m_dirfreeblk) {
		if (i == libxfs_lio_dir_count)
			break;
		if (dbno == NULLDFILOFF)
			break;
		if (mp->m_dirblkfsbs == 1) {
			xfs_dfsbno_t blk;

			/* avoid bmp realloc/free overhead, use blkmap_get */
			blk = blkmap_get(blkmap, dbno);
			if (blk == NULLDFSBNO)
				break;
			pdbno = dbno;
			liop[i].blkno = XFS_FSB_TO_DADDR(mp, blk);
			liop[i].len = (int) XFS_FSB_TO_BB(mp, 1);
			i++;
		}
		else if (mp->m_dirblkfsbs > 1) {
			nex = blkmap_getn(blkmap, dbno, mp->m_dirblkfsbs, &bmp, NULL);
			if (nex == 0)
				break;
			pdbno = dbno + mp->m_dirblkfsbs - 1;
			for (j = 0; j < nex; j++) {
				liop[i].blkno = XFS_FSB_TO_DADDR(mp, bmp[j].startblock);
				liop[i].len = (int) XFS_FSB_TO_BB(mp, bmp[j].blockcount);
				i++;
				if (i == libxfs_lio_dir_count)
					break;	/* for loop */
			}
			free(bmp);
		}
		else {
			do_error("invalid mp->m_dirblkfsbs %d\n", mp->m_dirblkfsbs);
		}
	}
	if (i > 1) {
		if (libxfs_readbuf_list(mp->m_dev, i, (void *) liop, LIBXFS_LIO_TYPE_DIR) == -1)
			do_prefetch = 0;
	}
	libxfs_put_lio_buffer((void *) liop);
}

static void
prefetch_p6_node(
	xfs_mount_t		*mp,
	xfs_inode_t		*ip,
	xfs_buf_t		*bp)
{
	xfs_da_intnode_t	*node;
	libxfs_lio_req_t	*liop;
	int			i;
	xfs_fsblock_t		fblock;
	xfs_dfsbno_t		fsbno;
	xfs_bmbt_irec_t		map;
	int			nmap;
	int			error;

	node = (xfs_da_intnode_t *)XFS_BUF_PTR(bp);
	if (INT_GET(node->hdr.count, ARCH_CONVERT) <= 1)
		return;

	if ((liop = (libxfs_lio_req_t *) libxfs_get_lio_buffer(LIBXFS_LIO_TYPE_DIR)) == NULL) {
		return;
	}

	fblock = NULLFSBLOCK;

	for (i = 0; i < INT_GET(node->hdr.count, ARCH_CONVERT); i++) {
		if (i == libxfs_lio_dir_count)
			break;

		nmap = 1;
		error = libxfs_bmapi(NULL, ip, (xfs_fileoff_t)
				INT_GET(node->btree[i].before, ARCH_CONVERT), 1,
				XFS_BMAPI_METADATA, &fblock, 0,
				&map, &nmap, NULL);

		if (error || (nmap != 1)) {
			libxfs_put_lio_buffer((void *) liop);
			return;
		}

		if ((fsbno = map.br_startblock) == HOLESTARTBLOCK) {
			libxfs_put_lio_buffer((void *) liop);
			return;
		}
		liop[i].blkno = XFS_FSB_TO_DADDR(mp, fsbno);
		liop[i].len =  XFS_FSB_TO_BB(mp, 1);
	}

	if (i > 1) {
		if (libxfs_readbuf_list(mp->m_dev, i, (void *) liop, LIBXFS_LIO_TYPE_DIR) == -1)
			do_prefetch = 0;
	}

	libxfs_put_lio_buffer((void *) liop);
	return;
}

void
prefetch_p6_dir1(
	xfs_mount_t		*mp,
	xfs_ino_t		ino,
	xfs_inode_t		*ip,
	xfs_dablk_t		da_bno,
	xfs_fsblock_t		*fblockp)
{
	xfs_da_intnode_t	*node;
	xfs_buf_t		*bp;
	xfs_dfsbno_t		fsbno;
	xfs_bmbt_irec_t		map;
	int			nmap;
	int			i;
	int			error;

	nmap = 1;
	error = libxfs_bmapi(NULL, ip, (xfs_fileoff_t) da_bno, 1,
			XFS_BMAPI_METADATA, fblockp, 0,
			&map, &nmap, NULL);
	if (error || (nmap != 1))  {
		return;
	}

	if ((fsbno = map.br_startblock) == HOLESTARTBLOCK)
		return;

	bp = libxfs_readbuf(mp->m_dev, XFS_FSB_TO_DADDR(mp, fsbno),
			XFS_FSB_TO_BB(mp, 1), 0);

	if (bp == NULL)
	 	return;


	node = (xfs_da_intnode_t *)XFS_BUF_PTR(bp);
	if (INT_GET(node->hdr.info.magic, ARCH_CONVERT) != XFS_DA_NODE_MAGIC)  {
		libxfs_putbuf(bp);
		return;
	}

	prefetch_p6_node(mp, ip, bp);

	/* skip prefetching if next level is leaf level */
	if (INT_GET(node->hdr.level, ARCH_CONVERT) > 1) {
		for (i = 0; i < INT_GET(node->hdr.count, ARCH_CONVERT); i++) {
			(void) prefetch_p6_dir1(mp, ino, ip,
				INT_GET(node->btree[i].before, ARCH_CONVERT),
				fblockp);
		}
	}
	
	libxfs_putbuf(bp);
	return;
}

#define	NMAPP	4

void
prefetch_p6_dir2(
	xfs_mount_t     *mp,
	xfs_inode_t	*ip)
{
	xfs_fileoff_t		da_bno;
	xfs_fileoff_t		next_da_bno;
	int			i, j;
	libxfs_lio_req_t	*liop;
	xfs_fsblock_t		fsb;
	int			nfsb;
	int			error;

	if ((liop = (libxfs_lio_req_t *) libxfs_get_lio_buffer(LIBXFS_LIO_TYPE_DIR)) == NULL) {
		return;
	}
	i = 0;
	for (da_bno = 0, next_da_bno = 0; next_da_bno != NULLFILEOFF; da_bno = next_da_bno) {
		if (i == libxfs_lio_dir_count)
			break;
		next_da_bno = da_bno + mp->m_dirblkfsbs - 1;
		if (libxfs_bmap_next_offset(NULL, ip, &next_da_bno, XFS_DATA_FORK))
			break;

		if (mp->m_dirblkfsbs == 1) {
			if ((error = libxfs_bmapi_single(NULL, ip, XFS_DATA_FORK, &fsb, da_bno)) != 0) {
				libxfs_put_lio_buffer((void *) liop);
				do_prefetch = 0;
				do_warn("phase6 prefetch: cannot bmap single block err = %d\n", error);
				return;
			}
			if (fsb == NULLFSBLOCK) {
				libxfs_put_lio_buffer((void *) liop);
				return;
			}

			liop[i].blkno = XFS_FSB_TO_DADDR(mp, fsb);
			liop[i].len =  XFS_FSB_TO_BB(mp, 1);
			i++;
		}
		else if ((nfsb = mp->m_dirblkfsbs) > 1) {
			xfs_fsblock_t   firstblock;
			xfs_bmbt_irec_t map[NMAPP];
			xfs_bmbt_irec_t *mapp;
			int             nmap;

			if (nfsb > NMAPP) {
                        	mapp = malloc(sizeof(*mapp) * nfsb);
				if (mapp == NULL) {
					libxfs_put_lio_buffer((void *) liop);
					do_prefetch = 0;
					do_warn("phase6 prefetch: cannot allocate mem for map\n");
					return;
				}
			}
			else {
				mapp = map;
			}
                        firstblock = NULLFSBLOCK;
                        nmap = nfsb;
                        if ((error = libxfs_bmapi(NULL, ip, da_bno,
                                        nfsb,
                                        XFS_BMAPI_METADATA | XFS_BMAPI_AFLAG(XFS_DATA_FORK),
                                        &firstblock, 0, mapp, &nmap, NULL))) {
				libxfs_put_lio_buffer((void *) liop);
				do_prefetch = 0;
				do_warn("phase6 prefetch: cannot bmap err = %d\n", error);
				return;
			}
			for (j = 0; j < nmap; j++) {
				liop[i].blkno = XFS_FSB_TO_DADDR(mp, mapp[j].br_startblock);
				liop[i].len = (int)XFS_FSB_TO_BB(mp, mapp[j].br_blockcount);
				i++;
				if (i == libxfs_lio_dir_count)
					break; /* for loop */
			}
			if (mapp != map)
				free(mapp);

		}
		else {
			do_error("phase6: invalid mp->m_dirblkfsbs %d\n", mp->m_dirblkfsbs);
		}
	}
	if (i > 1) {
		if (libxfs_readbuf_list(mp->m_dev, i, (void *) liop, LIBXFS_LIO_TYPE_DIR) == -1)
			do_prefetch = 0;
	}
	libxfs_put_lio_buffer((void *) liop);
}

void
prefetch_sb(xfs_mount_t *mp, xfs_agnumber_t  agno)
{
	libxfs_lio_req_t	*liop;

	if ((liop = (libxfs_lio_req_t *) libxfs_get_lio_buffer(LIBXFS_LIO_TYPE_RAW)) == NULL) {
		do_prefetch = 0;
		return;
	}

	liop[0].blkno = XFS_AG_DADDR(mp, agno, XFS_SB_DADDR);
	liop[1].blkno = XFS_AG_DADDR(mp, agno, XFS_AGF_DADDR(mp));
	liop[2].blkno = XFS_AG_DADDR(mp, agno, XFS_AGI_DADDR(mp));
	liop[0].len = XFS_FSS_TO_BB(mp, 1);
	liop[1].len = XFS_FSS_TO_BB(mp, 1);
	liop[2].len = XFS_FSS_TO_BB(mp, 1);
	if (libxfs_readbuf_list(mp->m_dev, 3, (void *) liop, LIBXFS_LIO_TYPE_RAW) == -1)
		do_prefetch = 0;

	libxfs_put_lio_buffer((void *) liop);
}

void
prefetch_roots(xfs_mount_t *mp, xfs_agnumber_t agno,
		xfs_agf_t *agf, xfs_agi_t *agi)
{
	int			i;
	libxfs_lio_req_t	*liop;

	if ((liop = (libxfs_lio_req_t *) libxfs_get_lio_buffer(LIBXFS_LIO_TYPE_RAW)) == NULL) {
		do_prefetch = 0;
		return;
	}

	i = 0;
	if (agf->agf_roots[XFS_BTNUM_BNO] != 0 &&
			verify_agbno(mp, agno, agf->agf_roots[XFS_BTNUM_BNO])) {
		liop[i].blkno = XFS_AGB_TO_DADDR(mp, agno, agf->agf_roots[XFS_BTNUM_BNO]);
		liop[i].len = XFS_FSB_TO_BB(mp, 1);
		i++;
	}
	if (agf->agf_roots[XFS_BTNUM_CNT] != 0 &&
			verify_agbno(mp, agno, agf->agf_roots[XFS_BTNUM_CNT])) {
		liop[i].blkno = XFS_AGB_TO_DADDR(mp, agno, agf->agf_roots[XFS_BTNUM_CNT]);
		liop[i].len = XFS_FSB_TO_BB(mp, 1);
		i++;
	}
	if (agi->agi_root != 0 && verify_agbno(mp, agno, agi->agi_root)) {
		liop[i].blkno = XFS_AGB_TO_DADDR(mp, agno, agi->agi_root);
		liop[i].len = XFS_FSB_TO_BB(mp, 1);
		i++;
	}
	if (i > 1) {
		if (libxfs_readbuf_list(mp->m_dev, i, (void *) liop, LIBXFS_LIO_TYPE_RAW) == -1)
			do_prefetch = 0;
	}

	libxfs_put_lio_buffer((void *) liop);
}
