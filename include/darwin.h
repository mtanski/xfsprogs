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
 *
 */
#ifndef __XFS_DARWIN_H__
#define __XFS_DARWIN_H__

#include <uuid/uuid.h>
#include <libgen.h>
#include <sys/vm.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mount.h>

#include <sys/syscall.h>
# ifndef SYS_fsctl
#  define SYS_fsctl	242
# endif
static __inline__ int xfsctl(const char *path, int fd, int cmd, void *p)
{
	return syscall(SYS_fsctl, path, cmd, p, 0);
}

static __inline__ int platform_test_xfs_fd(int fd)
{
	struct statfs buf;
	if (fstatfs(fd, &buf) < 0)
		return 0;
	return (strcmp(buf.f_fstypename, "xfs") == 0);
}

static __inline__ int platform_test_xfs_path(const char *path)
{
	struct statfs buf;
	if (statfs(path, &buf) < 0)
		return 0;
	return (strcmp(buf.f_fstypename, "xfs") == 0);
}

static __inline__ int platform_fstatfs(int fd, struct statfs *buf)
{
	return fstatfs(fd, buf);
}

static __inline__ void platform_getoptreset(void)
{
	extern int optreset;
	optreset = 0;
}

#define __int8_t	int8_t
#define __int16_t	int16_t
#define __int32_t	int32_t
#define __int32_t	int32_t
#define __int64_t	int64_t
#define __uint8_t	u_int8_t
#define __uint16_t	u_int16_t
#define __uint32_t	u_int32_t
#define __uint64_t	u_int64_t
#define __s8		int8_t
#define __s16		int16_t
#define __s32		int32_t
#define __s64		int64_t
#define __u8		u_int8_t
#define __u16		u_int16_t
#define __u32		u_int32_t
#define __u64		u_int64_t
#define loff_t		off_t
#define off64_t		off_t

typedef off_t		xfs_off_t;
typedef u_int64_t	xfs_ino_t;
typedef u_int32_t	xfs_dev_t;
typedef int64_t		xfs_daddr_t;
typedef char*		xfs_caddr_t;

typedef unsigned char	uchar_t;
#define stat64		stat
#define fstat64		fstat
#define lseek64		lseek
#define pread64		pread
#define pwrite64	pwrite
#define ftruncate64	ftruncate
#define fdatasync	fsync
#define memalign(a,sz)	valloc(sz)

#include <machine/endian.h>
#define __BYTE_ORDER	BYTE_ORDER
#define __BIG_ENDIAN	BIG_ENDIAN
#define __LITTLE_ENDIAN	LITTLE_ENDIAN
#include <xfs/swab.h>

#define O_LARGEFILE     0
#ifndef O_DIRECT
#define O_DIRECT        0
#endif
#ifndef O_SYNC
#define O_SYNC          0
#endif

#define B_FALSE		0
#define B_TRUE		1

#define ENOATTR		989     /* Attribute not found */
#define EFSCORRUPTED	990	/* Filesystem is corrupted */
#define constpp		char * const *

#define HAVE_FID	1

#endif	/* __XFS_DARWIN_H__ */
