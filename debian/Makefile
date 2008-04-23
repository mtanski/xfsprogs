#
# Copyright (c) 2000-2004 Silicon Graphics, Inc.  All Rights Reserved.
#

TOPDIR = ..
include $(TOPDIR)/include/builddefs

LSRCFILES = changelog compat control copyright rules watch
DEV_DOC_DIR = $(PKG_DOC_DIR)/../xfslibs-dev
BOOT_MKFS_BIN = $(TOPDIR)/mkfs/mkfs.xfs-xfsprogs-udeb

default:

include $(BUILDRULES)

install: default
ifeq ($(PKG_DISTRIBUTION), debian)
	$(INSTALL) -m 755 -d $(PKG_DOC_DIR)
	$(INSTALL) -m 644 changelog $(PKG_DOC_DIR)/changelog.Debian
endif

install-dev: default
ifeq ($(PKG_DISTRIBUTION), debian)
	$(INSTALL) -m 755 -d $(PKG_DOC_DIR)
	$(INSTALL) -m 755 -d $(DEV_DOC_DIR)
	$(INSTALL) -m 644 copyright $(DEV_DOC_DIR)
	$(INSTALL) -m 644 changelog $(DEV_DOC_DIR)/changelog.Debian
endif

install-d-i: default
ifeq ($(PKG_DISTRIBUTION), debian)
	$(INSTALL) -m 755 -d $(PKG_SBIN_DIR)
	$(INSTALL) -m 755 $(BOOT_MKFS_BIN) $(PKG_SBIN_DIR)/mkfs.xfs
endif
