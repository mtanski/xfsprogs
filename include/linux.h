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
#ifndef __XFS_LINUX_H__
#define __XFS_LINUX_H__

#include <uuid/uuid.h>
#include <sys/vfs.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <linux/types.h>
#include <malloc.h>
#include <getopt.h>
#include <endian.h>
#include <xfs/swab.h>

static __inline__ int xfsctl(const char *path, int fd, int cmd, void *p)
{
	return ioctl(fd, cmd, p);
}

static __inline__ int platform_test_xfs_fd(int fd)
{
	struct statfs buf;
	if (fstatfs(fd, &buf) < 0)
		return 0;
	return (buf.f_type == 0x58465342);	/* XFSB */
}

static __inline__ int platform_test_xfs_path(const char *path)
{
	struct statfs buf;
	if (statfs(path, &buf) < 0)
		return 0;
	return (buf.f_type == 0x58465342);	/* XFSB */
}

static __inline__ int platform_fstatfs(int fd, struct statfs *buf)
{
	return fstatfs(fd, buf);
}

static __inline__ void platform_getoptreset(void)
{
	extern int optind;
	optind = 0;
}

#ifndef O_DIRECT
# if defined (__powerpc__)
#  define O_DIRECT	0400000
# elif defined (__sparc__)
#  define O_DIRECT	0x100000
# endif
#endif

#if (__GLIBC__ < 2) || ((__GLIBC__ == 2) && (__GLIBC_MINOR__ <= 1))
# define constpp	const char * const *
#else
# define constpp	char * const *
#endif

#define ENOATTR		ENODATA	/* Attribute not found */
#define EFSCORRUPTED	990	/* Filesystem is corrupted */

typedef loff_t		xfs_off_t;
typedef __uint64_t	xfs_ino_t;
typedef __uint32_t	xfs_dev_t;
typedef __int64_t	xfs_daddr_t;
typedef char*		xfs_caddr_t;

#ifndef	_UCHAR_T_DEFINED
typedef unsigned char	uchar_t;
#define	_UCHAR_T_DEFINED	1
#endif

#ifndef _BOOLEAN_T_DEFINED
typedef enum {B_FALSE, B_TRUE}	boolean_t;
#define _BOOLEAN_T_DEFINED	1
#endif

#endif	/* __XFS_IRIX_H__ */
