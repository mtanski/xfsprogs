/*
 * Copyright (c) 2005-2006 Silicon Graphics, Inc.
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

#include <paths.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <xfs/path.h>
#include <xfs/input.h>
#include <xfs/project.h>

extern char *progname;

int fs_count;
struct fs_path *fs_table;
struct fs_path *fs_path;

char *mtab_file;
#define PROC_MOUNTS	"/proc/self/mounts"

static int
fs_device_number(
	const char	*name,
	dev_t		*devnum)
{
	struct stat64	sbuf;

	if (stat64(name, &sbuf) < 0)
		return errno;
	/*
	 * We want to match st_rdev if the path provided is a device
	 * special file.  Otherwise we are looking for the the
	 * device id for the containing filesystem, in st_dev.
	 */
	if (S_ISBLK(sbuf.st_mode) || S_ISCHR(sbuf.st_mode))
		*devnum = sbuf.st_rdev;
	else
		*devnum = sbuf.st_dev;

	return 0;
}

/*
 * Find the FS table entry for the given path.  The "flags" argument
 * is a mask containing FS_MOUNT_POINT or FS_PROJECT_PATH (or both)
 * to indicate the type of table entry sought.
 */
struct fs_path *
fs_table_lookup(
	const char	*dir,
	uint		flags)
{
	uint		i;
	dev_t		dev = 0;

	if (fs_device_number(dir, &dev))
		return NULL;

	for (i = 0; i < fs_count; i++) {
		if (flags && !(flags & fs_table[i].fs_flags))
			continue;
		if (fs_table[i].fs_datadev == dev)
			return &fs_table[i];
	}
	return NULL;
}

static int
fs_table_insert(
	char		*dir,
	uint		prid,
	uint		flags,
	char		*fsname,
	char		*fslog,
	char		*fsrt)
{
	dev_t		datadev, logdev, rtdev;
	struct fs_path	*tmp_fs_table;

	if (!dir || !fsname)
		return EINVAL;

	datadev = logdev = rtdev = 0;
	if (fs_device_number(dir, &datadev))
		return errno;
	if (fslog && fs_device_number(fslog, &logdev))
		return errno;
	if (fsrt && fs_device_number(fsrt, &rtdev))
		return errno;

	tmp_fs_table = realloc(fs_table, sizeof(fs_path_t) * (fs_count + 1));
	if (!tmp_fs_table)
		return ENOMEM;
	fs_table = tmp_fs_table;

	fs_path = &fs_table[fs_count];
	fs_path->fs_dir = dir;
	fs_path->fs_prid = prid;
	fs_path->fs_flags = flags;
	fs_path->fs_name = fsname;
	fs_path->fs_log = fslog;
	fs_path->fs_rt = fsrt;
	fs_path->fs_datadev = datadev;
	fs_path->fs_logdev = logdev;
	fs_path->fs_rtdev = rtdev;
	fs_count++;
	return 0;
}

void
fs_table_destroy(void)
{
	while (--fs_count >= 0) {
		free(fs_table[fs_count].fs_name);
		if (fs_table[fs_count].fs_log)
			free(fs_table[fs_count].fs_log);
		if (fs_table[fs_count].fs_rt)
			free(fs_table[fs_count].fs_rt);
		free(fs_table[fs_count].fs_dir);
	}
	if (fs_table)
		free(fs_table);
	fs_table = NULL;
	fs_count = 0;
}

/*
 * Table iteration (cursor-based) interfaces
 */

/*
 * Initialize an fs_table cursor.  If a directory path is supplied,
 * the cursor is set up to appear as though the table contains only
 * a single entry which represents the directory specified.
 * Otherwise it is set up to prepare for visiting all entries in the
 * global table, starting with the first.  "flags" can be either
 * FS_MOUNT_POINT or FS_PROJECT_PATH to limit what type of entries
 * will be selected by fs_cursor_next_entry().  0 can be used as a
 * wild card (selecting either type).
 */
void
fs_cursor_initialise(
	char		*dir,
	uint		flags,
	fs_cursor_t	*cur)
{
	fs_path_t	*path;

	memset(cur, 0, sizeof(*cur));
	if (dir) {
		if ((path = fs_table_lookup(dir, flags)) == NULL)
			return;
		cur->local = *path;
		cur->count = 1;
		cur->table = &cur->local;
	} else {
		cur->count = fs_count;
		cur->table = fs_table;
	}
	cur->flags = flags;
}

/*
 * Use the cursor to find the next entry in the table having the
 * type specified by the cursor's "flags" field.
 */
