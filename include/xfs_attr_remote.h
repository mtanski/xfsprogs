/*
 * Copyright (c) 2013 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2.1 of the GNU Lesser General Public License
 * as published by the Free Software Foundation.
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston MA 02111-1307,
 * USA.
 */
#ifndef __XFS_ATTR_REMOTE_H__
#define	__XFS_ATTR_REMOTE_H__

#define XFS_ATTR3_RMT_MAGIC	0x5841524d	/* XARM */

struct xfs_attr3_rmt_hdr {
	__be32	rm_magic;
	__be32	rm_offset;
	__be32	rm_bytes;
	__be32	rm_crc;
	uuid_t	rm_uuid;
	__be64	rm_owner;
	__be64	rm_blkno;
	__be64	rm_lsn;
};

#define XFS_ATTR3_RMT_CRC_OFF	offsetof(struct xfs_attr3_rmt_hdr, rm_crc)

#define XFS_ATTR3_RMT_BUF_SPACE(mp, bufsize)	\
	((bufsize) - (xfs_sb_version_hascrc(&(mp)->m_sb) ? \
			sizeof(struct xfs_attr3_rmt_hdr) : 0))

extern const struct xfs_buf_ops xfs_attr3_rmt_buf_ops;

int xfs_attr_rmtval_get(struct xfs_da_args *args);
int xfs_attr_rmtval_set(struct xfs_da_args *args);
int xfs_attr_rmtval_remove(struct xfs_da_args *args);

#endif /* __XFS_ATTR_REMOTE_H__ */
