#
# Copyright (c) 2000-2006 Silicon Graphics, Inc.
# All Rights Reserved.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it would be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write the Free Software Foundation,
# Inc.,  51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
#

TOPDIR = ..
include $(TOPDIR)/include/builddefs

HFILES = cache.h handle.h jdm.h libxfs.h libxlog.h parent.h xfs.h xqm.h \
	xfs_ag.h xfs_alloc.h xfs_alloc_btree.h xfs_arch.h xfs_attr_leaf.h \
	xfs_attr_sf.h xfs_bit.h xfs_bmap.h xfs_bmap_btree.h xfs_btree.h \
	xfs_buf_item.h xfs_da_btree.h xfs_dfrag.h xfs_dinode.h \
	xfs_dir2.h xfs_dir2_block.h xfs_dir2_data.h xfs_dir2_leaf.h \
	xfs_dir2_node.h xfs_dir2_sf.h xfs_dir_leaf.h xfs_dir_sf.h \
	xfs_extfree_item.h xfs_fs.h xfs_ialloc.h xfs_ialloc_btree.h \
	xfs_imap.h xfs_inode.h xfs_inode_item.h xfs_inum.h xfs_list.h \
	xfs_log.h xfs_log_priv.h xfs_log_recover.h xfs_mount.h xfs_quota.h \
	xfs_rtalloc.h xfs_sb.h xfs_trans.h xfs_trans_space.h xfs_types.h

HFILES += $(PKG_PLATFORM).h swab.h
PHFILES = darwin.h freebsd.h irix.h linux.h
DKHFILES = volume.h fstyp.h dvh.h
LSRCFILES = $(shell echo $(PHFILES) | sed -e "s/$(PKG_PLATFORM).h//g")
LSRCFILES += platform_defs.h.in builddefs.in buildmacros buildrules $(DKHFILES)
LSRCFILES += command.h input.h path.h project.h
LDIRT = xfs disk

default install :
	rm -f xfs disk
	$(LN_S) . xfs
	$(LN_S) . disk

include $(BUILDRULES)

install-dev: default
	$(INSTALL) -m 755 -d $(PKG_INC_DIR)
	$(INSTALL) -m 644 $(HFILES) $(PKG_INC_DIR)
	$(INSTALL) -m 644 platform_defs.h $(PKG_INC_DIR)
	$(INSTALL) -m 755 -d $(DK_INC_DIR)
	$(INSTALL) -m 644 $(DKHFILES) $(DK_INC_DIR)
