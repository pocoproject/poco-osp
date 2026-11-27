//
// OSPUtilTestSuite.cpp
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "OSPUtilTestSuite.h"
#include "VersionRangeTest.h"
#include "PropertiesTest.h"
#include "QLParserTest.h"
#include "BundleStreamFactoryTest.h"
#include "PreferencesTest.h"
#include "LanguageTagTest.h"
#include "LoginThrottleTest.h"


CppUnit::Test* OSPUtilTestSuite::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("OSPUtilTestSuite");

	pSuite->addTest(VersionRangeTest::suite());
	pSuite->addTest(PropertiesTest::suite());
	pSuite->addTest(QLParserTest::suite());
	pSuite->addTest(BundleStreamFactoryTest::suite());
	pSuite->addTest(PreferencesTest::suite());
	pSuite->addTest(LanguageTagTest::suite());
	pSuite->addTest(LoginThrottleTest::suite());

	return pSuite;
}
