#!/bin/sh -f
#
# Copyright (c) 2000-2001 Silicon Graphics, Inc.  All Rights Reserved.
#

OPTS=" "
DBOPTS=" "
USAGE="usage: xfs_ncheck64 [-sfvV] [-l logdev] [-i ino]... special"

while getopts "b:fi:l:svV" c
do
	case $c in
	s)	OPTS=$OPTS"-s ";;
	i)	OPTS=$OPTS"-i "$OPTARG" ";;
	v)	OPTS=$OPTS"-v ";;
	V)	OPTS=$OPTS"-V ";;
	f)	DBOPTS=" -f";;
	l)	DBOPTS=$DBOPTS" -l "$OPTARG" ";;
	\?)	echo $USAGE 1>&2
		exit 2
		;;
	esac
done
set -- extra $@
shift $OPTIND
case $# in
	1)	xfs_db64$DBOPTS -r -p xfs_ncheck64 -c "blockget -ns" -c "ncheck$OPTS" $1
		status=$?
		;;
	*)	echo $USAGE 1>&2
		exit 2
		;;
esac
exit $status
