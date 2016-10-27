
all: default-build

MAKEFILEDIR = /usr/local/share/rdlib-0.1/makefiles

include $(MAKEFILEDIR)/makefile.init

EXTRA_CFLAGS += $(shell pkg-config --cflags rdlib-0.1)
EXTRA_LIBS   += $(shell pkg-config --libs rdlib-0.1)

APPLICATION  := agrep
LOCAL_CFLAGS += -I$(APPLICATION)
OBJECTS      := $(APPLICATION:%=%.o)
include $(MAKEFILEDIR)/makefile.app

APPLICATION  := cmdsender
LOCAL_CFLAGS += -I$(APPLICATION)
OBJECTS      := $(APPLICATION:%=%.o)
include $(MAKEFILEDIR)/makefile.app

APPLICATION  := cmdserver
LOCAL_CFLAGS += -I$(APPLICATION)
OBJECTS      := $(APPLICATION:%=%.o)
include $(MAKEFILEDIR)/makefile.app

APPLICATION  := list
LOCAL_CFLAGS += -I$(APPLICATION)
OBJECTS      := $(APPLICATION:%=%.o)
include $(MAKEFILEDIR)/makefile.app

APPLICATION  := uri2uri
LOCAL_CFLAGS += -I$(APPLICATION)
OBJECTS      := $(APPLICATION:%=%.o)
include $(MAKEFILEDIR)/makefile.app

APPLICATION  := imagediff
LOCAL_CFLAGS += -I$(APPLICATION)
OBJECTS      := $(APPLICATION:%=%.o) ImageDiffer.o
include $(MAKEFILEDIR)/makefile.app

include $(MAKEFILEDIR)/makefile.post
