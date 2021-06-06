
all: default-build

MAKEFILEDIR := $(shell rdlib-config --makefiles)

include $(MAKEFILEDIR)/makefile.init

EXTRA_CFLAGS   += -std=c99
EXTRA_CXXFLAGS += -std=c++11

EXTRA_CFLAGS   += $(call pkgcflags,rdlib-0.1)
EXTRA_CXXFLAGS += $(call pkgcxxflags,rdlib-0.1)
EXTRA_LIBS	   += $(call pkglibs,rdlib-0.1)

DYNAMIC_EXTRA_LIBS := $(EXTRA_LIBS)

INITIAL_COMMON_FLAGS := $(LOCAL_COMMON_FLAGS)

APPLICATION	       := etagswrapper
LOCAL_COMMON_FLAGS := $(INITIAL_COMMON_FLAGS) -I$(APPLICATION)
OBJECTS		       := $(APPLICATION:%=%.o)
include $(MAKEFILEDIR)/makefile.app

APPLICATION	       := agrep
LOCAL_COMMON_FLAGS := $(INITIAL_COMMON_FLAGS) -I$(APPLICATION)
OBJECTS		       := $(APPLICATION:%=%.o)
include $(MAKEFILEDIR)/makefile.app

APPLICATION	       := cmdsender
LOCAL_COMMON_FLAGS := $(INITIAL_COMMON_FLAGS) -I$(APPLICATION)
OBJECTS		       := $(APPLICATION:%=%.o)
include $(MAKEFILEDIR)/makefile.app

APPLICATION	       := cmdserver
LOCAL_COMMON_FLAGS := $(INITIAL_COMMON_FLAGS) -I$(APPLICATION)
OBJECTS		       := $(APPLICATION:%=%.o)
include $(MAKEFILEDIR)/makefile.app

APPLICATION	       := list
LOCAL_COMMON_FLAGS := $(INITIAL_COMMON_FLAGS) -I$(APPLICATION)
OBJECTS		       := $(APPLICATION:%=%.o)
include $(MAKEFILEDIR)/makefile.app

APPLICATION	       := uri2uri
LOCAL_COMMON_FLAGS := $(INITIAL_COMMON_FLAGS) -I$(APPLICATION)
OBJECTS		       := $(APPLICATION:%=%.o)
include $(MAKEFILEDIR)/makefile.app

APPLICATION	       := imagediff
LOCAL_COMMON_FLAGS := $(INITIAL_COMMON_FLAGS) -I$(APPLICATION)
OBJECTS		       := $(APPLICATION:%=%.o) ImageDiffer.o
include $(MAKEFILEDIR)/makefile.app

APPLICATION	       := srttracker
LOCAL_COMMON_FLAGS := $(INITIAL_COMMON_FLAGS)
OBJECTS		       := $(APPLICATION:%=%.o)
include $(MAKEFILEDIR)/makefile.app

APPLICATION	       := sqlloganalyzer
LOCAL_COMMON_FLAGS := $(INITIAL_COMMON_FLAGS)
OBJECTS		       := $(APPLICATION:%=%.o)
include $(MAKEFILEDIR)/makefile.app

APPLICATION	       := accounts
LOCAL_COMMON_FLAGS := $(INITIAL_COMMON_FLAGS) -I$(APPLICATION)
OBJECTS		       := $(APPLICATION:%=%.o)
include $(MAKEFILEDIR)/makefile.app

APPLICATION	       := countsmswords
LOCAL_COMMON_FLAGS := $(INITIAL_COMMON_FLAGS) -I$(APPLICATION)
OBJECTS		       := $(APPLICATION:%=%.o)
include $(MAKEFILEDIR)/makefile.app

APPLICATION	       := multilogger
LOCAL_COMMON_FLAGS := $(INITIAL_COMMON_FLAGS)
OBJECTS		       := $(APPLICATION:%=%.o)
include $(MAKEFILEDIR)/makefile.app

# APPLICATION  := userdicgenerator
# OBJECTS	         := $(APPLICATION:%=%.o)
# include $(MAKEFILEDIR)/makefile.app

EXTRA_LIBS	 := $(call staticlib,$(EXTRA_LIBS),rdlib)

APPLICATION	       := picprojectdb
LOCAL_COMMON_FLAGS := $(INITIAL_COMMON_FLAGS)
OBJECTS		       := $(APPLICATION:%=%.o)
include $(MAKEFILEDIR)/makefile.app

APPLICATION	       := rota
LOCAL_COMMON_FLAGS := $(INITIAL_COMMON_FLAGS)
OBJECTS		       := $(APPLICATION:%=%.o)
include $(MAKEFILEDIR)/makefile.app

EXTRA_LIBS	 := $(DYNAMIC_EXTRA_LIBS)

LOCAL_INSTALLED_BINARIES := $(shell find scripts -type f)
LOCAL_INSTALLED_BINARIES := $(LOCAL_INSTALLED_BINARIES:scripts/%=$(INSTALLBINDST)/%)
INSTALLEDBINARIES += $(LOCAL_INSTALLED_BINARIES)
UNINSTALLFILES += $(LOCAL_INSTALLED_BINARIES)

$(INSTALLBINDST)/%: scripts/%
	@$(SUDO) $(MAKEFILEDIR)/copyifnewer "$<" "$@"

include $(MAKEFILEDIR)/makefile.post
