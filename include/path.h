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
#ifndef __PATH_H__
#define __PATH_H__

#include <xfs/libxfs.h>

/*
 * XFS Filesystem Paths
 *
 * Utility routines for iterating and searching through the list
 * of known mounted filesystems and project paths.
 */

#define FS_MOUNT_POINT	(1<<0)
#define FS_PROJECT_PATH	(1<<1)

typedef struct fs_path {
	char		*fs_name;	/* Data device for filesystem 	*/
	dev_t		fs_datadev;
	char		*fs_log;	/* External log device, if any	*/
	dev_t		fs_logdev;
	char		*fs_rt;		/* Realtime device, if any	*/
	dev_t		fs_rtdev;
	char		*fs_dir;	/* Directory / mount point	*/
	uint		fs_flags;	/* FS_MOUNT_POINT/FS_MOUNT_TREE */
	uint		fs_prid;	/* Project ID for tree root	*/
} fs_path_t;

extern int fs_count;		/* number of entries in fs table */
extern fs_path_t *fs_table;	/* array of entries in fs table  */
extern fs_path_t *fs_path;	/* current entry in the fs table */
extern char *mtab_file;

extern void fs_table_initialise(void);
extern void fs_table_destroy(void);

extern void fs_table_insert_mount(char *__mount);
extern void fs_table_insert_project(char *__project);

extern fs_path_t *fs_table_lookup(const char *__dir, uint __flags);

typedef struct fs_cursor {
	uint		count;		/* total count of mount entries	*/
	uint		index;		/* current position in table	*/
	uint		flags;		/* iterator flags: mounts/trees */
	fs_path_t	*table;		/* local/global table pointer	*/
	fs_path_t	local;		/* space for single-entry table	*/
} fs_cursor_t;

extern void fs_cursor_initialise(char *__dir, uint __flags, fs_cursor_t *__cp);
extern fs_path_t *fs_cursor_next_entry(fs_cursor_t *__cp);

#endif	/* __PATH_H__ */
