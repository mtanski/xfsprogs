/*
 * Copyright (c) 2004 Silicon Graphics, Inc.  All Rights Reserved.
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

#include "drivers.h"

int
mnt_is_dm_subvol(
	dev_t		dev)
{
	return get_driver_block_major("device-mapper", major(dev));
}

int
dm_get_subvol_stripe(
	char		*dfile,
	sv_type_t	type,
	int		*sunit,
	int		*swidth,
	struct stat64	*sb)
{
	int		count, stripes = 0, stripesize = 0;
	int		dmpipe[2];
	char		*dpath, *largv[4], tmppath[PATH_MAX];
	FILE		*stream;
	long long	offset, size;
	static char	*command = "table";	/* dmsetup table /dev/xxx */

	if (!mnt_is_dm_subvol(sb->st_rdev))
		return 0;

	/* Quest for dmsetup */
	if (!access("/usr/local/sbin/dmsetup", R_OK|X_OK))
		largv[0] = "/usr/local/sbin/dmsetup";
	else if (!access("/usr/sbin/dmsetup", R_OK|X_OK))
		largv[0] = "/usr/sbin/dmsetup";
	else if (!access("/sbin/dmsetup", R_OK|X_OK))
		largv[0] = "/sbin/dmsetup";
	else {
		fprintf(stderr,
	_("Warning - device mapper device, but no dmsetup(8) found\n"));
		return 0;
	}

	if (!(dpath = realpath(dfile, tmppath))) {
		fprintf(stderr,
	_("Warning - device mapper device, but cannot resolve path %s: %s\n"),
			dfile, strerror(errno));
		return 0;
	}

	largv[1] = command;
	largv[2] = dpath;
	largv[3] = NULL;

	/* Open pipe */
	if (pipe(dmpipe) < 0) {
		fprintf(stderr, _("Could not open pipe\n"));
		exit(1);
	}

	/* Spawn dmsetup */
	switch (fork()) {
	case 0:
		/* Plumbing */
		close(dmpipe[0]);

		if (dmpipe[1] != STDOUT_FILENO)
			dup2(dmpipe[1], STDOUT_FILENO);

		execv(largv[0], largv);

		fprintf(stderr, _("Failed to execute %s\n"), largv[0]);
		exit(1);

	case -1:
		fprintf(stderr, _("Failed forking dmsetup process\n"));
		exit(1);

	default:
		break;
	}

	close(dmpipe[1]);
	stream = fdopen(dmpipe[0], "r");
	count = fscanf(stream, "%lld %lld striped %d %d ",
			&offset, &size, &stripes, &stripesize);
	fclose(stream);
	if (count != 4)
		return 0;

	/* Update sizes */
	*sunit = stripesize;
	*swidth = (stripes * stripesize);
	return 1;
}
