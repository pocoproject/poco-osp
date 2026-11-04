//
// BundleAdminTestSuite.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "BundleAdminTestSuite.h"
#include "BundleAdminTest.h"
#include "CppUnit/Test.h"
#include "CppUnit/TestSuite.h"


CppUnit::Test* BundleAdminTestSuite::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("BundleAdminTestSuite");

	pSuite->addTest(BundleAdminTest::suite());

	return pSuite;
}
