/*
 * Copyright (c) 2005 Silicon Graphics, Inc.  All Rights Reserved.
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

#include <xfs/xqm.h>
#include <xfs/path.h>
#include <xfs/project.h>

/*
 * Different forms of XFS quota
 */
enum {
	XFS_BLOCK_QUOTA	=	0x1,
	XFS_INODE_QUOTA =	0x2,
	XFS_RTBLOCK_QUOTA =	0x4,
};

/*
 * System call definitions mapping to platform-specific quotactl
 */
extern int xfsquotactl(int __cmd, const char *__device,
			uint __type, uint __id, void * __addr);
enum {
	XFS_QUOTAON,	/* enable accounting/enforcement */
	XFS_QUOTAOFF,	/* disable accounting/enforcement */
	XFS_GETQUOTA,	/* get disk limits and usage */
	XFS_SETQLIM,	/* set disk limits */
	XFS_GETQSTAT,	/* get quota subsystem status */
	XFS_QUOTARM,	/* free disk space used by dquots */
};

/*
 * Utility routines
 */
extern char *type_to_string(uint __type);
extern char *form_to_string(uint __form);
extern char *time_to_string(__uint32_t __time, uint __flags);
extern char *bbs_to_string(__uint64_t __v, char *__c, uint __size);
extern char *num_to_string(__uint64_t __v, char *__c, uint __size);
extern char *pct_to_string(__uint64_t __v, __uint64_t __t, char *__c, uint __s);

extern FILE *fopen_write_secure(char *__filename);

/*
 * Various utility routine flags
 */
enum {
	NO_HEADER_FLAG =	0x0001,	/* don't print header */
	VERBOSE_FLAG =		0x0002,	/* increase verbosity */
	HUMAN_FLAG =		0x0004,	/* human-readable values */
	QUOTA_FLAG =		0x0008,	/* uid/gid/prid over-quota (soft) */
	LIMIT_FLAG =		0x0010,	/* uid/gid/prid over-limit (hard) */
	ALL_MOUNTS_FLAG =	0x0020,	/* iterate over every mounted xfs */
	TERSE_FLAG =		0x0040,	/* decrease verbosity */
	HISTOGRAM_FLAG =	0x0080,	/* histogram format output */
	DEFAULTS_FLAG =		0x0100,	/* use value as a default */
	ABSOLUTE_FLAG =		0x0200, /* absolute time, not related to now */
};

/*
 * Identifier (uid/gid/prid) cache routines
 */
extern char *uid_to_name(__uint32_t __uid);
extern char *gid_to_name(__uint32_t __gid);
extern char *prid_to_name(__uint32_t __prid);

