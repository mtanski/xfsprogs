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
#include <ctype.h>
#include "command.h"
#include "input.h"
#include "init.h"
#include "io.h"

static cmdinfo_t pread_cmd;

static void
pread_help(void)
{
	printf(_(
"\n"
" reads a range of bytes in a specified block size from the given offset\n"
"\n"
" Example:\n"
" 'pread -v 512 20' - dumps 20 bytes read from 512 bytes into the file\n"
"\n"
" Reads a segment of the currently open file, optionally dumping it to the\n"
" standard output stream (with -v option) for subsequent inspection.\n"
" The reads are performed in sequential blocks starting at offset, with the\n"
" blocksize tunable using the -b option (default blocksize is 4096 bytes).\n"
"\n"));
}

void	*buffer;
ssize_t	buffersize;

int
alloc_buffer(
	ssize_t		bsize,
	int		uflag,
	unsigned int	seed)
{
	if (bsize > buffersize) {
		if (buffer)
			free(buffer);
		buffer = memalign(pagesize, buffersize = bsize);
		if (!buffer) {
			perror("memalign");
			buffersize = 0;
			return -1;
		}
	}
	if (!uflag)
		memset(buffer, seed, buffersize);
	return 0;
}

void
dump_buffer(
	off64_t		offset,
	ssize_t		len)
{
	int		i, j;
	char		*p;

	for (i = 0, p = (char *)buffer; i < len; i += 16) {
		char	*s = p;

		printf("%08llx:  ", (unsigned long long)offset + i);
		for (j = 0; j < 16 && i + j < len; j++, p++)
			printf("%02x ", *p);
		printf(" ");
		for (j = 0; j < 16 && i + j < len; j++, s++) {
			if (isalnum((int)*s))
				printf("%c", *s);
			else
				printf(".");
		}
		printf("\n");
	}
}

int
read_buffer(
	int		fd,
	off64_t		offset,
	long long	count,
	long long	*total,
	int		verbose,
	int		onlyone)
{
	ssize_t		bytes, bytes_requested;
	int		ops = 0;

	*total = 0;
	while (count > 0) {
		bytes_requested = min(count, buffersize);
		bytes = pread64(fd, buffer, bytes_requested, offset);
		if (bytes == 0)
			break;
		if (bytes < 0) {
			perror("pread64");
			return -1;
		}
		ops++;
		if (verbose)
			dump_buffer(offset, bytes);
		*total += bytes;
		if (onlyone || bytes < bytes_requested)
			break;
		offset += bytes;
		count -= bytes;
	}
	return ops;
}

static int
pread_f(
	int		argc,
	char		**argv)
{
	off64_t		offset;
	long long	count, total;
	unsigned int	blocksize, sectsize;
	struct timeval	t1, t2;
	char		s1[64], s2[64], ts[64];
	int		uflag = 0, vflag = 0;
	int		c;

	init_cvtnum(&blocksize, &sectsize);
	while ((c = getopt(argc, argv, "b:uv")) != EOF) {
		switch (c) {
		case 'b':
			blocksize = cvtnum(blocksize, sectsize, optarg);
			if (blocksize < 0) {
				printf(_("non-numeric bsize -- %s\n"), optarg);
				return 0;
			}
			break;
		case 'u':
			uflag = 1;
			break;
		case 'v':
			vflag = 1;
			break;
		default:
			return command_usage(&pread_cmd);
		}
	}
	if (optind != argc - 2)
		return command_usage(&pread_cmd);

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

	if (alloc_buffer(blocksize, uflag, 0xabababab) < 0)
		return 0;

	gettimeofday(&t1, NULL);
	if ((c = read_buffer(file->fd, offset, count, &total, vflag, 0)) < 0)
		return 0;
	gettimeofday(&t2, NULL);
	t2 = tsub(t2, t1);

	printf(_("read %lld/%lld bytes at offset %lld\n"),
		total, count, (long long)offset);
	cvtstr((double)total, s1, sizeof(s1));
	cvtstr(tdiv((double)total, t2), s2, sizeof(s2));
	timestr(&t2, ts, sizeof(ts));
	printf(_("%s, %d ops; %s (%s/sec and %.4f ops/sec)\n"),
		s1, c, ts, s2, tdiv((double)c, t2));
	return 0;
}

void
pread_init(void)
{
	pread_cmd.name = _("pread");
	pread_cmd.altname = _("r");
	pread_cmd.cfunc = pread_f;
	pread_cmd.argmin = 2;
	pread_cmd.argmax = -1;
	pread_cmd.flags = CMD_NOMAP_OK | CMD_FOREIGN_OK;
	pread_cmd.args = _("[-b bs] [-v] off len");
	pread_cmd.oneline = _("reads a number of bytes at a specified offset");
	pread_cmd.help = pread_help;

	add_command(&pread_cmd);
}
