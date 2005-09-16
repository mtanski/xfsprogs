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
 * or the like.	 Any license provided herein, whether implied or
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

#include "quota.h"
#include <sys/quota.h>

#ifndef PRJQUOTA
#define PRJQUOTA 2
#endif

static int
xtype_to_qtype(
	uint		type)
{
	switch (type) {
	case XFS_USER_QUOTA:
		return USRQUOTA;
	case XFS_GROUP_QUOTA:
		return GRPQUOTA;
	case XFS_PROJ_QUOTA:
		return PRJQUOTA;
	}
	return 0;
}

static int
xcommand_to_qcommand(
	uint		command)
{
	switch (command) {
	case XFS_QUOTAON:
		return Q_XQUOTAON;
	case XFS_QUOTAOFF:
		return Q_XQUOTAOFF;
	case XFS_GETQUOTA:
		return Q_XGETQUOTA;
	case XFS_SETQLIM:
		return Q_XSETQLIM;
	case XFS_GETQSTAT:
		return Q_XGETQSTAT;
	case XFS_QUOTARM:
		return Q_XQUOTARM;
	case XFS_QSYNC:
		return Q_XQUOTASYNC;
	}
	return 0;
}

int
xfsquotactl(
	int		command,
	const char	*device,
	uint		type,
	uint		id,
	void		*addr)
{
	int		qcommand, qtype;

	qtype = xtype_to_qtype(type);
	qcommand = xcommand_to_qcommand(command);

	return quotactl(QCMD(qcommand, qtype), device, id, addr);
}
