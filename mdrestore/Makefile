#
# Copyright (c) 2007 Silicon Graphics, Inc.  All Rights Reserved.
#

TOPDIR = ..
include $(TOPDIR)/include/builddefs

LTCOMMAND = xfs_mdrestore
CFILES = xfs_mdrestore.c

LLDLIBS = $(LIBXFS) $(LIBRT) $(LIBPTHREAD)
LTDEPENDENCIES = $(LIBXFS)
LLDFLAGS = -static

default: $(LTCOMMAND)

include $(BUILDRULES)

install:
	$(INSTALL) -m 755 -d $(PKG_BIN_DIR)
	$(LTINSTALL) -m 755 $(LTCOMMAND) $(PKG_BIN_DIR)
install-dev:
