#
# Copyright (c) 2000-2001 Silicon Graphics, Inc.  All Rights Reserved.
#

TOPDIR = ..
include $(TOPDIR)/include/builddefs

LTCOMMAND = xfs_mkfile
CFILES = xfs_mkfile.c
LLDFLAGS += -static

default: $(LTCOMMAND)

include $(BUILDRULES)

install: default
	$(INSTALL) -m 755 -d $(PKG_BIN_DIR)
	$(LTINSTALL) -m 755 $(LTCOMMAND) $(PKG_BIN_DIR)
install-dev:
