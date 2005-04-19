/*
 * Copyright (c) 2000-2005 Silicon Graphics, Inc.  All Rights Reserved.
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
#ifndef __INPUT_H__
#define __INPUT_H__

#include <pwd.h>
#include <grp.h>
#include <sys/types.h>
#include <xfs/project.h>

extern char	**breakline(char *input, int *count);
extern void	doneline(char *input, char **vec);
extern char	*fetchline(void);

extern long long cvtnum(int blocksize, int sectorsize, char *s);
extern void	cvtstr(double value, char *str, size_t sz);
extern unsigned long cvttime(char *s);

extern struct timeval tadd(struct timeval t1, struct timeval t2);
extern struct timeval tsub(struct timeval t1, struct timeval t2);
extern double	tdiv(double value, struct timeval tv);

enum {
	DEFAULT_TIME		= 0x0,
	TERSE_FIXED_TIME	= 0x1,
	VERBOSE_FIXED_TIME	= 0x2,
};

extern void	timestr(struct timeval *tv, char *str, size_t sz, int flags);

extern uid_t	uid_from_string(char *user);
extern gid_t	gid_from_string(char *group);
extern prid_t	prid_from_string(char *project);

#define HAVE_FTW_H 1	/* TODO: configure me */

#ifdef HAVE_FTW_H
#include <ftw.h>
#else
struct FTW;
struct stat;
extern int nftw(
	char	*dir,
	int	(*fn)(const char *, const struct stat *, int, struct FTW *),
	int	depth,
	int	flags);
#endif

#endif	/* __INPUT_H__ */
