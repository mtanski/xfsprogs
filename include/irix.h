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
 *
 */
#ifndef __XFS_IRIX_H__
#define __XFS_IRIX_H__

#include <libgen.h>
#include <values.h>
#include <strings.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <sys/uuid.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/statfs.h>
#include <sys/statvfs.h>
#include <sys/syssgi.h>
#include <sys/sysmacros.h>
#include <sys/fs/xfs_fsops.h>
#include <sys/fs/xfs_itable.h>

#define __s8		char
#define __s16		short
#define __s32		__int32_t
#define __s64		__int64_t
#define __u8		unsigned char
#define __u16		unsigned short
#define __u32		__int32_t
#define __u64		__int64_t
#define __int8_t	char
#define __int16_t	short
#define __uint8_t	unsigned char
#define __uint16_t	unsigned short
#define loff_t		off64_t
typedef off64_t		xfs_off_t;
typedef __int64_t	xfs_ino_t;
typedef __int32_t	xfs_dev_t;
typedef __int64_t	xfs_daddr_t;
typedef char*		xfs_caddr_t;
typedef flock64_t	xfs_flock64_t;

#include <sys/endian.h>
#define __BYTE_ORDER	BYTE_ORDER
#define __BIG_ENDIAN	BIG_ENDIAN
#define __LITTLE_ENDIAN	LITTLE_ENDIAN
#define __fswab16(x)	(x)
#define __fswab32(x)	(x)
#define __fswab64(x)	(x)

/* Map some gcc macros for the MipsPRO compiler */
#ifndef __GNUC__
#define __sgi__		__sgi
#define __inline__	__inline
#endif

#define INT_MAX		INT32_MAX
#define UINT_MAX	UINT32_MAX
#define PATH_MAX	MAXPATHLEN
#define constpp		char * const *

static __inline__ int xfsctl(const char *path, int fd, int cmd, void *p)
{
	if (cmd >= 0 && cmd < XFS_FSOPS_COUNT)
		return syssgi(SGI_XFS_FSOPERATIONS, fd, cmd, (void *)0, p);
	return fcntl(fd, cmd, p);
}

static __inline__ int platform_test_xfs_fd(int fd)
{
	struct statvfs sbuf;
	if (fstatvfs(fd, &sbuf) < 0)
		return 0;
	return (strcmp(sbuf.f_basetype, "xfs") == 0);
}

static __inline__ int platform_test_xfs_path(const char *path)
{
	struct statvfs sbuf;
	if (statvfs(path, &sbuf) < 0)
		return 0;
	return (strcmp(sbuf.f_basetype, "xfs") == 0);
}

static __inline__ int platform_fstatfs(int fd, struct statfs *buf)
{
	return fstatfs(fd, buf, sizeof(struct statfs), 0);
}

static __inline__ void platform_getoptreset(void)
{
	getoptreset();
}

static __inline__ char * strsep(char **s, const char *ct)
{
	char *sbegin = *s, *end;

	if (!sbegin)
		return NULL;
	end = strpbrk(sbegin, ct);
	if (end)
		*end++ = '\0';
	*s = end;
	return sbegin;
}

#define HAVE_DIOATTR	1
#define HAVE_FSXATTR	1
#define HAVE_GETBMAP	1
#define HAVE_GETBMAPX	1
#define HAVE_FSDMIDATA	1
#define HAVE_FID	1
#define HAVE_IOCMACROS	1
#define HAVE_BBMACROS	1

#define __XFS_FS_H__	1

#define XFS_IOC_DIOINFO			F_DIOINFO
#define XFS_IOC_FSGETXATTR		F_FSGETXATTR
#define XFS_IOC_FSSETXATTR		F_FSSETXATTR
#define XFS_IOC_ALLOCSP64		F_ALLOCSP64
#define XFS_IOC_FREESP64		F_FREESP64
#define XFS_IOC_GETBMAP			F_GETBMAP
#define XFS_IOC_FSSETDM			F_FSSETDM
#define XFS_IOC_RESVSP64		F_RESVSP64
#define XFS_IOC_UNRESVSP64		F_UNRESVSP64
#define XFS_IOC_GETBMAPA		F_GETBMAPA
#define XFS_IOC_FSGETXATTRA		F_FSGETXATTRA
#define XFS_IOC_GETBMAPX		F_GETBMAPX

#define XFS_IOC_FSGEOMETRY_V1		XFS_FS_GEOMETRY
#define XFS_IOC_FSBULKSTAT		/* TODO */
#define XFS_IOC_FSBULKSTAT_SINGLE	/* TODO */
#define XFS_IOC_FSINUMBERS		/* TODO */
#define XFS_IOC_PATH_TO_FSHANDLE	/* TODO */
#define XFS_IOC_PATH_TO_HANDLE		/* TODO */
#define XFS_IOC_FD_TO_HANDLE		/* TODO */
#define XFS_IOC_OPEN_BY_HANDLE		/* TODO */
#define XFS_IOC_READLINK_BY_HANDLE	/* TODO */
#define XFS_IOC_SWAPEXT			/* TODO */
#define XFS_IOC_FSGROWFSDATA		XFS_GROWFS_DATA
#define XFS_IOC_FSGROWFSLOG		XFS_GROWFS_LOG
#define XFS_IOC_FSGROWFSRT		XFS_GROWFS_RT
#define XFS_IOC_FSCOUNTS		XFS_FS_COUNTS
#define XFS_IOC_SET_RESBLKS		XFS_SET_RESBLKS
#define XFS_IOC_GET_RESBLKS		XFS_GET_RESBLKS
#define XFS_IOC_ERROR_INJECTION		/* TODO */
#define XFS_IOC_ERROR_CLEARALL		/* TODO */
#define XFS_IOC_FREEZE			XFS_FS_FREEZE
#define XFS_IOC_THAW			XFS_FS_THAW
#define XFS_IOC_FSSETDM_BY_HANDLE	/* TODO */
#define XFS_IOC_ATTRLIST_BY_HANDLE	/* TODO */
#define XFS_IOC_ATTRMULTI_BY_HANDLE	/* TODO */
#define XFS_IOC_FSGEOMETRY		XFS_FS_GEOMETRY
#define XFS_IOC_GOINGDOWN		XFS_FS_GOINGDOWN

#endif	/* __XFS_IRIX_H__ */
