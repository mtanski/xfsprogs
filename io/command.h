/*
 * Copyright (c) 2000-2004 Silicon Graphics, Inc.  All Rights Reserved.
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

#define min(a,b)	(((a)<(b))?(a):(b))

typedef int (*cfunc_t)(int argc, char **argv);
typedef void (*helpfunc_t)(void);

#define CMD_NOFILE_OK	(1<<0)	/* command doesn't need an open file	*/
#define CMD_NOMAP_OK	(1<<1)	/* command doesn't need a mapped region	*/
#define CMD_FOREIGN_OK	(1<<2)	/* command not restricted to XFS files	*/

typedef struct cmdinfo {
	const char	*name;
	const char	*altname;
	cfunc_t		cfunc;
	int		argmin;
	int		argmax;
	int		canpush;
	int		flags;
	const char	*args;
	const char	*oneline;
	helpfunc_t      help;
} cmdinfo_t;

extern cmdinfo_t	*cmdtab;
extern int		ncmds;

extern void		add_command(const cmdinfo_t *ci);
extern int		command_usage(const cmdinfo_t *ci);
extern int		command(int argc, char **argv);
extern const cmdinfo_t	*find_command(const char *cmd);
extern void		init_commands(void);

extern void		bmap_init(void);
extern void		file_init(void);
extern void		freeze_init(void);
extern void		fsync_init(void);
extern void		help_init(void);
extern void		inject_init(void);
extern void		mmap_init(void);
extern void		open_init(void);
extern void		pread_init(void);
extern void		prealloc_init(void);
extern void		pwrite_init(void);
extern void		quit_init(void);
extern void		truncate_init(void);

#ifdef HAVE_FADVISE
extern void		fadvise_init(void);
#else
#define fadvise_init()	do { } while (0)
#endif

#ifdef HAVE_INJECT
extern void		inject_init(void);
#else
#define inject_init()	do { } while (0)
#endif

#ifdef HAVE_RESBLKS
extern void		resblks_init(void);
#else
#define resblks_init()	do { } while (0)
#endif

#ifdef HAVE_SENDFILE
extern void		sendfile_init(void);
#else
#define sendfile_init()	do { } while (0)
#endif

#ifdef HAVE_SHUTDOWN
extern void		shutdown_init(void);
#else
#define shutdown_init()	do { } while (0)
#endif
