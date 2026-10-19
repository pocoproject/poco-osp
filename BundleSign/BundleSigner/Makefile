#
# Makefile
#
# Makefile for POCO OSP Bundle Signer Tool
#

include $(POCO_BASE)/build/rules/global

SYSLIBS += -lssl -lcrypto

objects = BundleSigner

target         = signbundle
target_version = 1
target_libs    = PocoOSPBundleSign PocoOSP PocoCrypto PocoZip PocoUtil PocoJSON PocoXML PocoFoundation

include $(POCO_BASE)/build/rules/exec
