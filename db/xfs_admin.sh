#!/bin/sh -f
#
# Copyright (c) 2000-2001 Silicon Graphics, Inc.  All Rights Reserved.
#

OPTS=""
USAGE="Usage: xfs_admin [-efjluV] [-L label] [-U uuid] special"

while getopts "efjluL:U:V" c
do
	case $c in
	e)	OPTS=$OPTS" -c 'version extflg'";;
	f)	OPTS=$OPTS" -f";;
	j)	OPTS=$OPTS" -c 'version log2'";;
	l)	OPTS=$OPTS" -r -c label";;
	L)	OPTS=$OPTS" -c 'label "$OPTARG"'";;
	u)	OPTS=$OPTS" -r -c uuid";;
	U)	OPTS=$OPTS" -c 'uuid "$OPTARG"'";;
	V)	OPTS=$OPTS" -V";;
	\?)	echo $USAGE 1>&2
		exit 2
		;;
	esac
done
set -- extra $@
shift $OPTIND
case $# in
	1)	eval xfs_db -x -p xfs_admin $OPTS $1
		status=$?
		;;
	*)	echo $USAGE 1>&2
		exit 2
		;;
esac
exit $status
