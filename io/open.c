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

static cmdinfo_t open_cmd;
static cmdinfo_t stat_cmd;
static cmdinfo_t close_cmd;
static cmdinfo_t setfl_cmd;
static cmdinfo_t statfs_cmd;
static cmdinfo_t chattr_cmd;
static cmdinfo_t lsattr_cmd;
static cmdinfo_t extsize_cmd;

off64_t
filesize(void)
{
	struct stat64	st;

	if (fstat64(file->fd, &st) < 0) {
		perror("fstat64");
		return -1;
	}
	return st.st_size;
}

static char *
filetype(mode_t mode)
{
	switch (mode & S_IFMT) {
	case S_IFSOCK:
		return _("socket");
	case S_IFDIR:
		return _("directory");
	case S_IFCHR:
		return _("char device");
	case S_IFBLK:
		return _("block device");
	case S_IFREG:
		return _("regular file");
	case S_IFLNK:
		return _("symbolic link");
	case S_IFIFO:
		return _("fifo");
	}
	return NULL;
}

static void
printxattr(int flags, int verbose, int dofname, int dobraces, int doeol)
{
	static struct {
		int	flag;
		char	*shortname;
		char	*longname;
	} *p, xflags[] = {
		{ XFS_XFLAG_REALTIME,	"r", "realtime" },
		{ XFS_XFLAG_PREALLOC,	"p", "prealloc" },
		{ XFS_XFLAG_IMMUTABLE,	"i", "immutable" },
		{ XFS_XFLAG_APPEND,	"a", "append-only" },
		{ XFS_XFLAG_SYNC,	"s", "sync" },
		{ XFS_XFLAG_NOATIME,	"A", "no-atime" },
		{ XFS_XFLAG_NODUMP,	"d", "no-dump" },
		{ 0, NULL, NULL }
	};
	int	first = 1;

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
		printf(" %s ", file->name);
	if (doeol)
		fputs("\n", stdout);
}

static int
stat_f(
	int		argc,
	char		**argv)
{
	struct fsxattr	fsx;
	struct stat64	st;
	int		verbose = (argc == 2 && !strcmp(argv[1], "-v"));

	printf(_("fd.path = \"%s\"\n"), file->name);
	printf(_("fd.flags = %s,%s,%s%s%s\n"),
		file->flags & IO_OSYNC ? _("sync") : _("non-sync"),
		file->flags & IO_DIRECT ? _("direct") : _("non-direct"),
		file->flags & IO_READONLY ? _("read-only") : _("read-write"),
		file->flags & IO_REALTIME ? _(",real-time") : "",
		file->flags & IO_APPEND ? _(",append-only") : "");
	if (fstat64(file->fd, &st) < 0) {
		perror("fstat64");
	} else {
		printf(_("stat.ino = %lld\n"), (long long)st.st_ino);
		printf(_("stat.type = %s\n"), filetype(st.st_mode));
		printf(_("stat.size = %lld\n"), (long long)st.st_size);
		printf(_("stat.blocks = %lld\n"), (long long)st.st_blocks);
		if (verbose) {
			printf(_("stat.atime = %s"), ctime(&st.st_atime));
			printf(_("stat.mtime = %s"), ctime(&st.st_mtime));
			printf(_("stat.ctime = %s"), ctime(&st.st_ctime));
		}
	}
	if (file->flags & IO_FOREIGN)
		return 0;
	if ((xfsctl(file->name, file->fd, XFS_IOC_FSGETXATTR, &fsx)) < 0) {
		perror("XFS_IOC_FSGETXATTR");
	} else {
		printf(_("xattr.xflags = 0x%x "), fsx.fsx_xflags);
		printxattr(fsx.fsx_xflags, verbose, 0, 1, 1);
		printf(_("xattr.extsize = %u\n"), fsx.fsx_extsize);
		printf(_("xattr.nextents = %u\n"), fsx.fsx_nextents);
	}
	return 0;
}

