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

MANIFEST=src-manifest
SRCTAR=$(PACKAGE_NAME)-$(PACKAGE_VERSION).src.tar.gz
SRCTAR2=$(PACKAGE_NAME)_$(PACKAGE_VERSION).orig.tar.gz

LDIRT = $(MANIFEST) $(SRCTAR) $(SRCTAR2) bin-manifest $(TOPDIR)/$(PACKAGE_NAME)-$(PACKAGE_VERSION)

# for clean and clobber
SUBDIRS = tar rpm deb

# nothing to build here (it's all packaging)
default install :

include $(BUILDRULES)

# Symlink in the TOPDIR is used to pack files relative to
# product-version directory.
$(MANIFEST) : $(_FORCE)
	@if [ ! -L $(TOPDIR)/$(PACKAGE_NAME)-$(PACKAGE_VERSION) ] ; then \
	    $(LN_S) . $(TOPDIR)/$(PACKAGE_NAME)-$(PACKAGE_VERSION) ; \
	fi
	@CDIR=`pwd`; cd $(TOPDIR); \
	$(MAKE) --no-print-directory source | \
	    sed -e 's/^\./$(PACKAGE_NAME)-$(PACKAGE_VERSION)/' > $$CDIR/$@ ;\
	if [ $$?  -ne 0 ] ; then  \
	    exit 1; \
	else \
	    unset TAPE; \
	    $(TAR) -T $$CDIR/$@ -cf - | $(ZIP) --best > $$CDIR/$(SRCTAR); \
	    $(LN_S) $$CDIR/$(SRCTAR) $$CDIR/$(SRCTAR2); \
	fi

dist : default $(MANIFEST)
	@DIST_MANIFEST=`pwd`/bin-manifest; DIST_ROOT=/tmp/$$$$; \
	export DIST_MANIFEST DIST_ROOT; \
	rm -f $$DIST_MANIFEST; \
	echo === install === && $(MAKE) -C $(TOPDIR) install || exit $$?; \
	if [ -x $(TAR) ]; then \
	    ( echo "=== tar ===" && $(MAKEF) -C tar $@ || exit $$? ); \
	fi; \
	if [ -x $(RPM) ]; then \
	    ( echo "=== rpm ===" && $(MAKEF) -C rpm $@ || exit $$? ); \
	fi; \
	test -z "$$KEEP_DIST_ROOT" || rm -rf $$DIST_ROOT; echo Done
