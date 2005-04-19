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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xfs/project.h>

#define PROJID		"/etc/projid"
#define PROJECT_PATHS	"/etc/projects"
char *projid_file;
char *projects_file;

static FILE *projects;
static fs_project_t p;
static char projects_buffer[512];

static FILE *project_paths;
static fs_project_path_t pp;
static char project_paths_buffer[1024];

void
setprfiles(void)
{
	if (!projid_file)
		projid_file = PROJID;
	if (!projects_file)
		projects_file = PROJECT_PATHS;
}

void
setprent()
{
	setprfiles();
	projects = fopen(projid_file, "r");
}

void
setprpathent()
{
	setprfiles();
	project_paths = fopen(projects_file, "r");
}

void
endprent(void)
{
	if (projects)
		fclose(projects);
	projects = NULL;
}

void
endprpathent(void)
{
	if (project_paths)
		fclose(project_paths);
	project_paths = NULL;
}

fs_project_t *
getprent(void)
{
	char	*idstart, *idend;
	size_t	size = sizeof(projects_buffer) - 1;

	if (!projects)
		return NULL;
	do {
		if (!fgets(projects_buffer, size, projects))
			break;
		/*
		 * /etc/projid file format -- "name:id\n", ignore "^#..."
		 */
		if (projects_buffer[0] == '#')
			continue;
		idstart = strchr(projects_buffer, ':');
		if (!idstart)
			continue;
		if ((idstart + 1) - projects_buffer >= size)
			continue;
		idend = strchr(idstart+1, ':');
		if (idend)
			*idend = '\0';
		*idstart = '\0';
		p.pr_prid = atoi(idstart+1);
		p.pr_name = &projects_buffer[0];
		return &p;
	} while (1);

	return NULL;
}

fs_project_t *
getprnam(
	char		*name)
{
	fs_project_t	*p = NULL;

	setprent();
	while ((p = getprent()) != NULL)
		if (strcmp(p->pr_name, name) == 0)
			break;
	endprent();
	return p;
}

fs_project_t *
getprprid(
	prid_t		prid)
{
	fs_project_t	*p = NULL;

	setprent();
	while ((p = getprent()) != NULL)
		if (p->pr_prid == prid)
			break;
	endprent();
	return p;
}

fs_project_path_t *
getprpathent(void)
{
	char		*nmstart, *nmend;
	size_t		size = sizeof(project_paths_buffer) - 1;

	if (!project_paths)
		return NULL;
	do {
		if (!fgets(project_paths_buffer, size, project_paths))
			break;
		/*
		 * /etc/projects format -- "id:pathname\n", ignore "^#..."
		 */
		if (project_paths_buffer[0] == '#')
			continue;
		nmstart = strchr(project_paths_buffer, ':');
		if (!nmstart)
			continue;
		if ((nmstart + 1) - project_paths_buffer >= size)
			continue;
		nmend = strchr(nmstart + 1, '\n');
		if (nmend)
			*nmend = '\0';
		*nmstart = '\0';
		pp.pp_pathname = nmstart + 1;
		pp.pp_prid = atoi(&project_paths_buffer[0]);
		return &pp;
	} while (1);

	return NULL;
}


int
getprojid(
	const char	*name,
	int		fd,
	prid_t		*projid)
{
#if defined(__sgi__)
	struct stat64	st;
	if (fstat64(fd, &st) < 0) {
		perror("fstat64");
		return -1;
	}
	*projid = st.st_projid;
#else
	if (xfsctl(name, fd, XFS_IOC_GETPROJID, projid)) {
		perror("XFS_IOC_GETPROJID");
		return -1;
	}
#endif
	return 0;
}

int
setprojid(
	const char	*name,
	int		fd,
	prid_t		projid)
{
#if defined(__sgi__)
	return fchproj(fd, projid);
#else
	return xfsctl(name, fd, XFS_IOC_SETPROJID, &projid);
#endif
}