int
openfile(
	char		*path,
	xfs_fsop_geom_t	*geom,
	int		flags,
	mode_t		mode)
{
	int		fd;
	int		oflags;

	oflags = flags & IO_READONLY ? O_RDONLY : O_RDWR;
	if (flags & IO_APPEND)
		oflags |= O_APPEND;
	if (flags & IO_CREAT)
		oflags |= O_CREAT;
	if (flags & IO_DIRECT)
		oflags |= O_DIRECT;
	if (flags & IO_OSYNC)
		oflags |= O_SYNC;
	if (flags & IO_TRUNC)
		oflags |= O_TRUNC;

	fd = open(path, oflags, mode);
	if (fd < 0) {
		perror(path);
		return -1;
	}

	if (!geom)
		return fd;

	if (!platform_test_xfs_fd(fd)) {
		fprintf(stderr, _("%s: specified file "
			"[\"%s\"] is not on an XFS filesystem\n"),
			progname, path);
		close(fd);
		return -1;
	}

	if (xfsctl(path, fd, XFS_IOC_FSGEOMETRY, geom) < 0) {
		perror("XFS_IOC_FSGEOMETRY");
		close(fd);
		return -1;
	}

	if (!(flags & IO_READONLY) && (flags & IO_REALTIME)) {
		struct fsxattr	attr;

		if (xfsctl(path, fd, XFS_IOC_FSGETXATTR, &attr) < 0) {
			perror("XFS_IOC_FSGETXATTR");
			close(fd);
			return -1;
		}
		if (!(attr.fsx_xflags & XFS_XFLAG_REALTIME)) {
			attr.fsx_xflags |= XFS_XFLAG_REALTIME;
			if (xfsctl(path, fd, XFS_IOC_FSSETXATTR, &attr) < 0) {
				perror("XFS_IOC_FSSETXATTR");
				close(fd);
				return -1;
			}
		}
	}
	return fd;
}

int
addfile(
	char		*name,
	int		fd,
	xfs_fsop_geom_t	*geometry,
	int		flags)
{
	char		*filename;

	filename = strdup(name);
	if (!filename) {
		perror("strdup");
		close(fd);
		return -1;
	}

	/* Extend the table of currently open files */
	filetable = (fileio_t *)realloc(filetable,	/* growing */
					++filecount * sizeof(fileio_t));
	if (!filetable) {
		perror("realloc");
		filecount = 0;
		free(filename);
		close(fd);
		return -1;
	}

	/* Finally, make this the new active open file */
	file = &filetable[filecount - 1];
	file->fd = fd;
	file->flags = flags;
	file->name = filename;
	file->geom = *geometry;
	return 0;
}

static void
open_help(void)
{
	printf(_(
"\n"
" opens a new file in the requested mode\n"
"\n"
" Example:\n"
" 'open -cd /tmp/data' - creates/opens data file read-write for direct IO\n"
"\n"
" Opens a file for subsequent use by all of the other xfs_io commands.\n"
" With no arguments, open uses the stat command to show the current file.\n"
" -F -- foreign filesystem file, disallow XFS-specific commands\n"
" -a -- open with the O_APPEND flag (append-only mode)\n"
" -d -- open with O_DIRECT (non-buffered IO, note alignment constraints)\n"
" -f -- open with O_CREAT (create the file if it doesn't exist)\n"
" -m -- permissions to use in case a new file is created (default 0600)\n"
" -r -- open with O_RDONLY, the default is O_RDWR\n"
" -s -- open with O_SYNC\n"
" -t -- open with O_TRUNC (truncate the file to zero length if it exists)\n"
" -x -- mark the file as a realtime XFS file immediately after opening it\n"
" Note1: usually read/write direct IO requests must be blocksize aligned;\n"
"        some kernels, however, allow sectorsize alignment for direct IO.\n"
" Note2: the bmap for non-regular files can be obtained provided the file\n"
"        was opened correctly (in particular, must be opened read-only).\n"
"\n"));
}

