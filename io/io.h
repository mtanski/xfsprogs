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

#define IO_READONLY	(1<<0)
#define IO_DIRECT	(1<<1)
#define IO_REALTIME	(1<<2)
#define IO_APPEND	(1<<3)
#define IO_OSYNC	(1<<4)
#define IO_CREAT	(1<<5)
#define IO_TRUNC	(1<<6)
#define IO_FOREIGN	(1<<7)

/*
 * Regular file I/O control
 */
typedef struct fileio {
	int		fd;		/* open file descriptor */
	int		flags;		/* flags describing file state */
	char		*name;		/* file name at time of open */
	xfs_fsop_geom_t	geom;		/* XFS filesystem geometry */
} fileio_t;

extern fileio_t		*filetable;	/* open file table */
extern int		filecount;	/* number of open files */
extern fileio_t		*file;		/* active file in file table */
extern int filelist_f(void);

/*
 * Memory mapped file regions
 */
typedef struct mmap_region {
	void		*addr;		/* address of start of mapping */
	size_t		length;		/* length of mapping */
	off64_t		offset;		/* start offset into backing file */
	int		prot;		/* protection mode of the mapping */
	char		*name;		/* name of backing file */
} mmap_region_t;

extern mmap_region_t	*maptable;	/* mmap'd region array */
extern int		mapcount;	/* #entries in the mapping table */
extern mmap_region_t	*mapping;	/* active mapping table entry */
extern int maplist_f(void);

/*
 * Various xfs_io helper routines/globals
 */

extern off64_t		filesize(void);
extern int		openfile(char *, xfs_fsop_geom_t *, int, mode_t);
extern int		addfile(char *, int , xfs_fsop_geom_t *, int);

extern void		*buffer;
extern ssize_t		buffersize;
extern int		alloc_buffer(ssize_t, int, unsigned int);
extern int		read_buffer(int, off64_t, long long, long long *,
					int, int);
extern void		dump_buffer(off64_t, ssize_t);
