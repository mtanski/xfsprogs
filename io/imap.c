/*
 * Copyright (c) 2001-2003,2005 Silicon Graphics, Inc.  All Rights Reserved.
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

#if defined(__sgi__)
# define ino64p		ino64_t *
#else
# define ino64p		__u64 *
#endif

static cmdinfo_t imap_cmd;

int
imap_f(int argc, char **argv)
{
	int		count;
	int		nent;
	int		i;
	__s64		last = 0;
	xfs_inogrp_t	*t;
	xfs_fsop_bulkreq_t bulkreq;

	if (argc != 2)
		nent = 1;
	else
		nent = atoi(argv[1]);

	t = malloc(nent * sizeof(*t));

	bulkreq.lastip  = (ino64p)&last;
	bulkreq.icount  = nent;
	bulkreq.ubuffer = (void *)t;
	bulkreq.ocount  = &count;

	while (xfsctl(file->name, file->fd, XFS_IOC_FSINUMBERS, &bulkreq) == 0) {
		if (count == 0)
			return 0;
		for (i = 0; i < count; i++) {
			printf(_("ino %10llu count %2d mask %016llx\n"),
				(unsigned long long)t[i].xi_startino,
				t[i].xi_alloccount,
				(unsigned long long)t[i].xi_allocmask);
		}
	}
	perror("xfsctl(XFS_IOC_FSINUMBERS)");
	exitcode = 1;
	return 0;
}

void
imap_init(void)
{
	imap_cmd.name = _("imap");
	imap_cmd.cfunc = imap_f;
	imap_cmd.argmin = 0;
	imap_cmd.argmax = 0;
	imap_cmd.args = _("[nentries]");
	imap_cmd.flags = CMD_NOMAP_OK;
	imap_cmd.oneline = _("inode map for filesystem of current file");

	if (expert)
		add_command(&imap_cmd);
}
