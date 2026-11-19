//
// ShellTestSuite.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "ShellTestSuite.h"
#if defined(OSPSHELL_TEST_CLIENT)
#include "ClientTest.h"
#endif
#if defined(OSPSHELL_TEST_ECHO_LIBRARY)
#include "EchoCommandTest.h"
#endif
#if defined(OSP_SHELL_ENABLE_SSH)
#include "SSHShellConnectionTest.h"
#endif
#include "CommandExtensionPointTest.h"
#include "CommandProcessorTest.h"
#include "LogLevelCommandTest.h"
#include "LoginFailuresTest.h"
#include "ResponseStreamTest.h"
#include "SessionTest.h"
#include "ShellBundleActivatorTest.h"
#include "ShellCommandsTest.h"
#include "ShellConnectionTest.h"
#include "ShellServerTest.h"
#include "CppUnit/Test.h"
#include "CppUnit/TestSuite.h"


CppUnit::Test* ShellTestSuite::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("ShellTestSuite");

	pSuite->addTest(CommandExtensionPointTest::suite());
	pSuite->addTest(CommandProcessorTest::suite());
	pSuite->addTest(LogLevelCommandTest::suite());
	pSuite->addTest(LoginFailuresTest::suite());
	pSuite->addTest(ResponseStreamTest::suite());
	pSuite->addTest(SessionTest::suite());
	pSuite->addTest(ShellBundleActivatorTest::suite());
	pSuite->addTest(ShellCommandsTest::suite());
	pSuite->addTest(ShellConnectionTest::suite());
	pSuite->addTest(ShellServerTest::suite());
#if defined(OSPSHELL_TEST_CLIENT)
	pSuite->addTest(ClientTest::suite());
#endif
#if defined(OSPSHELL_TEST_ECHO_LIBRARY)
	pSuite->addTest(EchoCommandTest::suite());
#endif
#if defined(OSP_SHELL_ENABLE_SSH)
	pSuite->addTest(SSHShellConnectionTest::suite());
#endif

	return pSuite;
}
