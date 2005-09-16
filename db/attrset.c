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

#include <xfs/libxfs.h>
#include "command.h"
#include "attrset.h"
#include "io.h"
#include "output.h"
#include "type.h"
#include "init.h"
#include "inode.h"
#include "malloc.h"

static int		attr_set_f(int argc, char **argv);
static int		attr_remove_f(int argc, char **argv);
static void		attrset_help(void);

static const cmdinfo_t	attr_set_cmd =
	{ "attr_set", "aset", attr_set_f, 1, -1, 0,
	  "[-r|-s|-p|-u] [-R|-C] [-v n] name",
	  "set the named attribute on the current inode", attrset_help };
static const cmdinfo_t	attr_remove_cmd =
	{ "attr_remove", "aremove", attr_remove_f, 1, -1, 0,
	  "[-r|-s|-p|-u] name",
	  "remove the named attribute from the current inode", attrset_help };

static void
attrset_help(void)
{
	dbprintf(
"\n"
" The 'attr_set' and 'attr_remove' commands provide interfaces for debugging\n"
" the extended attribute allocation and removal code.\n"
" Both commands require an attribute name to be specified, and the attr_set\n"
" command allows an optional value length (-v) to be provided as well.\n"
" There are 4 namespace flags:\n"
"  -r -- 'root'\n"
"  -u -- 'user'		(default)\n"
"  -s -- 'secure'\n"
"\n"
" For attr_set, these options further define the type of set:\n"
"  -C -- 'create'    - create attribute, fail if it already exists\n"
"  -R -- 'replace'   - replace attribute, fail if it does not exist\n"
"\n");
}

void
attrset_init(void)
{
	if (!expert_mode)
		return;

	add_command(&attr_set_cmd);
	add_command(&attr_remove_cmd);
}

static int
attr_set_f(
	int		argc,
	char		**argv)
{
	xfs_inode_t	*ip = NULL;
	char		*name, *value, *sp;
	int		c, namelen, valuelen = 0, flags = 0;

	if (cur_typ == NULL) {
		dbprintf("no current type\n");
		return 0;
	}
	if (cur_typ->typnm != TYP_INODE) {
		dbprintf("current type is not inode\n");
		return 0;
	}

	while ((c = getopt(argc, argv, "rusCRv:")) != EOF) {
		switch (c) {
		/* namespaces */
		case 'r':
			flags |= LIBXFS_ATTR_ROOT;
			flags &= ~LIBXFS_ATTR_SECURE;
			break;
		case 'u':
			flags &= ~(LIBXFS_ATTR_ROOT | LIBXFS_ATTR_SECURE);
			break;
		case 's':
			flags |= LIBXFS_ATTR_SECURE;
			flags &= ~LIBXFS_ATTR_ROOT;
			break;

		/* modifiers */
		case 'C':
			flags |= LIBXFS_ATTR_CREATE;
			break;
		case 'R':
			flags |= LIBXFS_ATTR_REPLACE;
			break;

		/* value length */
		case 'v':
			valuelen = (int)strtol(optarg, &sp, 0);
			if (*sp != '\0' || valuelen < 0 || valuelen > 64*1024) {
				dbprintf("bad attr_set valuelen %s\n", optarg);
				return 0;
			}
			break;

		default:
			dbprintf("bad option for attr_set command\n");
			return 0;
		}
	}

	if (optind != argc - 1) {
		dbprintf("too few options for attr_set (no name given)\n");
		return 0;
	}

	name = argv[optind];
	namelen = strlen(name);

	if (valuelen) {
		value = (char *)memalign(getpagesize(), valuelen);
		if (!value) {
			dbprintf("cannot allocate buffer (%d)\n", valuelen);
			goto out;
		}
		memset(value, 0xfeedface, valuelen);
	} else {
		value = NULL;
	}

	if (libxfs_iget(mp, NULL, iocur_top->ino, 0, &ip, 0)) {
		dbprintf(_("failed to iget inode %llu\n"),
			(unsigned long long)iocur_top->ino);
		goto out;
	}

	if (libxfs_attr_set_int(ip, name, namelen, value, valuelen, flags)) {
		dbprintf(_("failed to set attr %s on inode %llu\n"),
			name, (unsigned long long)iocur_top->ino);
		goto out;
	}

	/* refresh with updated inode contents */
	set_cur_inode(iocur_top->ino);

out:
	if (ip)
		libxfs_iput(ip, 0);
	if (value)
		free(value);
	return 0;
}

static int
attr_remove_f(
	int		argc,
	char		**argv)
{
	xfs_inode_t	*ip = NULL;
	char		*name;
	int		c, namelen, flags = 0;

	if (cur_typ == NULL) {
		dbprintf("no current type\n");
		return 0;
	}
	if (cur_typ->typnm != TYP_INODE) {
		dbprintf("current type is not inode\n");
		return 0;
	}

	while ((c = getopt(argc, argv, "rus")) != EOF) {
		switch (c) {
		/* namespaces */
		case 'r':
			flags |= LIBXFS_ATTR_ROOT;
			flags &= ~LIBXFS_ATTR_SECURE;
			break;
		case 'u':
			flags &= ~(LIBXFS_ATTR_ROOT | LIBXFS_ATTR_SECURE);
			break;
		case 's':
			flags |= LIBXFS_ATTR_SECURE;
			flags &= ~LIBXFS_ATTR_ROOT;
			break;

		default:
			dbprintf("bad option for attr_remove command\n");
			return 0;
		}
	}

	if (optind != argc - 1) {
		dbprintf("too few options for attr_remove (no name given)\n");
		return 0;
	}

	name = argv[optind];
	namelen = strlen(name);

	if (libxfs_iget(mp, NULL, iocur_top->ino, 0, &ip, 0)) {
		dbprintf(_("failed to iget inode %llu\n"),
			(unsigned long long)iocur_top->ino);
		goto out;
	}

	if (libxfs_attr_remove_int(ip, name, namelen, flags)) {
		dbprintf(_("failed to remove attr %s from inode %llu\n"),
			name, (unsigned long long)iocur_top->ino);
		goto out;
	}

	/* refresh with updated inode contents */
	set_cur_inode(iocur_top->ino);

out:
	if (ip)
		libxfs_iput(ip, 0);
	return 0;
}
