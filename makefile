
all: default-build

MAKEFILEDIR = /usr/local/share/rdlib-0.1/makefiles

include $(MAKEFILEDIR)/makefile.init

APPLICATION  := etagswrapper
LOCAL_CFLAGS += -I$(APPLICATION)
OBJECTS      := $(APPLICATION:%=%.o)
include $(MAKEFILEDIR)/makefile.app

EXTRA_CFLAGS += $(call pkgcflags,rdlib-0.1)
EXTRA_LIBS   += $(call pkglibs,rdlib-0.1)

DYNAMIC_EXTRA_LIBS := $(EXTRA_LIBS)

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
OBJECTS      := $(APPLICATION:%=%.o)
include $(MAKEFILEDIR)/makefile.app

APPLICATION  := sqlloganalyzer
OBJECTS      := $(APPLICATION:%=%.o)
include $(MAKEFILEDIR)/makefile.app

APPLICATION  := accounts
LOCAL_CFLAGS += -I$(APPLICATION)
OBJECTS      := $(APPLICATION:%=%.o)
include $(MAKEFILEDIR)/makefile.app

APPLICATION  := countsmswords
LOCAL_CFLAGS += -I$(APPLICATION)
OBJECTS      := $(APPLICATION:%=%.o)
include $(MAKEFILEDIR)/makefile.app

# APPLICATION  := userdicgenerator
# OBJECTS      := $(APPLICATION:%=%.o)
# include $(MAKEFILEDIR)/makefile.app

EXTRA_LIBS   := $(call staticlib,$(EXTRA_LIBS),rdlib)

APPLICATION  := picprojectdb
OBJECTS      := $(APPLICATION:%=%.o)
include $(MAKEFILEDIR)/makefile.app

EXTRA_LIBS   := $(DYNAMIC_EXTRA_LIBS)

LOCAL_INSTALLED_BINARIES := $(shell find scripts -type f)
LOCAL_INSTALLED_BINARIES := $(LOCAL_INSTALLED_BINARIES:scripts/%=$(INSTALLBINDST)/%)
INSTALLEDBINARIES += $(LOCAL_INSTALLED_BINARIES)
UNINSTALLFILES += $(LOCAL_INSTALLED_BINARIES)

$(INSTALLBINDST)/%: scripts/%
	@$(SUDO) $(MAKEFILEDIR)/copyifnewer "$<" "$@"

include $(MAKEFILEDIR)/makefile.post
