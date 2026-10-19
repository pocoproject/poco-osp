#
# Makefile
#
# Makefile for Prometheus Bundle
#

include $(POCO_BASE)/build/rules/global
include $(POCOPLUS_BASE)/OSP/BundleCreator/BundleCreator.make

objects = WebServerCollector WebSessionCollector StringExporter PrometheusBundleActivator

target      = poco.prometheus
target_libs = PocoOSPWeb PocoOSP PocoPrometheus PocoNet PocoUtil PocoJSON PocoXML PocoFoundation

postbuild  = $(SET_LD_LIBRARY_PATH) $(BUNDLE_TOOL) -n$(OSNAME) -a$(OSARCH) -o../bundles Prometheus.bndlspec

include $(POCO_BASE)/build/rules/dylib
