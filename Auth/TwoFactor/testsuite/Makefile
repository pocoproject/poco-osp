#
# Makefile
#
# Makefile for Poco OSPAuth TwoFactor testsuite
#

include $(POCO_BASE)/build/rules/global

objects = TwoFactorTestSuite Driver \
	HOTPGeneratorTest \
	SecretGeneratorTest

target         = testrunner
target_version = 1
target_libs    = PocoOSPAuthTwoFactor PocoFoundation CppUnit

include $(POCO_BASE)/build/rules/exec
