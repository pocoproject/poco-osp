//
// AuthTestSuite.cpp
//
// Copyright (c) 2012, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "AuthTestSuite.h"
#include "AuthAdminServiceTest.h"
#include "AuthBundleActivatorTest.h"


CppUnit::Test* AuthTestSuite::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("AuthTestSuite");

	pSuite->addTest(AuthAdminServiceTest::suite());
	pSuite->addTest(InlineAttributesAuthAdminServiceTest::suite());
	pSuite->addTest(AuthBundleActivatorTest::suite());

	return pSuite;
}
