//
// OSPUtilTestSuite.cpp
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#include "OSPUtilTestSuite.h"
#include "VersionRangeTest.h"
#include "PropertiesTest.h"
#include "QLParserTest.h"
#include "BundleStreamFactoryTest.h"
#include "PreferencesTest.h"
#include "LanguageTagTest.h"


CppUnit::Test* OSPUtilTestSuite::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("OSPUtilTestSuite");

	pSuite->addTest(VersionRangeTest::suite());
	pSuite->addTest(PropertiesTest::suite());
	pSuite->addTest(QLParserTest::suite());
	pSuite->addTest(BundleStreamFactoryTest::suite());
	pSuite->addTest(PreferencesTest::suite());
	pSuite->addTest(LanguageTagTest::suite());

	return pSuite;
}