static int
open_f(
	int		argc,
	char		**argv)
{
	int		c, fd, flags = 0;
	char		*sp;
	mode_t		mode = 0600;
	xfs_fsop_geom_t	geometry = { 0 };

	if (argc == 1) {
		if (file)
			return stat_f(argc, argv);
		fprintf(stderr, _("no files are open, try 'help open'\n"));
		return 0;
	}

	while ((c = getopt(argc, argv, "Facdfm:rstx")) != EOF) {
		switch (c) {
		case 'F':
			flags |= IO_FOREIGN;
			break;
		case 'a':
			flags |= IO_APPEND;
			break;
		case 'c':
		case 'f':
			flags |= IO_CREAT;
			break;
		case 'd':
			flags |= IO_DIRECT;
			break;
		case 'm':
			mode = strtoul(optarg, &sp, 0);
			if (!sp || sp == optarg) {
				printf(_("non-numeric mode -- %s\n"), optarg);
				return 0;
			}
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
		case 'x':
			flags |= IO_REALTIME;
			break;
		default:
			return command_usage(&open_cmd);
		}
	}

	if (optind != argc - 1)
		return command_usage(&open_cmd);

	fd = openfile(argv[optind], flags & IO_FOREIGN ?
					NULL : &geometry, flags, mode);
	if (fd < 0)
		return 0;

	addfile(argv[optind], fd, &geometry, flags);
	return 0;
}

static int
close_f(
	int		argc,
	char		**argv)
{
	size_t		length;
	unsigned int	offset;

	if (close(file->fd) < 0) {
		perror("close");
		return 0;
	}
	free(file->name);

	/* Shuffle the file table entries down over the removed entry */
	offset = file - &filetable[0];
	length = filecount * sizeof(fileio_t);
	length -= (offset + 1) * sizeof(fileio_t);
	if (length)
		memmove(file, file + 1, length);

	/* Resize the memory allocated for the table, possibly freeing */
	if (--filecount) {
		filetable = (fileio_t *)realloc(filetable,	/* shrinking */
					filecount * sizeof(fileio_t));
		if (offset == filecount)
			offset--;
		file = filetable + offset;
	} else {
		free(filetable);
		file = filetable = NULL;
	}
	filelist_f();
	return 0;
}

static int
lsattr_f(
	int		argc,
	char		**argv)
{
	struct fsxattr	fsx;
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

	if ((xfsctl(file->name, file->fd, XFS_IOC_FSGETXATTR, &fsx)) < 0) {
		perror("XFS_IOC_FSGETXATTR");
	} else {
		printxattr(fsx.fsx_xflags, vflag, !aflag, vflag, !aflag);
		if (aflag) {
			fputs("/", stdout);
			printxattr(-1, 0, 1, 0, 1);
		}
	}
	return 0;
}

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
"\n"
" Options:\n"
" -a -- show all flags which can be set alongside those which are set\n"
" -v -- verbose mode; show long names of flags, not single characters\n"
"\n"));
}

