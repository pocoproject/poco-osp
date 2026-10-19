#
# Makefile
#
# $Id: //poco/1.11/OSP/SecureMail/Makefile#1 $
#
# Makefile for OSP Mail Bundle
#

include $(POCO_BASE)/build/rules/global
include $(POCOPLUS_BASE)/OSP/BundleCreator/BundleCreator.make

INCLUDE += -I $(POCOPLUS_BASE)/OSP/Mail/include

objects = SecureMailDeliveryServiceImpl SecureMailBundleActivator

target         = com.appinf.osp.mail.secure
target_version = 1
target_libs    = PocoOSPMail PocoOSP PocoNetSSL PocoNet PocoCrypto PocoUtil PocoXML PocoFoundation

postbuild      = $(SET_LD_LIBRARY_PATH) $(BUNDLE_TOOL) -n$(OSNAME) -a$(OSARCH) -o../bundles SecureMail.bndlspec

include $(POCO_BASE)/build/rules/dylib
