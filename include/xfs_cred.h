/*
 * Copyright (c) 2000 Silicon Graphics, Inc.  All Rights Reserved.
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
#ifndef __XFS_CRED_H__
#define __XFS_CRED_H__

/*
 * Access Control Lists
 */
typedef ushort  xfs_acl_perm_t;
typedef int     xfs_acl_type_t;
typedef int     xfs_acl_tag_t;
typedef int	xfs_acl_id_t;

#define XFS_ACL_MAX_ENTRIES 25
#define XFS_ACL_NOT_PRESENT (-1)

typedef struct xfs_acl_entry {
	xfs_acl_tag_t	ae_tag;
	xfs_acl_id_t	ae_id;
	xfs_acl_perm_t	ae_perm;
} xfs_acl_entry_t;

typedef struct xfs_acl {
	int		acl_cnt;
	xfs_acl_entry_t	acl_entry[XFS_ACL_MAX_ENTRIES];
} xfs_acl_t;

/*
 * Capabilities
 */
typedef __uint64_t xfs_cap_value_t;

typedef struct xfs_cap_set {
	xfs_cap_value_t	cap_effective;	/* use in capability checks */
	xfs_cap_value_t	cap_permitted;	/* combined with file attrs */
	xfs_cap_value_t	cap_inheritable;/* pass through exec */
} xfs_cap_set_t;


/*
 * Mandatory Access Control
 *
 * Layout of a composite MAC label:
 * ml_list contains the list of categories (MSEN) followed by the list of
 * divisions (MINT). This is actually a header for the data structure which
 * will have an ml_list with more than one element.
 *
 *      -------------------------------
 *      | ml_msen_type | ml_mint_type |
 *      -------------------------------
 *      | ml_level     | ml_grade     |
 *      -------------------------------
 *      | ml_catcount                 |
 *      -------------------------------
 *      | ml_divcount                 |
 *      -------------------------------
 *      | category 1                  |
 *      | . . .                       |
 *      | category N                  | (where N = ml_catcount)
 *      -------------------------------
 *      | division 1                  |
 *      | . . .                       |
 *      | division M                  | (where M = ml_divcount)
 *      -------------------------------
 */
#define XFS_MAC_MAX_SETS	250
typedef struct xfs_mac_label {
	unsigned char	ml_msen_type;	/* MSEN label type */
	unsigned char	ml_mint_type;	/* MINT label type */
	unsigned char	ml_level;	/* Hierarchical level  */
	unsigned char	ml_grade;	/* Hierarchical grade  */
	unsigned short	ml_catcount;	/* Category count */
	unsigned short	ml_divcount;	/* Division count */
					/* Category set, then Division set */
	unsigned short	ml_list[XFS_MAC_MAX_SETS];
} xfs_mac_label_t;

/* On-disk XFS extended attribute names (access control lists) */
#define SGI_ACL_FILE	"SGI_ACL_FILE"
#define SGI_ACL_DEFAULT	"SGI_ACL_DEFAULT"
#define SGI_ACL_FILE_SIZE	(sizeof(SGI_ACL_FILE)-1)
#define SGI_ACL_DEFAULT_SIZE	(sizeof(SGI_ACL_DEFAULT)-1)

/* On-disk XFS extended attribute names (mandatory access control) */
#define SGI_BI_FILE	"SGI_BI_FILE"
#define SGI_BLS_FILE	"SGI_BLS_FILE"
#define SGI_MAC_FILE	"SGI_MAC_FILE"
#define SGI_BI_FILE_SIZE	(sizeof(SGI_BI_FILE)-1)
#define SGI_BLS_FILE_SIZE	(sizeof(SGI_BLS_FILE)-1)
#define SGI_MAC_FILE_SIZE	(sizeof(SGI_MAC_FILE)-1)

/* On-disk XFS extended attribute names (capabilities) */
#define SGI_CAP_FILE	"SGI_CAP_FILE"
#define SGI_CAP_FILE_SIZE	(sizeof(SGI_CAP_FILE)-1)

/* MSEN label type names. Choose an upper case ASCII character.  */
#define MSEN_ADMIN_LABEL	'A'	/* Admin: low<admin != tcsec<high */
#define MSEN_EQUAL_LABEL	'E'	/* Wildcard - always equal */
#define MSEN_HIGH_LABEL		'H'	/* System High - always dominates */
#define MSEN_MLD_HIGH_LABEL	'I'	/* System High, multi-level dir */
#define MSEN_LOW_LABEL		'L'	/* System Low - always dominated */
#define MSEN_MLD_LABEL		'M'	/* TCSEC label on a multi-level dir */
#define MSEN_MLD_LOW_LABEL	'N'	/* System Low, multi-level dir */
#define MSEN_TCSEC_LABEL	'T'	/* TCSEC label */
#define MSEN_UNKNOWN_LABEL	'U'	/* unknown label */

/* MINT label type names. Choose a lower case ASCII character.  */
#define MINT_BIBA_LABEL		'b'	/* Dual of a TCSEC label */
#define MINT_EQUAL_LABEL	'e'	/* Wildcard - always equal */
#define MINT_HIGH_LABEL		'h'	/* High Grade - always dominates */
#define MINT_LOW_LABEL		'l'	/* Low Grade - always dominated */


#ifdef __KERNEL__

#include <asm/param.h>		/* For NGROUPS */
#include <linux/capability.h>
#include <linux/sched.h>

/*
 * Credentials
 */
typedef struct cred {
	int	cr_ref;			/* reference count */
	ushort	cr_ngroups;		/* number of groups in cr_groups */
	uid_t	cr_uid;			/* effective user id */
	gid_t	cr_gid;		 	/* effective group id */
	uid_t	cr_ruid;		/* real user id */
	gid_t	cr_rgid;		/* real group id */
	uid_t	cr_suid;		/* "saved" user id (from exec) */
	gid_t	cr_sgid;		/* "saved" group id (from exec) */
	xfs_mac_label_t	*cr_mac;	/* MAC label for B1 and beyond */
	xfs_cap_set_t	cr_cap;		/* capability (privilege) sets */
	gid_t	cr_groups[NGROUPS];	/* supplementary group list */
} cred_t;

#define VREAD		00400
#define VWRITE		00200
#define VEXEC		00100
#define MACEXEC		00100
#define MACWRITE	00200
#define MACREAD		00400

extern void cred_init(void);
static __inline cred_t *get_current_cred(void) { return NULL; }
/* 
 * XXX: tes
 * This is a hack. 
 * It assumes that if cred is not null then it is sys_cred which
 * has all capabilities.
 * One solution may be to implement capable_cred based on linux' capable()
 * and initialize all credentials in our xfs linvfs layer.
 */
static __inline int capable_cred(cred_t *cr, int cid) { return (cr==NULL) ? capable(cid) : 1; }
extern struct cred *sys_cred;
#endif	/* __KERNEL__ */

#endif  /* __XFS_CRED_H__ */
