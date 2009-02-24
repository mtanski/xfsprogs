#
# Copyright (c) 2000-2006 Silicon Graphics, Inc.  All Rights Reserved.
#

TOPDIR = .
HAVE_BUILDDEFS = $(shell test -f $(TOPDIR)/include/builddefs && echo yes || echo no)

ifeq ($(HAVE_BUILDDEFS), yes)
include $(TOPDIR)/include/builddefs
endif

CONFIGURE = configure include/builddefs include/platform_defs.h
LSRCFILES = configure configure.in Makepkgs aclocal.m4 install-sh README VERSION

LDIRT = config.log .dep config.status config.cache confdefs.h conftest* \
	Logs/* built .census install.* install-dev.* *.gz

LIB_SUBDIRS = libxfs libxlog libxcmd libhandle libdisk
TOOL_SUBDIRS = copy db estimate fsck fsr growfs io logprint mkfs quota \
		mdrestore repair rtcp m4 man doc po debian build

SUBDIRS = include $(LIB_SUBDIRS) $(TOOL_SUBDIRS)

default: include/builddefs include/platform_defs.h
ifeq ($(HAVE_BUILDDEFS), no)
	$(MAKE) -C . $@
else
	$(MAKE) $(SUBDIRS)
endif

# tool/lib dependencies
$(LIB_SUBDIRS) $(TOOL_SUBDIRS): include
copy mdrestore: libxfs
db logprint: libxfs libxlog
fsr: libhandle
growfs: libxfs libxcmd
io: libxcmd libhandle
mkfs: libxfs libdisk
quota: libxcmd
repair: libxfs libxlog

ifeq ($(HAVE_BUILDDEFS), yes)
include $(BUILDRULES)
else
clean:	# if configure hasn't run, nothing to clean
endif

configure include/builddefs:
	autoconf
	./configure \
		--prefix=/ \
		--exec-prefix=/ \
		--sbindir=/sbin \
		--bindir=/usr/sbin \
		--libdir=/lib \
		--libexecdir=/usr/lib \
		--enable-lib64=yes \
		--includedir=/usr/include \
		--mandir=/usr/share/man \
		--datadir=/usr/share \
		$$LOCAL_CONFIGURE_OPTIONS
	touch .census

include/platform_defs.h: include/builddefs
## Recover from the removal of $@
	@if test -f $@; then :; else \
		rm -f include/builddefs; \
		$(MAKE) $(AM_MAKEFLAGS) include/builddefs; \
	fi

aclocal.m4::
	aclocal --acdir=`pwd`/m4 --output=$@

install: default $(addsuffix -install,$(SUBDIRS))
	$(INSTALL) -m 755 -d $(PKG_DOC_DIR)
	$(INSTALL) -m 644 README $(PKG_DOC_DIR)

install-dev: default $(addsuffix -install-dev,$(SUBDIRS))

install-qa: install $(addsuffix -install-qa,$(SUBDIRS))

%-install:
	$(MAKE) -C $* install

%-install-dev:
	$(MAKE) -C $* install-dev

%-install-qa:
	$(MAKE) -C $* install-qa

realclean distclean: clean
	rm -f $(LDIRT) $(CONFIGURE)
	rm -rf autom4te.cache Logs
