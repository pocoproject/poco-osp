//
// ServiceTestSuite.cpp
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "ServiceTestSuite.h"
#include "ServiceTest.h"
#include "ServiceRegistryTest.h"
#include "ServiceListenerTest.h"


CppUnit::Test* ServiceTestSuite::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("ServiceTestSuite");

	pSuite->addTest(ServiceTest::suite());
	pSuite->addTest(ServiceRegistryTest::suite());
	pSuite->addTest(ServiceListenerTest::suite());

	return pSuite;
}
