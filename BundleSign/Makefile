#
# Makefile
#
# Makefile for Poco OSP BundleSign Library
#

include $(POCO_BASE)/build/rules/global

SYSLIBS += -lssl -lcrypto

objects = SignUtility SignatureFile \
	BundleSigner BundleVerifier

target         = PocoOSPBundleSign
target_version = 2
target_libs    = PocoOSP PocoZip PocoCrypto PocoUtil PocoXML PocoFoundation

include $(POCO_BASE)/build/rules/lib
