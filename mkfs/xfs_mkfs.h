/*
 * Copyright (c) 2000-2001,2004 Silicon Graphics, Inc.  All Rights Reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * 
 * Further, this software is distributed without any warranty that it is
 * free of the rightful claim of any third person regarding infringement
 * or the like.  Any license provided herein, whether implied or
 * otherwise, applies only to this software file.  Patent licenses, if
 * any, provided herein do not apply to combinations of this program with
 * other software, or any other product whatsoever.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston MA 02111-1307, USA.
 * 
 * Contact information: Silicon Graphics, Inc., 1600 Amphitheatre Pkwy,
 * Mountain View, CA  94043, or:
 * 
 * http://www.sgi.com 
 * 
 * For further information regarding this notice, see: 
 * 
 * http://oss.sgi.com/projects/GenInfo/SGIGPLNoticeExplan/
 */
#ifndef __XFS_MKFS_H__
#define	__XFS_MKFS_H__

#define	XFS_DFL_BLOCKSIZE_LOG	12		/* 4096 byte blocks */
#define	XFS_DINODE_DFL_LOG	8		/* 256 byte inodes */
#define	XFS_MIN_DATA_BLOCKS	100
#define	XFS_MIN_INODE_PERBLOCK	2		/* min inodes per block */
#define	XFS_DFL_IMAXIMUM_PCT	25		/* max % of space for inodes */
#define	XFS_IFLAG_ALIGN		1		/* -i align defaults on */
#define	XFS_MIN_REC_DIRSIZE	12		/* 4096 byte dirblocks (V2) */
#define	XFS_DFL_DIR_VERSION	2		/* default directory version */
#define	XFS_DFL_LOG_SIZE	1000		/* default log size, blocks */
#define	XFS_MIN_LOG_FACTOR	3		/* min log size factor */
#define	XFS_DFL_LOG_FACTOR	16		/* default log size, factor */
						/* with max trans reservation */
#define XFS_MAX_INODE_SIG_BITS	32		/* most significant bits in an 
						 * inode number that we'll
						 * accept w/o warnings
						 */

#define XFS_AG_BYTES(bblog)	((long long)BBSIZE << (bblog))
#define	XFS_AG_MIN_BYTES	((XFS_AG_BYTES(15)))	/* 16 MB */
#define XFS_AG_MIN_BLOCKS(blog)	((XFS_AG_BYTES(15)) >> (blog))
#define XFS_AG_MAX_BLOCKS(blog)	((XFS_AG_BYTES(31) - 1) >> (blog))

#define XFS_MAX_AGNUMBER	((xfs_agnumber_t)(NULLAGNUMBER - 1))


/* xfs_mkfs.c */
extern void usage (void);
extern int isdigits (char *str);
extern long long cvtnum (int blocksize, int sectorsize, char *s);

/* proto.c */
extern char *setup_proto (char *fname);
extern void parseproto (xfs_mount_t *mp, xfs_inode_t *pip, char **pp, char *n);
extern void res_failed (int err);

/* maxtrres.c */ 
extern int max_trans_res (int dirversion,
		int sectorlog, int blocklog, int inodelog, int dirblocklog);

#endif	/* __XFS_MKFS_H__ */