struct fs_path *
fs_cursor_next_entry(
	fs_cursor_t	*cur)
{
	while (cur->index < cur->count) {
		fs_path_t	*next = &cur->table[cur->index++];

		if (!cur->flags || (cur->flags & next->fs_flags))
			return next;
	}
	return NULL;
}


#if defined(HAVE_GETMNTENT)
#include <mntent.h>

static void
fs_extract_mount_options(
	struct mntent	*mnt,
	char		**logp,
	char		**rtp)
{
	char		*fslog, *fsrt;

	/* Extract log device and realtime device from mount options */
	if ((fslog = hasmntopt(mnt, "logdev=")))
		fslog += 7;
	if ((fsrt = hasmntopt(mnt, "rtdev=")))
		fsrt += 6;

	/* Do this only after we've finished processing mount options */
	if (fslog) {
		fslog = strndup(fslog, strcspn(fslog, " ,"));
		if (!fslog) {
			fprintf(stderr, _("%s: %s: out of memory (fslog)\n"),
				    progname, __func__);
			exit(1);
		}
	}
	if (fsrt) {
		fsrt = strndup(fsrt, strcspn(fsrt, " ,"));
		if (!fsrt) {
			fprintf(stderr, _("%s: %s: out of memory (fsrt)\n"),
				    progname, __func__);
			free(fslog);
			exit(1);
		}
	}

	*logp = fslog;
	*rtp = fsrt;
}

static int
fs_table_initialise_mounts(
	char		*path)
{
	struct mntent	*mnt;
	FILE		*mtp;
	char		*dir, *fsname, *fslog, *fsrt;
	int		error, found;

	error = found = 0;
	dir = fsname = fslog = fsrt = NULL;

	if (!mtab_file) {
		mtab_file = PROC_MOUNTS;
		if (access(mtab_file, R_OK) != 0)
			mtab_file = MOUNTED;
	}

	if ((mtp = setmntent(mtab_file, "r")) == NULL)
		return ENOENT;

	while ((mnt = getmntent(mtp)) != NULL) {
		if (strcmp(mnt->mnt_type, "xfs") != 0)
			continue;
		if (path &&
		    ((strcmp(path, mnt->mnt_dir) != 0) &&
		     (strcmp(path, mnt->mnt_fsname) != 0)))
			continue;
		found = 1;
		dir = strdup(mnt->mnt_dir);
		fsname = strdup(mnt->mnt_fsname);
		if (!dir || !fsname) {
			error = ENOMEM;
			break;
		}
		fs_extract_mount_options(mnt, &fslog, &fsrt);
		if ((error = fs_table_insert(dir, 0, FS_MOUNT_POINT,
						fsname, fslog, fsrt)))
			break;
	}
	endmntent(mtp);
	if (!error && path && !found)
		error = ENXIO;
	if (error) {
		if (dir) free(dir);
		if (fsrt) free(fsrt);
		if (fslog) free(fslog);
		if (fsname) free(fsname);
	}
	return error;
}

#elif defined(HAVE_GETMNTINFO)
#include <sys/mount.h>

static int
fs_table_initialise_mounts(
	char		*path)
{
	struct statfs	*stats;
	char		*dir, *fsname, *fslog, *fsrt;
	int		i, count, error, found;

	error = found = 0;
	dir = fsname = fslog = fsrt = NULL;

	if ((count = getmntinfo(&stats, 0)) < 0) {
		fprintf(stderr, _("%s: getmntinfo() failed: %s\n"),
				progname, strerror(errno));
		return 0;
	}

	for (i = 0; i < count; i++) {
		if (strcmp(stats[i].f_fstypename, "xfs") != 0)
			continue;
		if (path &&
		    ((strcmp(path, stats[i].f_mntonname) != 0) &&
		     (strcmp(path, stats[i].f_mntfromname) != 0)))
			continue;
		found = 1;
		dir = strdup(stats[i].f_mntonname);
		fsname = strdup(stats[i].f_mntfromname);
		if (!dir || !fsname) {
			error = ENOMEM;
			break;
		}
		/* TODO: external log and realtime device? */
		if ((error = fs_table_insert(dir, 0, FS_MOUNT_POINT,
						fsname, fslog, fsrt)))
			break;
	}
	if (!error && path && !found)
		error = ENXIO;
	if (error) {
		if (dir) free(dir);
		if (fsrt) free(fsrt);
		if (fslog) free(fslog);
		if (fsname) free(fsname);
	}
	return error;
}

