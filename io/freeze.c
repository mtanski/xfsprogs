/*
 * Copyright (c) 2001-2004 Silicon Graphics, Inc.  All Rights Reserved.
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

#include <xfs/libxfs.h>
#include "command.h"
#include "input.h"
#include "init.h"
#include "io.h"

static cmdinfo_t freeze_cmd;
static cmdinfo_t thaw_cmd;

int
freeze_f(
	int		argc,
	char		**argv)
{
	int		level = 1;

	if (xfsctl(file->name, file->fd, XFS_IOC_FREEZE, &level) < 0) {
		fprintf(stderr,
			_("%s: cannot freeze filesystem at %s: %s\n"),
			progname, file->name, strerror(errno));
		exitcode = 1;
		return 0;
	}
	return 0;
}

int
thaw_f(
	int		argc,
	char		**argv)
{
	int		level = 1;

	if (xfsctl(file->name, file->fd, XFS_IOC_THAW, &level) < 0) {
		fprintf(stderr,
			_("%s: cannot unfreeze filesystem mounted at %s: %s\n"),
			progname, file->name, strerror(errno));
		exitcode = 1;
		return 0;
	}
	return 0;
}

void
freeze_init(void)
{
	freeze_cmd.name = _("freeze");
	freeze_cmd.cfunc = freeze_f;
	freeze_cmd.argmin = 0;
	freeze_cmd.argmax = 0;
	freeze_cmd.flags = CMD_NOMAP_OK;
	freeze_cmd.oneline = _("freeze filesystem of current file");

	thaw_cmd.name = _("thaw");
	thaw_cmd.cfunc = thaw_f;
	thaw_cmd.argmin = 0;
	thaw_cmd.argmax = 0;
	thaw_cmd.flags = CMD_NOMAP_OK;
	thaw_cmd.oneline = _("unfreeze filesystem of current file");

	if (expert) {
		add_command(&freeze_cmd);
		add_command(&thaw_cmd);
	}
}
