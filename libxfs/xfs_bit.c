/*
 * Copyright (c) 2000 Silicon Graphics, Inc.  All Rights Reserved.
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
 * XFS bit manipulation routines, used in non-realtime code.
 */

#include <xfs.h>


#ifndef HAVE_ARCH_HIGHBIT
/*
 * Index of high bit number in byte, -1 for none set, 0..7 otherwise.
 */
const char xfs_highbit[256] = {
       -1, 0, 1, 1, 2, 2, 2, 2,			/* 00 .. 07 */
	3, 3, 3, 3, 3, 3, 3, 3,			/* 08 .. 0f */
	4, 4, 4, 4, 4, 4, 4, 4,			/* 10 .. 17 */
	4, 4, 4, 4, 4, 4, 4, 4,			/* 18 .. 1f */
	5, 5, 5, 5, 5, 5, 5, 5,			/* 20 .. 27 */
	5, 5, 5, 5, 5, 5, 5, 5,			/* 28 .. 2f */
	5, 5, 5, 5, 5, 5, 5, 5,			/* 30 .. 37 */
	5, 5, 5, 5, 5, 5, 5, 5,			/* 38 .. 3f */
	6, 6, 6, 6, 6, 6, 6, 6,			/* 40 .. 47 */
	6, 6, 6, 6, 6, 6, 6, 6,			/* 48 .. 4f */
	6, 6, 6, 6, 6, 6, 6, 6,			/* 50 .. 57 */
	6, 6, 6, 6, 6, 6, 6, 6,			/* 58 .. 5f */
	6, 6, 6, 6, 6, 6, 6, 6,			/* 60 .. 67 */
	6, 6, 6, 6, 6, 6, 6, 6,			/* 68 .. 6f */
	6, 6, 6, 6, 6, 6, 6, 6,			/* 70 .. 77 */
	6, 6, 6, 6, 6, 6, 6, 6,			/* 78 .. 7f */
	7, 7, 7, 7, 7, 7, 7, 7,			/* 80 .. 87 */
	7, 7, 7, 7, 7, 7, 7, 7,			/* 88 .. 8f */
	7, 7, 7, 7, 7, 7, 7, 7,			/* 90 .. 97 */
	7, 7, 7, 7, 7, 7, 7, 7,			/* 98 .. 9f */
	7, 7, 7, 7, 7, 7, 7, 7,			/* a0 .. a7 */
	7, 7, 7, 7, 7, 7, 7, 7,			/* a8 .. af */
	7, 7, 7, 7, 7, 7, 7, 7,			/* b0 .. b7 */
	7, 7, 7, 7, 7, 7, 7, 7,			/* b8 .. bf */
	7, 7, 7, 7, 7, 7, 7, 7,			/* c0 .. c7 */
	7, 7, 7, 7, 7, 7, 7, 7,			/* c8 .. cf */
	7, 7, 7, 7, 7, 7, 7, 7,			/* d0 .. d7 */
	7, 7, 7, 7, 7, 7, 7, 7,			/* d8 .. df */
	7, 7, 7, 7, 7, 7, 7, 7,			/* e0 .. e7 */
	7, 7, 7, 7, 7, 7, 7, 7,			/* e8 .. ef */
	7, 7, 7, 7, 7, 7, 7, 7,			/* f0 .. f7 */
	7, 7, 7, 7, 7, 7, 7, 7,			/* f8 .. ff */
};
#endif

/*
 * xfs_highbit32: get high bit set out of 32-bit argument, -1 if none set.
 */
int
xfs_highbit32(
	__uint32_t	v)
{
#ifdef HAVE_ARCH_HIGHBIT
	return highbit32(v);
#else
	int		i;

	if (v & 0xffff0000)
		if (v & 0xff000000)
			i = 24;
		else
			i = 16;
	else if (v & 0x0000ffff)
		if (v & 0x0000ff00)
			i = 8;
		else
			i = 0;
	else
		return -1;
	return i + xfs_highbit[(v >> i) & 0xff];
#endif
}

/*
 * xfs_lowbit64: get low bit set out of 64-bit argument, -1 if none set.
 */
int
xfs_lowbit64(
	__uint64_t	v)
{
	int n;
	n = ffs((unsigned)v);
	if (n == 0) { 
		n = ffs(v >> 32);
		if (n >= 0) 
			n+=32;
	}
	return n-1;
}

/*
 * xfs_highbit64: get high bit set out of 64-bit argument, -1 if none set.
 */
int
xfs_highbit64(
	__uint64_t	v)
{
	__uint32_t h = v >> 32; 
	if (h) 
		return xfs_highbit32(h) + 32;
	return xfs_highbit32((__u32)v); 
}
