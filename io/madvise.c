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

static cmdinfo_t madvise_cmd;

static void
madvise_help(void)
{
	printf(_(
"\n"
" advise the page cache about access patterns expected for a mapping\n"
"\n"
" Modifies page cache behavior when operating on the current mapping.\n"
" The range arguments are required by some advise commands ([*] below).\n"
" With no arguments, the POSIX_MADV_NORMAL advice is implied.\n"
" -d -- don't need these pages (POSIX_MADV_DONTNEED) [*]\n"
" -r -- expect random page references (POSIX_MADV_RANDOM)\n"
" -s -- expect sequential page references (POSIX_MADV_SEQUENTIAL)\n"
" -w -- will need these pages (POSIX_MADV_WILLNEED) [*]\n"
" Notes:\n"
"   NORMAL sets the default readahead setting on the file.\n"
"   RANDOM sets the readahead setting on the file to zero.\n"
"   SEQUENTIAL sets double the default readahead setting on the file.\n"
"   WILLNEED forces the maximum readahead.\n"
"\n"));
}

int
madvise_f(
	int		argc,
	char		**argv)
{
	off64_t		offset;
	size_t		length;
	void		*start;
	int		advise = MADV_NORMAL, c;
	unsigned int	blocksize, sectsize;

	while ((c = getopt(argc, argv, "drsw")) != EOF) {
		switch (c) {
		case 'd':	/* Don't need these pages */
			advise = MADV_DONTNEED;
			break;
		case 'r':	/* Expect random page references */
			advise = MADV_RANDOM;
			break;
		case 's':	/* Expect sequential page references */
			advise = MADV_SEQUENTIAL;
			break;
		case 'w':	/* Will need these pages */
			advise = MADV_WILLNEED;
			break;
		default:
			return command_usage(&madvise_cmd);
		}
	}

	if (optind == argc) {
		offset = mapping->offset;
		length = mapping->length;
	} else if (optind == argc - 2) {
		init_cvtnum(&blocksize, &sectsize);
		offset = cvtnum(blocksize, sectsize, argv[optind]);
		if (offset < 0) {
			printf(_("non-numeric offset argument -- %s\n"),
				argv[optind]);
			return 0;
		}
		optind++;
		length = cvtnum(blocksize, sectsize, argv[optind]);
		if (length < 0) {
			printf(_("non-numeric length argument -- %s\n"),
				argv[optind]);
			return 0;
		}
	} else {
		return command_usage(&madvise_cmd);
	}

	start = check_mapping_range(mapping, offset, length, 1);
	if (!start)
		return 0;

	if (madvise(start, length, advise) < 0) {
		perror("madvise");
		return 0;
	}
	return 0;
}

void
madvise_init(void)
{
	madvise_cmd.name = _("madvise");
	madvise_cmd.altname = _("ma");
	madvise_cmd.cfunc = madvise_f;
	madvise_cmd.argmin = 0;
	madvise_cmd.argmax = -1;
	madvise_cmd.flags = CMD_NOFILE_OK | CMD_FOREIGN_OK;
	madvise_cmd.args = _("[-drsw] [off len]");
	madvise_cmd.oneline = _("give advice about use of memory");
	madvise_cmd.help = madvise_help;

	add_command(&madvise_cmd);
}
