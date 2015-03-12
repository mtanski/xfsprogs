/*
 * Copyright (c) 2015 Milosz Tanski <mtanski@gmail.com>
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write the Free Software Foundation,
 * Inc.,  51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <xfs/xfs.h>
#include <xfs/command.h>
#include <xfs/input.h>
#include <sys/mman.h>
#include "init.h"
#include "io.h"

static cmdinfo_t mlock_cmd;
static cmdinfo_t munlock_cmd;

static void
mlock_help(void)
{
	printf(_(
"\n"
" lock part of the file mapping in the page cache\n"
"\n"
" Modifies page cache behavior when operating on the current mapping.\n"
" If the range arguments are ommitted, it will atempt to lock the whole\n"
" file in the page cache.\n"
" Notes:\n"
"  Can fail easily on large map locks due to rlimites.\n"
"\n"));
}

int
mlock_f(
	int		argc,
	char		**argv)
{
	off64_t		offset, llength;
	size_t		length;
	void		*start;
	size_t		blocksize, sectsize;

	if (argc == 1) {
		offset = mapping->offset;
		length = mapping->length;
	} else if (argc == 3) {
		init_cvtnum(&blocksize, &sectsize);
		offset = cvtnum(blocksize, sectsize, argv[1]);
		if (offset < 0) {
			printf(_("non-numeric offset argument -- %s\n"),
				argv[1]);
			return 0;
		}
		llength = cvtnum(blocksize, sectsize, argv[2]);
		if (llength < 0) {
			printf(_("non-numeric length argument -- %s\n"),
				argv[2]);
			return 0;
		} else if (llength > (size_t)llength) {
			printf(_("length argument too large -- %lld\n"),
				(long long)llength);
			return 0;
		} else
			length = (size_t)llength;
	} else {
		return command_usage(&mlock_cmd);
	}

	start = check_mapping_range(mapping, offset, length, 1);
	if (!start)
		return 0;

	if (mlock(start, length) == -1)
		perror("mlock");

	return 0;
}

static void
munlock_help(void)
{
	printf(_(
"\n"
" unlock previously locked file mapping in the page cache\n"
"\n"
" Modifies page cache behavior when operating on the current mapping.\n"
" If the range arguments are ommitted, it will atempt to lock the whole\n"
" file in the page cache.\n"
"\n"));
}

int
munlock_f(
	int		argc,
	char		**argv)
{
	off64_t		offset, llength;
	size_t		length;
	void		*start;
	size_t		blocksize, sectsize;

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
		llength = cvtnum(blocksize, sectsize, argv[optind]);
		if (llength < 0) {
			printf(_("non-numeric length argument -- %s\n"),
				argv[optind]);
			return 0;
		} else if (llength > (size_t)llength) {
			printf(_("length argument too large -- %lld\n"),
				(long long)llength);
			return 0;
		} else
			length = (size_t)llength;
	} else {
		return command_usage(&munlock_cmd);
	}

	start = check_mapping_range(mapping, offset, length, 1);
	if (!start)
		return 0;

	if (munlock(start, length) == -1)
		perror("munlock");

	return 0;
}

void
mlock_init(void)
{
	mlock_cmd.name = "mlock";
	mlock_cmd.altname = "ml";
	mlock_cmd.cfunc = mlock_f;
	mlock_cmd.argmin = 0;
	mlock_cmd.argmax = 2;
	mlock_cmd.flags = CMD_NOFILE_OK | CMD_FOREIGN_OK;
	mlock_cmd.args = _("[off len]");
	mlock_cmd.oneline =
		_("alters the size of the current memory mapping");
	mlock_cmd.help = mlock_help;

	munlock_cmd.name = "munlock";
	munlock_cmd.altname = "mul";
	munlock_cmd.cfunc = munlock_f;
	munlock_cmd.argmin = 0;
	munlock_cmd.argmax = 2;
	munlock_cmd.flags = CMD_NOFILE_OK | CMD_FOREIGN_OK;
	munlock_cmd.args = _("[off len]");
	munlock_cmd.oneline =
		_("alters the size of the current memory mapping");
	munlock_cmd.help = munlock_help;

	add_command(&mlock_cmd);
	add_command(&munlock_cmd);
}
