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

static cmdinfo_t pwrite_cmd;

static void
pwrite_help(void)
{
	printf(_(
"\n"
" writes a range of bytes (in block size increments) from the given offset\n"
"\n"
" Example:\n"
" 'pwrite 512 20' - writes 20 bytes at 512 bytes into the open file\n"
"\n"
" Writes into a segment of the currently open file, using either a buffer\n"
" filled with a set pattern (0xcdcdcdcd) or data read from an input file.\n"
" -S -- use an alternate seed number\n"
" -i -- specifies an input file from which to source data to write\n"
" -d -- open the input file for direct IO\n"
" -s -- skip a number of bytes at the start of the input file\n"
" The writes are performed in sequential blocks starting at offset, with the\n"
" blocksize tunable using the -b option (default blocksize is 4096 bytes).\n"
"\n"));
}

static int
write_buffer(
	off64_t		offset,
	long long	count,
	ssize_t		bs,
	int		fd,
	off64_t		skip,
	long long	*total)
{
	ssize_t		bytes, bytes_requested;
	long long	bar = min(bs, count);
	int		ops = 0;

	*total = 0;
	while (count > 0) {
		if (fd > 0) {	/* input file given, read buffer first */
			if (read_buffer(fd, skip + *total, bs, &bar, 0, 1) < 0)
				break;
		}
		bytes_requested = min(bar, count);
		bytes = pwrite64(file->fd, buffer, bytes_requested, offset);
		if (bytes == 0)
			break;
		if (bytes < 0) {
			perror("pwrite64");
			return -1;
		}
		ops++;
		*total += bytes;
		if (bytes < bytes_requested)
			break;
		offset += bytes;
		count -= bytes;
	}
	return ops;
}

static int
pwrite_f(
	int		argc,
	char		**argv)
{
	off64_t		offset, skip = 0;
	long long	count, total;
	unsigned int	seed = 0xcdcdcdcd;
	unsigned int	blocksize, sectsize;
	struct timeval	t1, t2;
	char		s1[64], s2[64], ts[64];
	char		*sp, *infile = NULL;
	int		c, fd = -1, uflag = 0, dflag = 0;

	init_cvtnum(&blocksize, &sectsize);
	while ((c = getopt(argc, argv, "b:df:i:s:S:u")) != EOF) {
		switch (c) {
		case 'b':
			blocksize = cvtnum(blocksize, sectsize, optarg);
			if (blocksize < 0) {
				printf(_("non-numeric bsize -- %s\n"), optarg);
				return 0;
			}
			break;
		case 'd':
			dflag = 1;
			break;
		case 'f':
		case 'i':
			infile = optarg;
			break;
		case 's':
			skip = cvtnum(blocksize, sectsize, optarg);
			if (skip < 0) {
				printf(_("non-numeric skip -- %s\n"), optarg);
				return 0;
			}
			break;
		case 'S':
			seed = strtoul(optarg, &sp, 0);
			if (!sp || sp == optarg) {
				printf(_("non-numeric seed -- %s\n"), optarg);
				return 0;
			}
			break;
		case 'u':
			uflag = 1;
			break;
		default:
			return command_usage(&pwrite_cmd);
		}
	}
	if ( ((skip || dflag) && !infile) || (optind != argc - 2))
		return command_usage(&pwrite_cmd);
	offset = cvtnum(blocksize, sectsize, argv[optind]);
	if (offset < 0) {
		printf(_("non-numeric offset argument -- %s\n"), argv[optind]);
		return 0;
	}
	optind++;
	count = cvtnum(blocksize, sectsize, argv[optind]);
	if (count < 0) {
		printf(_("non-numeric length argument -- %s\n"), argv[optind]);
		return 0;
	}

	if (alloc_buffer(blocksize, uflag, seed) < 0)
		return 0;

	c = IO_READONLY | (dflag ? IO_DIRECT : 0);
	if (infile && ((fd = openfile(infile, NULL, c, 0)) < 0))
		return 0;

	gettimeofday(&t1, NULL);
	c = write_buffer(offset, count, blocksize, fd, skip, &total);
	if (c < 0) {
		close(fd);
		return 0;
	}
	gettimeofday(&t2, NULL);
	t2 = tsub(t2, t1);

	printf(_("wrote %lld/%lld bytes at offset %lld\n"),
		total, count, (long long)offset);
	cvtstr((double)total, s1, sizeof(s1));
	cvtstr(tdiv((double)total, t2), s2, sizeof(s2));
	timestr(&t2, ts, sizeof(ts));
	printf(_("%s, %d ops; %s (%s/sec and %.4f ops/sec)\n"),
		s1, c, ts, s2, tdiv((double)c, t2));
	close(fd);
	return 0;
}

void
pwrite_init(void)
{
	pwrite_cmd.name = _("pwrite");
	pwrite_cmd.altname = _("w");
	pwrite_cmd.cfunc = pwrite_f;
	pwrite_cmd.argmin = 2;
	pwrite_cmd.argmax = -1;
	pwrite_cmd.flags = CMD_NOMAP_OK | CMD_FOREIGN_OK;
	pwrite_cmd.args =
		_("[-i infile [-d] [-s skip]] [-b bs] [-S seed] off len");
	pwrite_cmd.oneline =
		_("writes a number of bytes at a specified offset");
	pwrite_cmd.help = pwrite_help;

	add_command(&pwrite_cmd);
}
