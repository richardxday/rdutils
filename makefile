
all: default-build

MAKEFILEDIR = /usr/local/share/rdlib-0.1/makefiles

include $(MAKEFILEDIR)/makefile.init

EXTRA_CFLAGS += $(shell pkg-config --cflags rdlib-0.1)
EXTRA_LIBS   += $(shell pkg-config --libs rdlib-0.1)

APPLICATION := agrep
OBJECTS     := $(APPLICATION:%=%.o)
include $(MAKEFILEDIR)/makefile.app

APPLICATION := cmdsender
OBJECTS     := $(APPLICATION:%=%.o)
include $(MAKEFILEDIR)/makefile.app

APPLICATION := cmdserver
OBJECTS     := $(APPLICATION:%=%.o)
include $(MAKEFILEDIR)/makefile.app

APPLICATION := list
OBJECTS     := $(APPLICATION:%=%.o)
include $(MAKEFILEDIR)/makefile.app

APPLICATION := uri2uri
OBJECTS     := $(APPLICATION:%=%.o)
include $(MAKEFILEDIR)/makefile.app

APPLICATION := imagediff
OBJECTS     := $(APPLICATION:%=%.o) ImageDiffer.o
include $(MAKEFILEDIR)/makefile.app

include $(MAKEFILEDIR)/makefile.post
