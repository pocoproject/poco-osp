#
# Makefile
#
# Makefile for OSP Shell Client
#

include $(POCO_BASE)/build/rules/global

# check for editline (libedit) and use it if it's available
ifndef OSPSH_NO_EDITLINE
ifeq ($(OSNAME),Darwin)
OSPSH_HAVE_EDITLINE = 1
else
ifeq ($(OSARCH),$(POCO_HOST_OSARCH))
ifeq ($(shell test -f /usr/include/histedit.h && echo 1),1)
OSPSH_HAVE_EDITLINE = 1
$(info ** Building ospsh with editline (libedit) support)
endif
endif
endif
endif

ifdef OSPSH_HAVE_EDITLINE
EXTRA_CXXFLAGS += -DOSPSH_HAVE_EDITLINE
SYSLIBS += -ledit
endif

objects = Client

target         = ospsh
target_version = 1
target_libs    = PocoUtil PocoXML PocoJSON PocoNet PocoFoundation

include $(POCO_BASE)/build/rules/exec
