/*
 * Copyright (c) 2000-2002 Silicon Graphics, Inc.  All Rights Reserved.
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

/* 
 * Bmap display utility for xfs.
 */

#include <libxfs.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

int aflag = 0;	/* Attribute fork. */
int lflag = 0;	/* list number of blocks with each extent */
int nflag = 0;	/* number of extents specified */
int vflag = 0;	/* Verbose output */
int bmv_iflags = 0;	/* Input flags for XFS_IOC_GETBMAPX */
char *progname;

int dofile(char *);
off64_t file_size(int fd, char * fname);
int numlen(off64_t);

int
main(int argc, char **argv)
{
	char	*fname;
	int	i = 0;
	int	option;

	progname = basename(argv[0]);
	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	while ((option = getopt(argc, argv, "adln:pvV")) != EOF) {
		switch (option) {
		case 'a':
			bmv_iflags |= BMV_IF_ATTRFORK;
			aflag = 1;
			break;
		case 'l':
			lflag = 1;
			break;
		case 'n':
			nflag = atoi(optarg);
			break;
		case 'd':
		/* do not recall possibly offline DMAPI files */
			bmv_iflags |= BMV_IF_NO_DMAPI_READ;
			break;
		case 'p':
		/* report unwritten preallocated blocks */
			bmv_iflags |= BMV_IF_PREALLOC;
			break;
		case 'v':
			vflag++;
			break;
		case 'V':
			printf(_("%s version %s\n"), progname, VERSION);
			exit(0);
		default:
			fprintf(stderr,
				_("Usage: %s [-adlpV] [-n nx] file...\n"),
				progname);
			exit(1);
		}
	}
	if (aflag) 
		bmv_iflags &=  ~(BMV_IF_PREALLOC|BMV_IF_NO_DMAPI_READ);
	while (optind < argc) {
		fname = argv[optind];
		i += dofile(fname);
		optind++;
	}
	return(i ? 1 : 0);
}

off64_t
file_size(int	fd, char *fname)
{
	struct stat64	st;
	int		i;
	int		errno_save;

	errno_save = errno;	/* in case fstat64 fails */
	i = fstat64(fd, &st);
	if (i < 0) {
		fprintf(stderr, _("%s: fstat64 failed for %s: %s\n"),
			progname, fname, strerror(errno));
		errno = errno_save;
		return -1;
	}
	return st.st_size;
}
	

int
dofile(char *fname)
{
	int		i;
	int		fd;
	struct statfs	buf;
	struct fsxattr	fsx;
	struct getbmapx	*map;
	int		map_size;
	int		loop = 0;
	xfs_fsop_geom_v1_t fsgeo;

	fd = open(fname, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, _("%s: cannot open \"%s\": %s\n"),
			progname, fname, strerror(errno));
		return 1;
	}
	fstatfs(fd, &buf);
	if (statfstype(&buf) != XFS_SUPER_MAGIC) {
		fprintf(stderr, _("%s: specified file "
			"[\"%s\"] is not on an XFS filesystem\n"),
			progname, fname);
		close(fd);
		return 1;
        }

	if (vflag) {
		if (ioctl(fd, XFS_IOC_FSGEOMETRY_V1, &fsgeo) < 0) {
			fprintf(stderr,
				_("%s: can't get geometry [\"%s\"]: %s\n"),
				progname, fname, strerror(errno));
			close(fd);
			return 1;
		}
		
		if (vflag > 1)
			printf(_(
	"xfs_bmap: fsgeo.agblocks=%u, fsgeo.blocksize=%u, fsgeo.agcount=%u\n"),
					fsgeo.agblocks, fsgeo.blocksize,
					fsgeo.agcount);

		if ((ioctl(fd, XFS_IOC_FSGETXATTR, &fsx)) < 0) {
			fprintf(stderr,
				_("%s: cannot read attrs on \"%s\": %s\n"),
				progname, fname, strerror(errno));
			close(fd);
			return 1;
		}

		if (vflag > 1)
			printf(_(
    "xfs_bmap: fsx.dsx_xflags=%u, fsx.fsx_extsize=%u, fsx.fsx_nextents=%u\n"),
					fsx.fsx_xflags, fsx.fsx_extsize,
					fsx.fsx_nextents);

		if (fsx.fsx_xflags == XFS_XFLAG_REALTIME) {
			/* 
			 * ag info not applicable to rt, continue
			 * without ag output.
			 */
			vflag = 0;  
		}
	}

	map_size = nflag ? nflag+1 : 32;	/* initial guess - 256 for checkin KCM */
	map = malloc(map_size*sizeof(*map));
	if (map == NULL) {
		fprintf(stderr, _("%s: malloc of %d bytes failed.\n"),
			progname, (int)(map_size * sizeof(*map)));
		close(fd);
		return 1;
	}
		

