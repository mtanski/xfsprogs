#
# Copyright (c) 2000 Silicon Graphics, Inc.  All Rights Reserved.
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

TOPDIR = ..
include $(TOPDIR)/include/builddefs

LTCOMMAND = mkfs.xfs
MAXTRRES = maxtrres
FSTYP = fstyp

HFILES = xfs_mkfs.h proto.h
CFILES = $(HFILES:.h=.c)

# Note: ordering for libdisk and liblvm is important - we must
# now present liblvm to ld twice :( -> libdisk works around the
# liblvm unresolved symbol problem, but also calls into liblvm.
LLDLIBS = $(LIBXFS) $(LIBUUID) $(LIBLVM) $(LIBDISK) $(LIBLVM)
LTDEPENDENCIES = $(LIBXFS) $(LIBDISK)
LLDFLAGS = -static

MAXTRLIBS = $(LIBXFS) $(LIBUUID)
LSRCFILES = $(MAXTRRES).c $(FSTYP).c
LDIRT = $(MAXTRRES) $(MAXTRRES).h $(FSTYP)

default: $(MAXTRRES).h $(FSTYP) $(LTCOMMAND)

include $(BUILDRULES)

$(MAXTRRES):
	$(LTLINK) $@.c -o $@ $(CFLAGS) $(LDFLAGS) $(MAXTRLIBS)

$(MAXTRRES).h: $(MAXTRRES)
	$(LTEXEC) ./$(MAXTRRES) > $@ || ( rm -f $@ && exit 1 )

$(FSTYP):
	$(LTLINK) $@.c -o $@ $(CFLAGS) $(LDFLAGS) $(LIBDISK)

install: default
	$(INSTALL) -m 755 -d $(PKG_SBIN_DIR)
	$(INSTALL) -m 755 $(LTCOMMAND) $(PKG_SBIN_DIR)
install-dev:
