#
# Copyright (c) 2000-2003 Silicon Graphics, Inc.  All Rights Reserved.
#

TOPDIR = ..
include $(TOPDIR)/include/builddefs

LTCOMMAND = xfs_copy
CFILES = xfs_copy.c
HFILES = xfs_copy.h

LLDLIBS = $(LIBXFS) $(LIBUUID) $(LIBPTHREAD)
LTDEPENDENCIES = $(LIBXFS)
LLDFLAGS = -static

default: $(LTCOMMAND)

include $(BUILDRULES)

install: default
	$(INSTALL) -m 755 -d $(PKG_BIN_DIR)
	$(LTINSTALL) -m 755 $(LTCOMMAND) $(PKG_BIN_DIR)
install-dev:
