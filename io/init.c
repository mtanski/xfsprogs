/*
 * Copyright (c) 2003-2005 Silicon Graphics, Inc.  All Rights Reserved.
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
#include <xfs/command.h>
#include <xfs/input.h>
#include "init.h"
#include "io.h"

char	*progname;
int	exitcode;
int	expert;
size_t	pagesize;
struct timeval stopwatch;

void
usage(void)
{
	fprintf(stderr,
		_("Usage: %s [-adFfmrRstx] [-p prog] [-c cmd]... file\n"),
		progname);
	exit(1);
}

void
init_cvtnum(
	unsigned int	*blocksize,
	unsigned int	*sectsize)
{
	if (!file || (file->flags & IO_FOREIGN)) {
		*blocksize = 4096;
		*sectsize = 512;
	} else {
		*blocksize = file->geom.blocksize;
		*sectsize = file->geom.sectsize;
	}
}

static void
init_commands(void)
{
	attr_init();
	bmap_init();
	fadvise_init();
	file_init();
	freeze_init();
	fsync_init();
	getrusage_init();
	help_init();
	imap_init();
	inject_init();
	mmap_init();
	open_init();
	pread_init();
	prealloc_init();
	pwrite_init();
	quit_init();
	resblks_init();
	sendfile_init();
	shutdown_init();
	truncate_init();
}

static int
init_args_command(
	int	index)
{
	if (index >= filecount)
		return 0;
	file = &filetable[index++];
	return index;
}

static int
init_check_command(
	const cmdinfo_t	*ct)
{
	if (!file && !(ct->flags & CMD_NOFILE_OK)) {
		fprintf(stderr, _("no files are open, try 'help open'\n"));
		return 0;
	}
	if (!mapping && !(ct->flags & CMD_NOMAP_OK)) {
		fprintf(stderr, _("no mapped regions, try 'help mmap'\n"));
		return 0;
	}
	if (file && !(ct->flags & CMD_FOREIGN_OK) &&
					(file->flags & IO_FOREIGN)) {
		fprintf(stderr,
	_("foreign file active, %s command is for XFS filesystems only\n"),
			ct->name);
		return 0;
	}
	return 1;
}

void
init(
	int		argc,
	char		**argv)
{
	int		c, flags = 0;
	char		*sp;
	mode_t		mode = 0600;
	xfs_fsop_geom_t	geometry = { 0 };

	progname = basename(argv[0]);
	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	pagesize = getpagesize();
	gettimeofday(&stopwatch, NULL);

	while ((c = getopt(argc, argv, "ac:dFfmp:nrRstVx")) != EOF) {
		switch (c) {
		case 'a':
			flags |= IO_APPEND;
			break;
		case 'c':
			add_user_command(optarg);
			break;
		case 'd':
			flags |= IO_DIRECT;
			break;
		case 'F':
			flags |= IO_FOREIGN;
			break;
		case 'f':
			flags |= IO_CREAT;
			break;
		case 'm':
			mode = strtoul(optarg, &sp, 0);
			if (!sp || sp == optarg) {
				fprintf(stderr, _("non-numeric mode -- %s\n"),
					optarg);
				exit(1);
			}
			break;
		case 'n':
			flags |= IO_NONBLOCK;
			break;
		case 'p':
			progname = optarg;
			break;
		case 'r':
			flags |= IO_READONLY;
			break;
		case 's':
			flags |= IO_OSYNC;
			break;
		case 't':
			flags |= IO_TRUNC;
			break;
		case 'R':
			flags |= IO_REALTIME;
			break;
		case 'x':
			expert = 1;
			break;
		case 'V':
			printf(_("%s version %s\n"), progname, VERSION);
			exit(0);
		default:
			usage();
		}
	}

	while (optind < argc) {
		if ((c = openfile(argv[optind], flags & IO_FOREIGN ?
					NULL : &geometry, flags, mode)) < 0)
			exit(1);
		if (addfile(argv[optind], c, &geometry, flags) < 0)
			exit(1);
		optind++;
	}

	init_commands();
	add_args_command(init_args_command);
	add_check_command(init_check_command);
}

int
main(
	int	argc,
	char	**argv)
{
	init(argc, argv);
	command_loop();
	return exitcode;
}
