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
#ifndef __PROJECT_H__
#define __PROJECT_H__

#include <xfs/xfs.h>

typedef __uint32_t	prid_t;
extern int setprojid(const char *__name, int __fd, prid_t __id);
extern int getprojid(const char *__name, int __fd, prid_t *__id);

typedef struct fs_project {
	prid_t		pr_prid;	/* project identifier */
	char		*pr_name;	/* project name */
} fs_project_t;

extern void setprent(void);
extern void endprent(void);
extern fs_project_t *getprent(void);
extern fs_project_t *getprnam(char *__name);
extern fs_project_t *getprprid(prid_t __id);

typedef struct fs_project_path {
	prid_t		pp_prid;	/* project identifier */
	char		*pp_pathname;	/* pathname to root of project tree */
} fs_project_path_t;

extern void setprpathent(void);
extern void endprpathent(void);
extern fs_project_path_t *getprpathent(void);

extern void setprfiles(void);
extern char *projid_file;
extern char *projects_file;

#endif	/* __PROJECT_H__ */
