#
# Copyright (c) 2001-2003 Silicon Graphics, Inc.  All Rights Reserved.
#

TOPDIR = ..
include $(TOPDIR)/include/builddefs

# Currently LINGUAS is undefined, so buildmacros provides no targets.
LINGUAS =
LSRCFILES = $(LINGUAS:%=%.po) $(PKG_NAME).pot

# TODO: db/ logprint/
XGETTEXTFILES =	\
		$(TOPDIR)/growfs/explore.c \
		$(TOPDIR)/growfs/xfs_growfs.c \
		$(TOPDIR)/imap/xfs_imap.c \
		$(TOPDIR)/io/bmap.c \
		$(TOPDIR)/io/command.c \
		$(TOPDIR)/io/fadvise.c \
		$(TOPDIR)/io/file.c \
		$(TOPDIR)/io/freeze.c \
		$(TOPDIR)/io/fsync.c \
		$(TOPDIR)/io/help.c \
		$(TOPDIR)/io/init.c \
		$(TOPDIR)/io/inject.c \
		$(TOPDIR)/io/input.c \
		$(TOPDIR)/io/mmap.c \
		$(TOPDIR)/io/open.c \
		$(TOPDIR)/io/pread.c \
		$(TOPDIR)/io/prealloc.c \
		$(TOPDIR)/io/pwrite.c \
		$(TOPDIR)/io/quit.c \
		$(TOPDIR)/io/resblks.c \
		$(TOPDIR)/io/shutdown.c \
		$(TOPDIR)/io/truncate.c \
		$(TOPDIR)/mkfile/xfs_mkfile.c \
		$(TOPDIR)/mkfs/proto.c \
		$(TOPDIR)/mkfs/xfs_mkfs.c \
		$(TOPDIR)/libdisk/drivers.c \
		$(TOPDIR)/libdisk/lvm.c \
		$(TOPDIR)/libdisk/md.c \
		$(TOPDIR)/libxfs/darwin.c \
		$(TOPDIR)/libxfs/freebsd.c \
		$(TOPDIR)/libxfs/init.c \
		$(TOPDIR)/libxfs/irix.c \
		$(TOPDIR)/libxfs/linux.c \
		$(TOPDIR)/libxfs/rdwr.c \
		$(TOPDIR)/libxfs/trans.c \
		$(TOPDIR)/libxfs/util.c \
		$(TOPDIR)/libxlog/util.c \
		$(TOPDIR)/repair/agheader.c \
		$(TOPDIR)/repair/attr_repair.c \
		$(TOPDIR)/repair/avl.c \
		$(TOPDIR)/repair/avl64.c \
		$(TOPDIR)/repair/bmap.c \
		$(TOPDIR)/repair/dino_chunks.c \
		$(TOPDIR)/repair/dinode.c \
		$(TOPDIR)/repair/dir.c \
		$(TOPDIR)/repair/dir2.c \
		$(TOPDIR)/repair/dir_stack.c \
		$(TOPDIR)/repair/incore.c \
		$(TOPDIR)/repair/incore_ext.c \
		$(TOPDIR)/repair/incore_ino.c \
		$(TOPDIR)/repair/init.c \
		$(TOPDIR)/repair/io.c \
		$(TOPDIR)/repair/phase1.c \
		$(TOPDIR)/repair/phase2.c \
		$(TOPDIR)/repair/phase3.c \
		$(TOPDIR)/repair/phase4.c \
		$(TOPDIR)/repair/phase5.c \
		$(TOPDIR)/repair/phase6.c \
		$(TOPDIR)/repair/phase7.c \
		$(TOPDIR)/repair/rt.c \
		$(TOPDIR)/repair/sb.c \
		$(TOPDIR)/repair/scan.c \
		$(TOPDIR)/repair/versions.c \
		$(TOPDIR)/repair/xfs_repair.c \
		$(TOPDIR)/rtcp/xfs_rtcp.c

default: $(LINGUAS:%=%.mo)

include $(BUILDRULES)

install: default
	$(INSTALL_LINGUAS)

install-dev install-lib:
