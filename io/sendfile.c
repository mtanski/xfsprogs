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
#include <sys/sendfile.h>
#include "command.h"
#include "input.h"
#include "init.h"
#include "io.h"

static cmdinfo_t sendfile_cmd;

static void
sendfile_help(void)
{
	printf(_(
"\n"
" transfer a range of bytes from the given offset between files\n"
"\n"
" Example:\n"
" 'send -f 2 512 20' - writes 20 bytes at 512 bytes into the open file\n"
"\n"
" Copies data between one file descriptor and another.  Because this copying\n"
" is done within the kernel, sendfile does not need to transfer data to and\n"
" from user space.\n"
" -f -- specifies an input file from which to source data to write\n"
" -i -- specifies an input file name from which to source data to write.\n"
" An offset and length in the source file can be optionally specified.\n"
"\n"));
}

static int
send_buffer(
	off64_t		offset,
	size_t		count,
	int		fd,
	long long	*total)
{
	off64_t		off = offset;
	ssize_t		bytes, bytes_remaining = count;
	int		ops = 0;

	*total = 0;
	while (count > 0) {
		bytes = sendfile64(file->fd, fd, &off, bytes_remaining);
		if (bytes == 0)
			break;
		if (bytes < 0) {
			perror("sendfile64");
			return -1;
		}
		ops++;
		*total += bytes;
		if (bytes >= bytes_remaining)
			break;
		bytes_remaining -= bytes;
	}
	return ops;
}

static int
sendfile_f(
	int		argc,
	char		**argv)
{
	off64_t		offset = 0;
	long long	count, total;
	unsigned int	blocksize, sectsize;
	struct timeval	t1, t2;
	char		s1[64], s2[64], ts[64];
	char		*infile = NULL;
	int		c, fd = -1;

	init_cvtnum(&blocksize, &sectsize);
	while ((c = getopt(argc, argv, "f:i:")) != EOF) {
		switch (c) {
		case 'f':
			fd = atoi(argv[1]);
			if (fd < 0 || fd >= filecount) {
				printf(_("value %d is out of range (0-%d)\n"),
					fd, filecount-1);
				return 0;
			}
			break;
		case 'i':
			infile = optarg;
			break;
		default:
			return command_usage(&sendfile_cmd);
		}
	}
	if (infile && fd != -1)
		return command_usage(&sendfile_cmd);

	if (!infile)
		fd = filetable[fd].fd;
	else if ((fd = openfile(infile, NULL, IO_READONLY, 0)) < 0)
		return 0;

	if (optind == argc - 2) {
		offset = cvtnum(blocksize, sectsize, argv[optind]);
		if (offset < 0) {
			printf(_("non-numeric offset argument -- %s\n"),
				argv[optind]);
			goto done;
		}
		optind++;
		count = cvtnum(blocksize, sectsize, argv[optind]);
		if (count < 0) {
			printf(_("non-numeric length argument -- %s\n"),
				argv[optind]);
			goto done;
		}
	} else {
		struct stat64	stat;

		if (fstat64(fd, &stat) < 0) {
			perror("fstat64");
			goto done;
		}
		count = stat.st_size;
	}

	gettimeofday(&t1, NULL);
	c = send_buffer(offset, count, fd, &total);
	if (c < 0)
		goto done;
	gettimeofday(&t2, NULL);
	t2 = tsub(t2, t1);

	printf(_("sent %lld/%lld bytes from offset %lld\n"),
		total, count, (long long)offset);
	cvtstr((double)total, s1, sizeof(s1));
	cvtstr(tdiv((double)total, t2), s2, sizeof(s2));
	timestr(&t2, ts, sizeof(ts));
	printf(_("%s, %d ops; %s (%s/sec and %.4f ops/sec)\n"),
		s1, c, ts, s2, tdiv((double)c, t2));
done:
	if (infile)
		close(fd);
	return 0;
}

void
sendfile_init(void)
{
	sendfile_cmd.name = _("sendfile");
	sendfile_cmd.altname = _("send");
	sendfile_cmd.cfunc = sendfile_f;
	sendfile_cmd.argmin = 2;
	sendfile_cmd.argmax = 4;
	sendfile_cmd.flags = CMD_NOMAP_OK | CMD_FOREIGN_OK;
	sendfile_cmd.args =
		_("-i infile | -f N [off len]");
	sendfile_cmd.oneline =
		_("Transfer data directly between file descriptors");
	sendfile_cmd.help = sendfile_help;

	add_command(&sendfile_cmd);
}
