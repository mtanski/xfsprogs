#
# Copyright (c) 2005 Silicon Graphics, Inc.  All Rights Reserved.
#

TOPDIR = ..
include $(TOPDIR)/include/builddefs

LTLIBRARY = libxcmd.la
LT_CURRENT = 0
LT_REVISION = 0
LT_AGE = 0

CFILES = command.c input.c paths.c projects.c help.c quit.c

ifeq ($(HAVE_GETMNTENT),yes)
LCFLAGS += -DHAVE_GETMNTENT
endif

ifeq ($(HAVE_GETMNTINFO),yes)
LCFLAGS += -DHAVE_GETMNTINFO
endif

default: $(LTLIBRARY)

include $(BUILDRULES)

install install-dev install-qa: default
