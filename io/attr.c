/*
 * Copyright (c) 2003-2004 Silicon Graphics, Inc.  All Rights Reserved.
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

static cmdinfo_t chattr_cmd;
static cmdinfo_t lsattr_cmd;

#define CHATTR_XFLAG_LIST	"riasAdtPn"

static struct xflags {
	uint	flag;
	char	*shortname;
	char	*longname;
} xflags[] = {
	{ XFS_XFLAG_REALTIME,	"r", "realtime" },
	{ XFS_XFLAG_PREALLOC,	"p", "prealloc" },
	{ XFS_XFLAG_IMMUTABLE,	"i", "immutable" },
	{ XFS_XFLAG_APPEND,	"a", "append-only" },
	{ XFS_XFLAG_SYNC,	"s", "sync" },
	{ XFS_XFLAG_NOATIME,	"A", "no-atime" },
	{ XFS_XFLAG_NODUMP,	"d", "no-dump" },
	{ XFS_XFLAG_RTINHERIT,	"t", "rt-inherit" },
	{ XFS_XFLAG_PROJINHERIT,"P", "proj-inherit" },
	{ XFS_XFLAG_NOSYMLINKS,	"n", "nosymlinks" },
	{ 0, NULL, NULL }
};

static void
lsattr_help(void)
{
	printf(_(
"\n"
" displays the set of extended inode flags associated with the current file\n"
"\n"
" Each individual flag is displayed as a single character, in this order:\n"
" r -- file data is stored in the realtime section\n"
" p -- file has preallocated extents (cannot be changed using chattr)\n"
" i -- immutable, file cannot be modified\n"
" a -- append-only, file can only be appended to\n"
" s -- all updates are synchronous\n"
" A -- the access time is not updated for this inode\n"
" d -- do not include this file in a dump of the filesystem\n"
" t -- child created in this directory has realtime bit set by default\n"
" P -- child created in this directory has parents project ID by default\n"
" n -- symbolic links cannot be created in this directory\n"
"\n"
" Options:\n"
" -a -- show all flags which can be set alongside those which are set\n"
" -v -- verbose mode; show long names of flags, not single characters\n"
"\n"));
}

static void
chattr_help(void)
{
	printf(_(
"\n"
" modifies the set of extended inode flags associated with the current file\n"
"\n"
" Examples:\n"
" 'chattr +a' - sets the append-only flag\n"
" 'chattr -a' - clears the append-only flag\n"
"\n"
" +/-r -- set/clear the realtime flag\n"
" +/-i -- set/clear the immutable flag\n"
" +/-a -- set/clear the append-only flag\n"
" +/-s -- set/clear the sync flag\n"
" +/-A -- set/clear the no-atime flag\n"
" +/-d -- set/clear the no-dump flag\n"
" +/-t -- set/clear the realtime inheritance flag\n"
" +/-P -- set/clear the project ID inheritance flag\n"
" +/-n -- set/clear the no-symbolic-links flag\n"
" Note1: user must have certain capabilities to modify immutable/append-only.\n"
" Note2: immutable/append-only files cannot be deleted; removing these files\n"
"        requires the immutable/append-only flag to be cleared first.\n"
" Note3: the realtime flag can only be set if the filesystem has a realtime\n"
"        section, and the (regular) file must be empty when the flag is set.\n"
"\n"));
}

void
printxattr(
	uint		flags,
	int		verbose,
	int		dofname,
	const char	*fname,
	int		dobraces,
	int		doeol)
{
	struct xflags	*p;
	int		first = 1;

	if (dobraces)
		fputs("[", stdout);
	for (p = xflags; p->flag; p++) {
		if (flags & p->flag) {
			if (verbose) {
				if (first)
					first = 0;
				else
					fputs(", ", stdout);
				fputs(p->longname, stdout);
			} else {
				fputs(p->shortname, stdout);
			}
		} else if (!verbose) {
			fputs("-", stdout);
		}
	}
	if (dobraces)
		fputs("]", stdout);
	if (dofname)
		printf(" %s ", fname);
	if (doeol)
		fputs("\n", stdout);
}

static int
lsattr_f(
	int		argc,
	char		**argv)
{
	struct fsxattr	fsx;
	char		*name = file->name;
	int		c, aflag = 0, vflag = 0;

	while ((c = getopt(argc, argv, "av")) != EOF) {
		switch (c) {
		case 'a':
			aflag = 1;
			vflag = 0;
			break;
		case 'v':
			aflag = 0;
			vflag = 1;
			break;
		default:
			printf(_("invalid lsattr argument -- '%c'\n"), c);
			return 0;
		}
	}

	if ((xfsctl(name, file->fd, XFS_IOC_FSGETXATTR, &fsx)) < 0) {
		perror("XFS_IOC_FSGETXATTR");
	} else {
		printxattr(fsx.fsx_xflags, vflag, !aflag, name, vflag, !aflag);
		if (aflag) {
			fputs("/", stdout);
			printxattr(-1, 0, 1, name, 0, 1);
		}
	}
	return 0;
}

static int
chattr_f(
	int		argc,
	char		**argv)
{
	struct fsxattr	attr;
	struct xflags	*p;
	unsigned int	i = 0;
	char		*c, *name = file->name;

	if (xfsctl(name, file->fd, XFS_IOC_FSGETXATTR, &attr) < 0) {
		perror("XFS_IOC_FSGETXATTR");
		return 0;
	}
	while (++i < argc) {
		if (argv[i][0] == '+') {
			for (c = &argv[i][1]; *c; c++) {
				for (p = xflags; p->flag; p++) {
					if (strncmp(p->shortname, c, 1) == 0) {
						attr.fsx_xflags |= p->flag;
						break;
					}
				}
				if (!p->flag) {
					fprintf(stderr, _("%s: unknown flag\n"),
						progname);
					return 0;
				}
			}
		} else if (argv[i][0] == '-') {
			for (c = &argv[i][1]; *c; c++) {
				for (p = xflags; p->flag; p++) {
					if (strncmp(p->shortname, c, 1) == 0) {
						attr.fsx_xflags &= ~p->flag;
						break;
					}
				}
				if (!p->flag) {
					fprintf(stderr, _("%s: unknown flag\n"),
						progname);
					return 0;
				}
			}
		} else {
			fprintf(stderr, _("%s: bad chattr command, not +/-X\n"),
				progname);
			return 0;
		}
	}
	if (xfsctl(name, file->fd, XFS_IOC_FSSETXATTR, &attr) < 0)
		perror("XFS_IOC_FSSETXATTR");
	return 0;
}

void
attr_init(void)
{
	chattr_cmd.name = _("chattr");
	chattr_cmd.cfunc = chattr_f;
	chattr_cmd.args = _("[+/-"CHATTR_XFLAG_LIST"]");
	chattr_cmd.argmin = 1;
	chattr_cmd.argmax = -1;
	chattr_cmd.flags = CMD_NOMAP_OK;
	chattr_cmd.oneline =
		_("change extended inode flags on the currently open file");
	chattr_cmd.help = chattr_help;

	lsattr_cmd.name = _("lsattr");
	lsattr_cmd.cfunc = lsattr_f;
	lsattr_cmd.args = _("[-a|-v]");
	lsattr_cmd.argmin = 0;
	lsattr_cmd.argmax = 1;
	lsattr_cmd.flags = CMD_NOMAP_OK;
	lsattr_cmd.oneline =
		_("list extended inode flags set on the currently open file");
	lsattr_cmd.help = lsattr_help;

	add_command(&chattr_cmd);
	add_command(&lsattr_cmd);
}
