/*
 * Copyright (c) 2000-2002 Silicon Graphics, Inc.  All Rights Reserved.
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
#ifndef __XFS_CAP_H__
#define __XFS_CAP_H__

/*
 * Capabilities
 */
typedef __uint64_t xfs_cap_value_t;

typedef struct xfs_cap_set {
	xfs_cap_value_t	cap_effective;	/* use in capability checks */
	xfs_cap_value_t	cap_permitted;	/* combined with file attrs */
	xfs_cap_value_t	cap_inheritable;/* pass through exec */
} xfs_cap_set_t;

/* On-disk XFS extended attribute names */
#define SGI_CAP_FILE	"SGI_CAP_FILE"
#define SGI_CAP_FILE_SIZE	(sizeof(SGI_CAP_FILE)-1)

/* On-disk bitfield positions, as defined for us by IRIX */
#define	XFS_CAP_CHOWN			1
#define	XFS_CAP_DAC_WRITE		2
#define	XFS_CAP_DAC_READ_SEARCH		3
#define	XFS_CAP_FOWNER			4
#define	XFS_CAP_FSETID			5
#define	XFS_CAP_KILL			6
#define	XFS_CAP_LINK_DIR		7
#define	XFS_CAP_SETFPRIV		8
#define	XFS_CAP_SETPPRIV		9
#define	XFS_CAP_SETGID			10
#define	XFS_CAP_SETUID			11
#define	XFS_CAP_MAC_DOWNGRADE		12
#define	XFS_CAP_MAC_READ		13
#define	XFS_CAP_MAC_RELABEL_SUBJ	14
#define	XFS_CAP_MAC_WRITE		15
#define	XFS_CAP_MAC_UPGRADE		16
#define	XFS_CAP_INF_NOFLOAT_OBJ		17	/* Currently unused */
#define	XFS_CAP_INF_NOFLOAT_SUBJ	18	/* Currently unused */
#define	XFS_CAP_INF_DOWNGRADE		19	/* Currently unused */
#define	XFS_CAP_INF_UPGRADE		20	/* Currently unused */
#define	XFS_CAP_INF_RELABEL_SUBJ	21	/* Currently unused */
#define	XFS_CAP_AUDIT_CONTROL		22
#define	XFS_CAP_AUDIT_WRITE		23
#define	XFS_CAP_MAC_MLD			24
#define	XFS_CAP_MEMORY_MGT		25
#define	XFS_CAP_SWAP_MGT		26
#define	XFS_CAP_TIME_MGT		27
#define	XFS_CAP_SYSINFO_MGT		28
#define	XFS_CAP_MOUNT_MGT		29
#define	XFS_CAP_QUOTA_MGT		30
#define	XFS_CAP_PRIV_PORT		31
#define	XFS_CAP_STREAMS_MGT		32
#define	XFS_CAP_SCHED_MGT		33
#define	XFS_CAP_PROC_MGT		34
#define	XFS_CAP_SVIPC_MGT		35
#define	XFS_CAP_NETWORK_MGT		36
#define	XFS_CAP_DEVICE_MGT		37
#define	XFS_CAP_ACCT_MGT		38
#define	XFS_CAP_SHUTDOWN		39
#define	XFS_CAP_CHROOT			40
#define	XFS_CAP_DAC_EXECUTE		41
#define	XFS_CAP_MAC_RELABEL_OPEN	42
#define	XFS_CAP_SIGMASK			43	/* Not implemented */
#define	XFS_CAP_XTCB			44	/* X11 Trusted Client */


#ifdef __KERNEL__

#ifdef CONFIG_FS_POSIX_CAP

#include <linux/posix_cap_xattr.h>

struct vnode;

extern int xfs_cap_vhascap(struct vnode *);
extern int xfs_cap_vset(struct vnode *, void *, size_t);
extern int xfs_cap_vget(struct vnode *, void *, size_t);
extern int xfs_cap_vremove(struct vnode *vp);

#define _CAP_EXISTS		xfs_cap_vhascap

#else
#define xfs_cap_vset(v,p,sz)	(-ENOTSUP)
#define xfs_cap_vget(v,p,sz)	(-ENOTSUP)
#define xfs_cap_vremove(v)	(-ENOTSUP)
#define _CAP_EXISTS		(NULL)
#endif

#endif	/* __KERNEL__ */

#endif  /* __XFS_CAP_H__ */
