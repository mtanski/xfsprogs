#
# Copyright (c) 2000-2006 Silicon Graphics, Inc.  All Rights Reserved.
#

TOPDIR = ..
include $(TOPDIR)/include/builddefs

LTLIBRARY = libxfs.la
LT_CURRENT = 0
LT_REVISION = 0
LT_AGE = 0

HFILES = xfs.h init.h xfs_dir2_priv.h crc32defs.h crc32table.h
CFILES = cache.c init.c kmem.c logitem.c radix-tree.c rdwr.c trans.c util.c \
	xfs_alloc.c \
	xfs_alloc_btree.c \
	xfs_btree.c \
	xfs_ialloc.c \
	xfs_inode_buf.c \
	xfs_inode_fork.c \
	xfs_ialloc_btree.c xfs_bmap_btree.c xfs_da_btree.c \
	xfs_dir2.c xfs_dir2_leaf.c xfs_attr_leaf.c xfs_dir2_block.c \
	xfs_dir2_node.c xfs_dir2_data.c xfs_dir2_sf.c xfs_bmap.c \
	xfs_mount.c xfs_rtalloc.c xfs_attr.c xfs_attr_remote.c \
	crc32.c \
	xfs_symlink.c \
	xfs_trans_resv.c

CFILES += $(PKG_PLATFORM).c
PCFILES = darwin.c freebsd.c irix.c linux.c
LSRCFILES = $(shell echo $(PCFILES) | sed -e "s/$(PKG_PLATFORM).c//g")
LSRCFILES += gen_crc32table.c

#
# Tracing flags:
# -DIO_DEBUG		reads and writes of buffers
# -DMEM_DEBUG		all zone memory use
# -DLI_DEBUG		log item (ino/buf) manipulation
# -DXACT_DEBUG		transaction state changes
#
#LCFLAGS +=

FCFLAGS = -I.

LTLIBS = $(LIBPTHREAD) $(LIBRT)

# don't try linking xfs_repair with a debug libxfs.
DEBUG = -DNDEBUG

LDIRT = gen_crc32table crc32table.h crc32selftest

default: crc32selftest ltdepend $(LTLIBRARY)

crc32table.h: gen_crc32table.c
	@echo "    [CC]     gen_crc32table"
	$(Q) $(CC) $(CFLAGS) -o gen_crc32table $<
	@echo "    [GENERATE] $@"
	$(Q) ./gen_crc32table > crc32table.h

# The selftest binary will return an error if it fails. This is made a
# dependency of the build process so that we refuse to build the tools on broken
# systems/architectures. Hence we make sure that xfsprogs will never use a
# busted CRC calculation at build time and hence avoid putting bad CRCs down on
# disk.
crc32selftest: gen_crc32table.c crc32table.h crc32.c
	@echo "    [TEST]    CRC32"
	$(Q) $(CC) $(CFLAGS) -D CRC32_SELFTEST=1 crc32.c -o $@
	$(Q) ./$@

include $(BUILDRULES)

install: default

install-dev: default

install-qa: default

-include .ltdep
