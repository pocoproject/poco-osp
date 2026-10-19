#
# Makefile
#
# Makefile for Poco OSP BundleSign testsuite
#

include $(POCO_BASE)/build/rules/global

objects = Driver BundleSignTestSuite \
	SignUtilityTest SignatureFileTest \
	BundleSignerTest BundleVerifierTest

target         = testrunner
target_version = 1
target_libs    = PocoOSPBundleSign PocoOSP PocoZip PocoCrypto PocoUtil PocoXML PocoFoundation CppUnit

include $(POCO_BASE)/build/rules/exec
