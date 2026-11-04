//
// WebEventTestSuite.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "WebEventTestSuite.h"
#include "WebEventServiceTest.h"
#include "WebEventBundleActivatorTest.h"


CppUnit::Test* WebEventTestSuite::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("WebEventTestSuite");

	pSuite->addTest(WebEventServiceTest::suite());
	pSuite->addTest(WebEventBundleActivatorTest::suite());

	return pSuite;
}
