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
 */

#include <xfs/libxfs.h>
#include <xfs/command.h>
#include <xfs/input.h>
#include <sys/mman.h>
#include "init.h"
#include "io.h"

static cmdinfo_t mincore_cmd;

int
mincore_f(
	int		argc,
	char		**argv)
{
	off64_t		offset;
	size_t		length;
	void		*start;
	void		*current, *previous;
	unsigned char	*vec;
	int		i;
	unsigned int	blocksize, sectsize;

	if (argc == 1) {
		offset = mapping->offset;
		length = mapping->length;
	} else if (argc == 3) {
		init_cvtnum(&blocksize, &sectsize);
		offset = cvtnum(blocksize, sectsize, argv[1]);
		if (offset < 0) {
			printf(_("non-numeric offset argument -- %s\n"),
				argv[1]);
			return 0;
		}
		length = cvtnum(blocksize, sectsize, argv[2]);
		if (length < 0) {
			printf(_("non-numeric length argument -- %s\n"),
				argv[2]);
			return 0;
		}
	} else {
		return command_usage(&mincore_cmd);
	}

	start = check_mapping_range(mapping, offset, length, 1);
	if (!start)
		return 0;

	vec = calloc(length/pagesize, sizeof(unsigned char));
	if (!vec) {
		perror("calloc");
		return 0;
	}

	if (mincore(start, length, vec) < 0) {
		perror("mincore");
		free(vec);
		return 0;
	}

	previous = NULL;
	current = start;
	for (i = 0; i < length/pagesize; i++, current += pagesize) {
		if (vec[i]) {
			if (!previous) {	/* print start address */
				printf("0x%lx - ", (unsigned long)current);
				previous = start + (i * pagesize);
			}
		} else if (previous) {		/* print end and page count */
			printf(_("0x%lx  %lu pages (%llu : %lu)\n"),
				(unsigned long)current,
				(unsigned long)(current - previous) / pagesize,
				(unsigned long long)offset +
					(unsigned long long)(previous - start),
				(unsigned long)(current - previous));
			previous = NULL;
		}
	}
	if (previous)
		printf(_("0x%lx  %lu pages (%llu : %lu)\n"),
			(unsigned long)current,
			(unsigned long)(current - previous) / pagesize,
			(unsigned long long)offset +
				(unsigned long long)(previous - start),
			(unsigned long)(current - previous));

	free(vec);
	return 0;
}

void
mincore_init(void)
{
	mincore_cmd.name = _("mincore");
	mincore_cmd.altname = _("mi");
	mincore_cmd.cfunc = mincore_f;
	mincore_cmd.argmin = 0;
	mincore_cmd.argmax = 2;
	mincore_cmd.flags = CMD_NOFILE_OK | CMD_FOREIGN_OK;
	mincore_cmd.args = _("[off len]");
	mincore_cmd.oneline = _("find mapping pages that are memory resident");

	add_command(&mincore_cmd);
}
