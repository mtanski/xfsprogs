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

#include <xfs/libxfs.h>
#include <sys/mman.h>
#include "command.h"
#include "input.h"
#include "init.h"
#include "io.h"

static cmdinfo_t file_cmd;
static cmdinfo_t print_cmd;

fileio_t	*filetable;
int		filecount;
fileio_t	*file;

static void
print_fileio(
	fileio_t	*file,
	int		index,
	int		braces)
{
	printf(_("%c%d%c %-14s (%s,%s,%s,%s%s%s)\n"),
		braces? '[' : ' ', index, braces? ']' : ' ', file->name,
		file->flags & IO_FOREIGN ? _("foreign") : _("xfs"),
		file->flags & IO_OSYNC ? _("sync") : _("non-sync"),
		file->flags & IO_DIRECT ? _("direct") : _("non-direct"),
		file->flags & IO_READONLY ? _("read-only") : _("read-write"),
		file->flags & IO_REALTIME ? _(",real-time") : "",
		file->flags & IO_APPEND ? _(",append-only") : "");
}

int
filelist_f(void)
{
	int		i;

	for (i = 0; i < filecount; i++)
		print_fileio(&filetable[i], i, &filetable[i] == file);
	return 0;
}

static int
print_f(
	int		argc,
	char		**argv)
{
	filelist_f();
	maplist_f();
	return 0;
}

static int
file_f(
	int		argc,
	char		**argv)
{
	int		i;

	if (argc <= 1)
		return filelist_f();
	i = atoi(argv[1]);
	if (i < 0 || i >= filecount) {
		printf(_("value %d is out of range (0-%d)\n"), i, filecount-1);
	} else {
		file = &filetable[i];
		filelist_f();
	}
	return 0;
}

void
file_init(void)
{
	file_cmd.name = _("file");
	file_cmd.altname = _("f");
	file_cmd.args = _("[N]");
	file_cmd.cfunc = file_f;
	file_cmd.argmin = 0;
	file_cmd.argmax = 1;
	file_cmd.flags = CMD_NOMAP_OK | CMD_FOREIGN_OK;
	file_cmd.oneline = _("set the current file");

	print_cmd.name = _("print");
	print_cmd.altname = _("p");
	print_cmd.cfunc = print_f;
	print_cmd.argmin = 0;
	print_cmd.argmax = 0;
	print_cmd.flags = CMD_NOMAP_OK | CMD_NOFILE_OK | CMD_FOREIGN_OK;
	print_cmd.oneline = _("list current open files and memory mappings");

	add_command(&file_cmd);
	add_command(&print_cmd);
}
