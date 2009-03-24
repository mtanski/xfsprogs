#
# Copyright (c) 2000-2006 Silicon Graphics, Inc.  All Rights Reserved.
#

TOPDIR = .
HAVE_BUILDDEFS = $(shell test -f $(TOPDIR)/include/builddefs && echo yes || echo no)

ifeq ($(HAVE_BUILDDEFS), yes)
include $(TOPDIR)/include/builddefs
endif

CONFIGURE = aclocal.m4 configure config.guess config.sub \
	    ltmain.sh m4/libtool.m4 m4/ltoptions.m4 m4/ltsugar.m4 \
	    m4/ltversion.m4 m4/lt~obsolete.m4 \
	    include/builddefs include/platform_defs.h
LSRCFILES = configure.in Makepkgs install-sh README VERSION $(CONFIGURE)

LDIRT = config.log .dep config.status config.cache confdefs.h conftest* \
	Logs/* built .census install.* install-dev.* *.gz

LIB_SUBDIRS = libxfs libxlog libxcmd libhandle libdisk
TOOL_SUBDIRS = copy db estimate fsck fsr growfs io logprint mkfs quota \
		mdrestore repair rtcp m4 man doc po debian build

SUBDIRS = include $(LIB_SUBDIRS) $(TOOL_SUBDIRS)

default: configure include/builddefs include/platform_defs.h
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

# Recent versions of libtool require the -i option for copying auxiliary
# files (config.sub, config.guess, install-sh, ltmain.sh), while older
# versions will copy those files anyway, and don't understand -i.
LIBTOOLIZE_INSTALL = `libtoolize -n -i >/dev/null 2>/dev/null && echo -i`

configure include/builddefs:
	libtoolize -c $(LIBTOOLIZE_INSTALL) -f
	cp include/install-sh .
	aclocal -I m4
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
	rm -f include/builddefs include/config.h install-sh libtool
	rm -rf autom4te.cache Logs
