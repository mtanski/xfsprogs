#
# Copyright (c) 2000-2005 Silicon Graphics, Inc.  All Rights Reserved.
#

TOPDIR = ..
include $(TOPDIR)/include/builddefs

LTCOMMAND = xfs_db

HFILES = addr.h agf.h agfl.h agi.h attr.h attrshort.h bit.h block.h bmap.h \
	bmapbt.h bmroot.h bnobt.h check.h cntbt.h command.h convert.h \
	dbread.h debug.h dir.h dir2.h dir2sf.h dirshort.h dquot.h echo.h \
	faddr.h field.h flist.h fprint.h frag.h freesp.h hash.h help.h \
	init.h inobt.h inode.h input.h io.h malloc.h output.h \
	print.h quit.h sb.h sig.h strvec.h text.h type.h write.h \
	attrset.h
CFILES = $(HFILES:.h=.c)
LSRCFILES = xfs_admin.sh xfs_check.sh xfs_ncheck.sh
LLDLIBS	= $(LIBXFS) $(LIBXLOG) $(LIBUUID) $(LIBRT)
LTDEPENDENCIES = $(LIBXFS) $(LIBXLOG)
LLDFLAGS += -static

ifeq ($(ENABLE_READLINE),yes)
LLDLIBS += $(LIBREADLINE) $(LIBTERMCAP)
CFLAGS += -DENABLE_READLINE
endif

ifeq ($(ENABLE_EDITLINE),yes)
LLDLIBS += $(LIBEDITLINE) $(LIBTERMCAP)
CFLAGS += -DENABLE_EDITLINE
endif

default: $(LTCOMMAND)

include $(BUILDRULES)

install: default
	$(INSTALL) -m 755 -d $(PKG_BIN_DIR)
	$(LTINSTALL) -m 755 $(LTCOMMAND) $(PKG_BIN_DIR)
	$(INSTALL) -m 755 xfs_admin.sh $(PKG_BIN_DIR)/xfs_admin
	$(INSTALL) -m 755 xfs_check.sh $(PKG_BIN_DIR)/xfs_check
	$(INSTALL) -m 755 xfs_ncheck.sh $(PKG_BIN_DIR)/xfs_ncheck
install-dev:
