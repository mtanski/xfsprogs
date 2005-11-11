#
# Copyright (c) 2000-2004 Silicon Graphics, Inc.  All Rights Reserved.
#

TOPDIR = ..
include $(TOPDIR)/include/builddefs

LTCOMMAND = mkfs.xfs
FSTYP = fstyp

HFILES = xfs_mkfs.h
CFILES = maxtrres.c proto.c xfs_mkfs.c 

LLDLIBS = $(LIBXFS) $(LIBUUID) $(LIBDISK)
LTDEPENDENCIES = $(LIBXFS) $(LIBDISK)
LLDFLAGS = -static

LSRCFILES = $(FSTYP).c
LDIRT = $(FSTYP)

default: $(FSTYP) $(LTCOMMAND)

include $(BUILDRULES)

$(FSTYP):
	$(LTLINK) $@.c -o $@ $(CFLAGS) $(LDFLAGS) $(LIBDISK) $(PLDLIBS)

install: default
	$(INSTALL) -m 755 -d $(PKG_SBIN_DIR)
	$(LTINSTALL) -m 755 $(LTCOMMAND) $(PKG_SBIN_DIR)
install-dev:
