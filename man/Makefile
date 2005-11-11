#
# Copyright (c) 2000-2001 Silicon Graphics, Inc.  All Rights Reserved.
#

TOPDIR = ..
include $(TOPDIR)/include/builddefs

SUBDIRS = man3 man5 man8

default install install-dev : $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

include $(BUILDRULES)
