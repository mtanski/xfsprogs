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

#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <volume.h>

#if HAVE_LIBLVM
  #include "lvm_user.h"

  char *cmd;		/* Not used. liblvm is broken */
  int opt_d;		/* Same thing */
#endif


int
lvm_get_subvol_stripe(
	char		*dfile,
	sv_type_t	type,
	int		*sunit,
	int		*swidth,
	struct stat64	*sb)
{
#if HAVE_LIBLVM
	if (sb->st_rdev >> 8 == LVM_BLK_MAJOR) {
		lv_t	*lv;
		char	*vgname;

		/* Find volume group */
		if (! (vgname = vg_name_of_lv(dfile))) {
			fprintf(stderr, "Can't find volume group for %s\n", 
				dfile);
			exit(1);
		}
		
		/* Logical volume */
		if (! lvm_tab_lv_check_exist(dfile)) {
			fprintf(stderr, "Logical volume %s doesn't exist!\n",
				dfile);
			exit(1);
		}
		
		/* Get status */
		if (lv_status_byname(vgname, dfile, &lv) < 0 || lv == NULL) {
			fprintf(stderr, "Could not get status info from %s\n",
				dfile);
			exit(1);
		}
		
		/* Check that data is consistent */
		if (lv_check_consistency(lv) < 0) {
			fprintf(stderr, "Logical volume %s is inconsistent\n",
				dfile);
			exit(1);
		}
		
		/* Update sizes */
		*sunit = lv->lv_stripesize;
		*swidth = lv->lv_stripes * lv->lv_stripesize;
		
		return 1;
	}
#endif /* HAVE_LIBLVM */
	return 0;
}
