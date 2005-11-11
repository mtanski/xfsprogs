/*
 * Copyright (c) 2000-2005 Silicon Graphics, Inc.
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

#include "drivers.h"
#include "md.h"

int
mnt_is_md_subvol(
	dev_t		dev)
{
	if (major(dev) == MD_MAJOR)
		return 1;
	return get_driver_block_major("md", major(dev));
}

int
md_get_subvol_stripe(
	char		*dfile,
	sv_type_t	type,
	int		*sunit,
	int		*swidth,
	int		*sectalign,
	struct stat64	*sb)
{
	if (mnt_is_md_subvol(sb->st_rdev)) {
		struct md_array_info	md;
		int			fd;

		/* Open device */
		fd = open(dfile, O_RDONLY);
		if (fd == -1)
			return 0;

		/* Is this thing on... */
		if (ioctl(fd, GET_ARRAY_INFO, &md)) {
			fprintf(stderr,
				_("Error getting MD array info from %s\n"),
				dfile);
			exit(1);
		}
		close(fd);

		/*
		 * Ignore levels we don't want aligned (e.g. linear)
		 * and deduct disk(s) from stripe width on RAID4/5/6
		 */
		switch (md.level) {
		case 6:
			md.nr_disks--;
			/* fallthrough */
		case 5:
		case 4:
			md.nr_disks--;
			/* fallthrough */
		case 1:
		case 0:
		case 10:
			break;
		default:
			return 0;
		}

		/* Update sizes */
		*sunit = md.chunk_size >> 9;
		*swidth = *sunit * md.nr_disks;
		*sectalign = (md.level == 4 || md.level == 5 || md.level == 6);

		return 1;
	}
	return 0;
}
