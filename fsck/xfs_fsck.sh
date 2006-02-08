#!/bin/sh -f
#
# Copyright (c) 2006 Silicon Graphics, Inc.  All Rights Reserved.
#

AUTO=false
while getopts ":aA" c
do
	case $c in
	a|A)	AUTO=true;;
	esac
done
if $AUTO; then
	echo "$0: XFS file system."
else
	echo "If you wish to check the consistency of an XFS filesystem or"
	echo "repair a damaged filesystem, see xfs_check(8) and xfs_repair(8)."
fi
exit 0
