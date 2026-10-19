#
# Makefile
#
# Makefile for AuthCommand
#

include $(POCO_BASE)/build/rules/global
include $(POCOPLUS_BASE)/OSP/BundleCreator/BundleCreator.make

objects = AuthCommand

target         = com.appinf.osp.auth.command
target_version = 1
target_libs    = PocoOSPAuth PocoOSPShell PocoOSP PocoUtil PocoXML PocoFoundation

postbuild      = $(SET_LD_LIBRARY_PATH) $(BUNDLE_TOOL) -n$(OSNAME) -a$(OSARCH) -o../../bundles AuthCommand.bndlspec

include $(POCO_BASE)/build/rules/dylib
