//
// RemotingNGTCPTestSuite.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "RemotingNGTCPTestSuite.h"
#include "TCPBundleActivatorTest.h"
#include "CppUnit/Test.h"
#include "CppUnit/TestSuite.h"


CppUnit::Test* RemotingNGTCPTestSuite::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("RemotingNGTCPTestSuite");

	pSuite->addTest(TCPBundleActivatorTest::suite());

	return pSuite;
}