#else
# error "How do I extract info about mounted filesystems on this platform?"
#endif

/*
 * Given a directory, match it up to a filesystem mount point.
 */
static struct fs_path *
fs_mount_point_from_path(
	const char	*dir)
{
	fs_cursor_t	cursor;
	fs_path_t	*fs;
	dev_t		dev = 0;

	if (fs_device_number(dir, &dev))
		return NULL;

	fs_cursor_initialise(NULL, FS_MOUNT_POINT, &cursor);
	while ((fs = fs_cursor_next_entry(&cursor))) {
		if (fs->fs_datadev == dev)
			break;
	}
	return fs;
}

static void
fs_table_insert_mount(
	char		*mount)
{
	int		error;

	error = fs_table_initialise_mounts(mount);
	if (error) {
		fs_table_destroy();
		fprintf(stderr, _("%s: cannot setup path for mount %s: %s\n"),
			progname, mount, strerror(error));
		exit(1);
	}
}

static int
fs_table_initialise_projects(
	char		*project)
{
	fs_project_path_t *path;
	fs_path_t	*fs;
	prid_t		prid = 0;
	char		*dir = NULL, *fsname = NULL;
	int		error = 0, found = 0;

	if (project)
		prid = prid_from_string(project);

	setprpathent();
	while ((path = getprpathent()) != NULL) {
		if (project && prid != path->pp_prid)
			continue;
		if ((fs = fs_mount_point_from_path(path->pp_pathname)) == NULL) {
			fprintf(stderr, _("%s: cannot find mount point for path `%s': %s\n"),
					progname, path->pp_pathname, strerror(errno));
			continue;
		}
		found = 1;
		dir = strdup(path->pp_pathname);
		fsname = strdup(fs->fs_name);
		if (!dir || !fsname) {
			error = ENOMEM;
			break;
		}
		if ((error = fs_table_insert(dir, path->pp_prid,
					FS_PROJECT_PATH, fsname, NULL, NULL)))
			break;
	}
	endprpathent();

	if (!error && project && !found)
		error = ENOENT;
	if (error) {
		if (dir) free(dir);
		if (fsname) free(fsname);
	}
	return error;
}

static void
fs_table_insert_project(
	char		*project)
{
	int		error;

	if (!fs_count) {
		fprintf(stderr, _("%s: no mount table yet, so no projects\n"),
			progname);
		exit(1);
	}
	error = fs_table_initialise_projects(project);
	if (error) {
		fs_table_destroy();
		fprintf(stderr, _("%s: cannot setup path for project %s: %s\n"),
			progname, project, strerror(error));
		exit(1);
	}
}

/*
 * Initialize fs_table to contain the given set of mount points and
 * projects.  If mount_count is zero, mounts is ignored and the
 * table is populated with mounted filesystems.  If project_count is
 * zero, projects is ignored and the table is populated with all
 * projects defined in the projects file.
 */
void
fs_table_initialise(
	int	mount_count,
	char	*mounts[],
	int	project_count,
	char	*projects[])
{
	int	error;
	int	i;

	if (mount_count) {
		for (i = 0; i < mount_count; i++)
			fs_table_insert_mount(mounts[i]);
	} else {
		error = fs_table_initialise_mounts(NULL);
		if (error)
			goto out_exit;
	}
	if (project_count) {
		for (i = 0; i < project_count; i++)
			fs_table_insert_project(projects[i]);
	} else {
		error = fs_table_initialise_projects(NULL);
		if (error)
			goto out_exit;
	}

	return;

out_exit:
	fs_table_destroy();
	fprintf(stderr, _("%s: cannot initialise path table: %s\n"),
		progname, strerror(error));
	exit(1);
}

void 
fs_table_insert_project_path(
	char		*udir,
	prid_t		prid)
{
	fs_path_t	*fs;
	char		*dir = NULL, *fsname = NULL;
	int		error = 0;

	if ((fs = fs_mount_point_from_path(udir)) != NULL) {
		dir = strdup(udir);
		fsname = strdup(fs->fs_name);
		if (dir && fsname)
			error = fs_table_insert(dir, prid,
					FS_PROJECT_PATH, fsname, NULL, NULL);
		else
			error = ENOMEM;
	} else
		error = ENOENT;

	if (error) {
		if (dir)
			free(dir);
		if (fsname)
			free(fsname);
		fprintf(stderr, _("%s: cannot setup path for project dir %s: %s\n"),
				progname, udir, strerror(error));
		exit(1);
	}
}

