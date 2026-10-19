#
# Makefile
#
# Makefile for Redis Bundle
#

include $(POCO_BASE)/build/rules/global
include $(POCOPLUS_BASE)/OSP/BundleCreator/BundleCreator.make

objects = RedisBundleActivator

target      = poco.redis
target_libs = PocoRedis PocoOSP PocoUtil PocoXML PocoFoundation

postbuild   = $(SET_LD_LIBRARY_PATH) $(BUNDLE_TOOL) -n$(OSNAME) -a$(OSARCH) -o../bundles Redis.bndlspec

include $(POCO_BASE)/build/rules/dylib