/*	Try the ioctl(XFS_IOC_GETBMAPX) for the number of extents specified by
 *	nflag, or the initial guess number of extents (256).
 *
 *	If there are more extents than we guessed, use ioctl 
 *	(XFS_IOC_FSGETXATTR[A]) to get the extent count, realloc some more 
 *	space based on this count, and try again.
 *
 *	If the initial FGETBMAPX attempt returns EINVAL, this may mean
 *	that we tried the FGETBMAPX on a zero length file.  If we get
 *	EINVAL, check the length with fstat() and return "no extents"
 *	if the length == 0.
 *
 *	Why not do the ioctl(XFS_IOC_FSGETXATTR[A]) first?  Two reasons:
 *	(1)	The extent count may be wrong for a file with delayed
 *		allocation blocks.  The XFS_IOC_GETBMAPX forces the real
 *		allocation and fixes up the extent count.
 *	(2)	For XFS_IOC_GETBMAP[X] on a DMAPI file that has been moved 
 *		offline by a DMAPI application (e.g., DMF) the 
 *		XFS_IOC_FSGETXATTR only reflects the extents actually online.
 *		Doing XFS_IOC_GETBMAPX call first forces that data blocks online
 *		and then everything proceeds normally (see PV #545725).
 *		
 *		If you don't want this behavior on a DMAPI offline file,
 *		try the "-d" option which sets the BMV_IF_NO_DMAPI_READ
 *		iflag for XFS_IOC_GETBMAPX.
 */

	do {	/* loop a miximum of two times */

		bzero(map, sizeof(*map));	/* zero header */

		map->bmv_length = -1;
		map->bmv_count = map_size;
		map->bmv_iflags = bmv_iflags;

		i = ioctl(fd, XFS_IOC_GETBMAPX, map);

		if (vflag > 1)
			printf(_(
		"xfs_bmap: i=%d map.bmv_offset=%lld, map.bmv_block=%lld, "
		"map.bmv_length=%lld, map.bmv_count=%d, map.bmv_entries=%d\n"),
					i, (long long)map->bmv_offset,
					(long long)map->bmv_block,
					(long long)map->bmv_length,
					map->bmv_count, map->bmv_entries);
		if (i < 0) {
			if (   errno == EINVAL
			    && !aflag && file_size(fd, fname) == 0) {
				break;
			} else	{
				fprintf(stderr, _("%s: ioctl(XFS_IOC_GETBMAPX) "
					"iflags=0x%x [\"%s\"]: %s\n"),
					progname, map->bmv_iflags, fname,
					strerror(errno));
				close(fd);
				free(map);
				return 1;
			}
		}
		if (nflag)
			break;
		if (map->bmv_entries < map->bmv_count-1)
			break;
		/* Get number of extents from ioctl XFS_IOC_FSGETXATTR[A]
		 * syscall.
		 */
		i = ioctl(fd, aflag ? XFS_IOC_FSGETXATTRA : XFS_IOC_FSGETXATTR, &fsx);
		if (i < 0) {
			fprintf(stderr, _("%s: ioctl(XFS_IOC_FSGETXATTR%s) "
				"[\"%s\"]: %s\n"), progname, aflag ? "A" : "",
				fname, strerror(errno));
			close(fd);
			free(map);
			return 1;
		}
		if (fsx.fsx_nextents >= map_size-1) {
			map_size = 2*(fsx.fsx_nextents+1);
			map = realloc(map, map_size*sizeof(*map));
			if (map == NULL) {
				fprintf(stderr,
					_("%s: cannot realloc %d bytes\n"),
					progname, (int)(map_size*sizeof(*map)));
				close(fd);
				return 1;
			}
		}
	} while (++loop < 2);
	if (!nflag) {
		if (map->bmv_entries <= 0) {
			printf(_("%s: no extents\n"), fname);
			close(fd);
			free(map);
			return 0;
		}
	}
	close(fd);
	printf("%s:\n", fname);
	if (!vflag) {
		for (i = 0; i < map->bmv_entries; i++) {
			printf("\t%d: [%lld..%lld]: ", i,
				(long long) map[i + 1].bmv_offset,
				(long long)(map[i + 1].bmv_offset + 
				map[i + 1].bmv_length - 1LL));
			if (map[i + 1].bmv_block == -1)
				printf(_("hole"));
			else {
				printf("%lld..%lld",
					(long long) map[i + 1].bmv_block,
					(long long)(map[i + 1].bmv_block +
						map[i + 1].bmv_length - 1LL));

			}
			if (lflag)
				printf(_(" %lld blocks\n"),
					(long long)map[i+1].bmv_length);
			else
				printf("\n");
		}
	} else {
		/*
		 * Verbose mode displays: 
		 *   extent: [startoffset..endoffset]: startblock..endblock \
		 *   	ag# (agoffset..agendoffset) totalbbs
		 */
#define MINRANGE_WIDTH	16
#define MINAG_WIDTH	2
#define MINTOT_WIDTH	5
#define	max(a,b)	(a > b ? a : b)
		int	  agno;
		off64_t	  agoff, bbperag;
		int 	  foff_w, boff_w, aoff_w, tot_w, agno_w;
		char 	  rbuf[32], bbuf[32], abuf[32];

		foff_w = boff_w = aoff_w = MINRANGE_WIDTH;
		tot_w = MINTOT_WIDTH;
		bbperag = (off64_t)fsgeo.agblocks * 
		          (off64_t)fsgeo.blocksize / BBSIZE;

		/* 
		 * Go through the extents and figure out the width
		 * needed for all columns.
		 */
		for (i = 0; i < map->bmv_entries; i++) {
			snprintf(rbuf, sizeof(rbuf), "[%lld..%lld]:", 
				(long long) map[i + 1].bmv_offset,
				(long long)(map[i + 1].bmv_offset +
				map[i + 1].bmv_length - 1LL));
			if (map[i + 1].bmv_block == -1) {
				foff_w = max(foff_w, strlen(rbuf)); 
				tot_w = max(tot_w, 
					numlen(map[i+1].bmv_length));
			} else {
				snprintf(bbuf, sizeof(bbuf), "%lld..%lld", 
					(long long) map[i + 1].bmv_block,
					(long long)(map[i + 1].bmv_block +
						map[i + 1].bmv_length - 1LL));
				agno = map[i + 1].bmv_block / bbperag;
				agoff = map[i + 1].bmv_block - (agno * bbperag);
				snprintf(abuf, sizeof(abuf), "(%lld..%lld)", 
					(long long)agoff,  (long long)
					(agoff + map[i + 1].bmv_length - 1LL));
				foff_w = max(foff_w, strlen(rbuf)); 
				boff_w = max(boff_w, strlen(bbuf)); 
				aoff_w = max(aoff_w, strlen(abuf)); 
				tot_w = max(tot_w, 
					numlen(map[i+1].bmv_length));
			}
		}
		agno_w = max(MINAG_WIDTH, numlen(fsgeo.agcount));
		printf("%4s: %-*s %-*s %*s %-*s %*s\n", 
			_("EXT"),
			foff_w, _("FILE-OFFSET"),
			boff_w, _("BLOCK-RANGE"),
			agno_w, _("AG"),
			aoff_w, _("AG-OFFSET"),
			tot_w, _("TOTAL"));
		for (i = 0; i < map->bmv_entries; i++) {
			snprintf(rbuf, sizeof(rbuf), "[%lld..%lld]:", 
				(long long) map[i + 1].bmv_offset,
				(long long)(map[i + 1].bmv_offset +
				map[i + 1].bmv_length - 1LL));
			if (map[i + 1].bmv_block == -1) {
				printf("%4d: %-*s %-*s %*s %-*s %*lld\n", 
					i, 
					foff_w, rbuf, 
					boff_w, _("hole"),
					agno_w, "",
					aoff_w, "", 
					tot_w, (long long)map[i+1].bmv_length);
			} else {
				snprintf(bbuf, sizeof(bbuf), "%lld..%lld", 
					(long long) map[i + 1].bmv_block,
					(long long)(map[i + 1].bmv_block +
						map[i + 1].bmv_length - 1LL));
				agno = map[i + 1].bmv_block / bbperag;
				agoff = map[i + 1].bmv_block - (agno * bbperag);
				snprintf(abuf, sizeof(abuf), "(%lld..%lld)", 
					(long long)agoff,  (long long)
					(agoff + map[i + 1].bmv_length - 1LL));
				printf("%4d: %-*s %-*s %*d %-*s %*lld\n", 
					i, 
					foff_w, rbuf, 
					boff_w, bbuf, 
					agno_w, agno, 
					aoff_w, abuf, 
					tot_w, (long long)map[i+1].bmv_length);
			}
		}
	}
	free(map);
	return 0;
}

int
numlen(off64_t val)
{
	off64_t tmp;
	int len;

	for (len=0, tmp=val; tmp > 0; tmp=tmp/10) len++;
	return(len == 0 ? 1 : len);
}
