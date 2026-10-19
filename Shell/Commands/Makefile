#
# Makefile-Bundle
#
# Makefile for Poco OSP Shell Commands Bundle
#

include $(POCO_BASE)/build/rules/global
include $(POCOPLUS_BASE)/OSP/BundleCreator/BundleCreator.make

objects = BundleActionCommand BundlesCommand InfoCommand \
	InstallCommand ResolveCommand ServicesCommand \
	StartCommand StopCommand UninstallCommand \
	UpgradeCommand \
	CommandFactory

target         = com.appinf.osp.shell.commands
target_version = 1
target_libs    = PocoOSPShell PocoOSP PocoNet PocoUtil PocoXML PocoFoundation

postbuild      = $(SET_LD_LIBRARY_PATH) $(BUNDLE_TOOL) -n$(OSNAME) -a$(OSARCH) -o../../bundles Commands.bndlspec

include $(POCO_BASE)/build/rules/dylib
