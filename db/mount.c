/*
 * Copyright (c) 2000-2001 Silicon Graphics, Inc.  All Rights Reserved.
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

#include <libxfs.h>
#include "init.h"
#include "io.h"
#include "mount.h"
#include "malloc.h"
#include "data.h"

xfs_mount_t	*mp;

xfs_mount_t *
dbmount(void)
{
	void		*bufp;
	xfs_mount_t	*mp;
	xfs_sb_t	*sbp;

	mp = xcalloc(1, sizeof(*mp));
	bufp = NULL;
	if (read_bbs(XFS_SB_DADDR, 1, &bufp, NULL))
		return NULL;

        /* copy sb from buf to in-core, converting architecture */
        libxfs_xlate_sb(bufp, &mp->m_sb, 1, ARCH_CONVERT, XFS_SB_ALL_BITS);
	xfree(bufp);
	sbp = &mp->m_sb;
        if (sbp->sb_magicnum != XFS_SB_MAGIC) {
            fprintf(stderr,"%s: unexpected XFS SB magic number 0x%08x\n",
                    progname, sbp->sb_magicnum);
        }

	libxfs_mount_common(mp, sbp);
	libxfs_bmap_compute_maxlevels(mp, XFS_DATA_FORK);
	libxfs_bmap_compute_maxlevels(mp, XFS_ATTR_FORK);
	libxfs_ialloc_compute_maxlevels(mp);

	if (sbp->sb_rblocks) {
		mp->m_rsumlevels = sbp->sb_rextslog + 1;
		mp->m_rsumsize =
			(uint)sizeof(xfs_suminfo_t) * mp->m_rsumlevels *
			sbp->sb_rbmblocks;
		if (sbp->sb_blocksize)
			mp->m_rsumsize =
				roundup(mp->m_rsumsize, sbp->sb_blocksize);
	}

	if (XFS_SB_VERSION_HASDIRV2(sbp)) {
		libxfs_dir2_mount(mp);
	} else {
		libxfs_dir_mount(mp);
	}
	return mp;
}
