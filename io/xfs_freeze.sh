#!/bin/sh -f
#
# Copyright (c) 2004 Silicon Graphics, Inc.  All Rights Reserved.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of version 2 of the GNU General Public License as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it would be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#
# Further, this software is distributed without any warranty that it is
# free of the rightful claim of any third person regarding infringement
# or the like.  Any license provided herein, whether implied or
# otherwise, applies only to this software file.  Patent licenses, if
# any, provided herein do not apply to combinations of this program with
# other software, or any other product whatsoever.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write the Free Software Foundation, Inc., 59
# Temple Place - Suite 330, Boston MA 02111-1307, USA.
#
# Contact information: Silicon Graphics, Inc., 1600 Amphitheatre Pkwy,
# Mountain View, CA  94043, or:
#
# http://www.sgi.com
#
# For further information regarding this notice, see:
#
# http://oss.sgi.com/projects/GenInfo/SGIGPLNoticeExplan/
#

OPTS=""
USAGE="Usage: xfs_freeze -f | -u <mountpoint>"
DIRNAME=`dirname $0`
VERSION=false
FREEZE=false
THAW=false

while getopts "fuV" c
do
	case $c in
	f)	FREEZE=true;;
	u)	THAW=true;;
	V)	VERSION=true;;
	\?)	echo $USAGE 1>&2
		exit 2
		;;
	esac
done
if $VERSION ; then
	$DIRNAME/xfs_io -p xfs_freeze -V
	exit 0
fi

shift `expr $OPTIND - 1`
if [ "$1" = "" ]; then
	echo $USAGE 1>&2
	exit 2
fi

if $FREEZE ; then
	$DIRNAME/xfs_io -r -p xfs_freeze -x -c "freeze" "$1"
	status=$?
	[ $status -ne 0 ] && exit $status
elif $THAW ; then
	$DIRNAME/xfs_io -r -p xfs_freeze -x -c "thaw" "$1"
	status=$?
	[ $status -ne 0 ] && exit $status
else
	echo $USAGE 1>&2
	exit 2
fi
exit 0