static int
chattr_f(
	int		argc,
	char		**argv)
{
	static struct {
		int	flag;
		char	optc;
	} *p, xflags[] = {
		{ XFS_XFLAG_REALTIME,	'r' },
		{ XFS_XFLAG_IMMUTABLE,	'i' },
		{ XFS_XFLAG_APPEND,	'a' },
		{ XFS_XFLAG_SYNC,	's' },
		{ XFS_XFLAG_NOATIME,	'A' },
		{ XFS_XFLAG_NODUMP,	'd' },
		{ 0, '\0' }
	};
	struct fsxattr	attr;
	unsigned int	i = 0;
	char		*c;

	if (xfsctl(file->name, file->fd, XFS_IOC_FSGETXATTR, &attr) < 0) {
		perror("XFS_IOC_FSGETXATTR");
		return 0;
	}
	while (++i < argc) {
		if (argv[i][0] == '+') {
			for (c = &argv[i][1]; *c; c++) {
				for (p = xflags; p->flag; p++) {
					if (p->optc == *c) {
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
					if (p->optc == *c) {
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
	if (xfsctl(file->name, file->fd, XFS_IOC_FSSETXATTR, &attr) < 0)
		perror("XFS_IOC_FSSETXATTR");
	return 0;
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
" Note1: user must have certain capabilities to modify immutable/append-only.\n"
" Note2: immutable/append-only files cannot be deleted; removing these files\n"
"        requires the immutable/append-only flag to be cleared first.\n"
" Note3: the realtime flag can only be set if the filesystem has a realtime\n"
"        section, and the (regular) file must be empty when the flag is set.\n"
"\n"));
}

static int
setfl_f(
	int			argc,
	char			**argv)
{
	int			c, flags;

	flags = fcntl(file->fd, F_GETFL, 0);
	if (flags < 0) {
		perror("fcntl(F_GETFL)");
		return 0;
	}

	while ((c = getopt(argc, argv, "ad")) != EOF) {
		switch (c) {
		case 'a':
			if (flags & O_APPEND)
				flags |= O_APPEND;
			else
				flags &= ~O_APPEND;
			break;
		case 'd':
			if (flags & O_DIRECT)
				flags |= O_DIRECT;
			else
				flags &= ~O_DIRECT;
			break;
		default:
			printf(_("invalid setfl argument -- '%c'\n"), c);
			return 0;
		}
	}

	if (fcntl(file->fd, F_SETFL, flags)  < 0)
		perror("fcntl(F_SETFL)");

	return 0;
}

static int
extsize_f(
	int			argc,
	char			**argv)
{
	struct fsxattr		fsx;
	long			extsize;
	int			blocksize, sectsize;

	init_cvtnum(&blocksize, &sectsize);
	extsize = (long)cvtnum(blocksize, sectsize, argv[1]);
	if (extsize < 0) {
		printf(_("non-numeric extsize argument -- %s\n"), argv[1]);
		return 0;
	}
	if ((xfsctl(file->name, file->fd, XFS_IOC_FSGETXATTR, &fsx)) < 0) {
		perror("XFS_IOC_FSGETXATTR");
		return 0;
	}
	fsx.fsx_extsize = extsize;
	if ((xfsctl(file->name, file->fd, XFS_IOC_FSSETXATTR, &fsx)) < 0) {
		perror("XFS_IOC_FSSETXATTR");
		return 0;
	}

	return 0;
}

static int
statfs_f(
	int			argc,
	char			**argv)
{
	struct xfs_fsop_geom	fsgeo;
	struct statfs		st;

	printf(_("fd.path = \"%s\"\n"), file->name);
	if (platform_fstatfs(file->fd, &st) < 0) {
		perror("fstatfs");
	} else {
		printf(_("statfs.f_bsize = %lld\n"), (long long) st.f_bsize);
		printf(_("statfs.f_blocks = %lld\n"), (long long) st.f_blocks);
#if defined(__sgi__)
		printf(_("statfs.f_frsize = %lld\n"), (long long) st.f_frsize);
#else
		printf(_("statfs.f_bavail = %lld\n"), (long long) st.f_bavail);
#endif
		printf(_("statfs.f_files = %lld\n"), (long long) st.f_files);
		printf(_("statfs.f_ffree = %lld\n"), (long long) st.f_ffree);
	}
	if (file->flags & IO_FOREIGN)
		return 0;
	if ((xfsctl(file->name, file->fd, XFS_IOC_FSGEOMETRY_V1, &fsgeo)) < 0) {
		perror("XFS_IOC_FSGEOMETRY_V1");
	} else {
		printf(_("geom.bsize = %u\n"), fsgeo.blocksize);
		printf(_("geom.agcount = %u\n"), fsgeo.agcount);
		printf(_("geom.agblocks = %u\n"), fsgeo.agblocks);
		printf(_("geom.datablocks = %llu\n"),
			(unsigned long long) fsgeo.datablocks);
		printf(_("geom.rtblocks = %llu\n"),
			(unsigned long long) fsgeo.rtblocks);
		printf(_("geom.rtextents = %llu\n"),
			(unsigned long long) fsgeo.rtextents);
		printf(_("geom.rtextsize = %u\n"), fsgeo.rtextsize);
		printf(_("geom.sunit = %u\n"), fsgeo.sunit);
		printf(_("geom.swidth = %u\n"), fsgeo.swidth);
	}
	return 0;
}

void
open_init(void)
{
	open_cmd.name = _("open");
	open_cmd.altname = _("o");
	open_cmd.cfunc = open_f;
	open_cmd.argmin = 0;
	open_cmd.argmax = -1;
	open_cmd.flags = CMD_NOMAP_OK | CMD_NOFILE_OK | CMD_FOREIGN_OK;
	open_cmd.args = _("[-acdrstx] [path]");
	open_cmd.oneline = _("open the file specified by path");
	open_cmd.help = open_help;

	stat_cmd.name = _("stat");
	stat_cmd.cfunc = stat_f;
	stat_cmd.argmin = 0;
	stat_cmd.argmax = 1;
	stat_cmd.flags = CMD_NOMAP_OK | CMD_FOREIGN_OK;
	stat_cmd.args = _("[-v]");
	stat_cmd.oneline = _("statistics on the currently open file");

	close_cmd.name = _("close");
	close_cmd.altname = _("c");
	close_cmd.cfunc = close_f;
	close_cmd.argmin = 0;
	close_cmd.argmax = 0;
	close_cmd.flags = CMD_NOMAP_OK | CMD_FOREIGN_OK;
	close_cmd.oneline = _("close the current open file");

	setfl_cmd.name = _("setfl");
	setfl_cmd.cfunc = setfl_f;
	setfl_cmd.args = _("[-adx]");
	setfl_cmd.flags = CMD_NOMAP_OK | CMD_FOREIGN_OK;
	setfl_cmd.oneline =
		_("set/clear append/direct flags on the open file");

	statfs_cmd.name = _("statfs");
	statfs_cmd.cfunc = statfs_f;
	statfs_cmd.flags = CMD_NOMAP_OK | CMD_FOREIGN_OK;
	statfs_cmd.oneline =
		_("statistics on the filesystem of the currently open file");

	chattr_cmd.name = _("chattr");
	chattr_cmd.cfunc = chattr_f;
	chattr_cmd.args = _("[+/-riasAd]");
	chattr_cmd.argmin = 1;
	chattr_cmd.argmax = 1;
	chattr_cmd.flags = CMD_NOMAP_OK;
	chattr_cmd.oneline =
		_("change extended inode flags on the currently open file");
	chattr_cmd.help = chattr_help;

	lsattr_cmd.name = _("lsattr");
	lsattr_cmd.cfunc = lsattr_f;
	lsattr_cmd.args = _("[-a | -v]");
	lsattr_cmd.argmin = 0;
	lsattr_cmd.argmax = 2;
	lsattr_cmd.flags = CMD_NOMAP_OK;
	lsattr_cmd.oneline =
		_("list extended inode flags set on the currently open file");
	lsattr_cmd.help = lsattr_help;

	extsize_cmd.name = _("extsize");
	extsize_cmd.cfunc = extsize_f;
	extsize_cmd.argmin = 1;
	extsize_cmd.argmax = 1;
	extsize_cmd.flags = CMD_NOMAP_OK;
	extsize_cmd.oneline =
		_("set prefered extent size (in bytes) for the open file");

	add_command(&open_cmd);
	add_command(&stat_cmd);
	add_command(&close_cmd);
	add_command(&setfl_cmd);
	add_command(&statfs_cmd);
	add_command(&chattr_cmd);
	add_command(&lsattr_cmd);

	if (expert)
		add_command(&extsize_cmd);
}
