
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

APPLICATION  := srttracker
LOCAL_CFLAGS += -I$(APPLICATION)
OBJECTS      := $(APPLICATION:%=%.o)
include $(MAKEFILEDIR)/makefile.app

LOCAL_INSTALLED_BINARIES := $(shell find scripts -type f)
LOCAL_INSTALLED_BINARIES := $(LOCAL_INSTALLED_BINARIES:scripts/%=$(INSTALLBINDST)/%)
INSTALLEDBINARIES += $(LOCAL_INSTALLED_BINARIES)
UNINSTALLFILES += $(LOCAL_INSTALLED_BINARIES)

$(INSTALLBINDST)/%: scripts/%
	@$(SUDO) $(MAKEFILEDIR)/copyifnewer "$<" "$@"

include $(MAKEFILEDIR)/makefile.post
