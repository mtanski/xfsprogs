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
#include <fcntl.h>
#include <mntent.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/param.h>
#include "mountinfo.h"

int
mnt_check_init(mnt_check_state_t **check_state)
{
	return(0);
}

int
mnt_find_mount_conflicts(mnt_check_state_t *check_state, char *devname)
{
#define PROC_MOUNTED	"/proc/mounts"
	int		sts = 0;
	FILE		*f;
	struct mntent	*mnt;
	struct stat64	ns, ms;
	char		mounts[MAXPATHLEN];

	if (stat64(devname, &ns) < 0)
		return sts;

	strcpy(mounts, access(PROC_MOUNTED, R_OK)? PROC_MOUNTED : MOUNTED);
	if ((f = setmntent(mounts, "r")) == NULL)
		return sts;
	while ((mnt = getmntent(f)) != NULL) {
		if (stat64(mnt->mnt_fsname, &ms) < 0)
			continue;
		if (S_ISBLK(ms.st_mode) && ns.st_rdev == ms.st_rdev)
			break;
		if (!S_ISBLK(ms.st_mode) && !strcmp(devname, mnt->mnt_fsname))
			break;
	}
	endmntent(f);
	if (mnt)
		sts = 1;
	return sts;
}

int
mnt_find_mounted_partitions(mnt_check_state_t *check_state, char *devname)
{
	return 0;
}

int
mnt_causes_test(mnt_check_state_t *check_state, int cause)
{
	switch(cause) {
	case MNT_CAUSE_MOUNTED:
		return(1);

	default:
		fprintf(stderr, "mnt_causes_test: unknown cause %d\n", cause);
		exit(99);
	}
}

void
mnt_causes_show(mnt_check_state_t *check_state, FILE *fp, char *prefix)
{
	fprintf(fp, "mnt_causes_show: not implemented. Called with %s\n",
		prefix);
	exit(98);
}

void
mnt_plist_show(mnt_check_state_t *check_state, FILE *fp, char *prefix)
{
	/*
	 * Need to do some work for volume mgmt.
	 */
}

int
mnt_check_end(mnt_check_state_t *check_state)
{
	return(0);
}
