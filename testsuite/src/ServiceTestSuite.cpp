//
// ServiceTestSuite.cpp
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#include "ServiceTestSuite.h"
#include "ServiceTest.h"
#include "ServiceRegistryTest.h"
#include "ServiceListenerTest.h"
#include "WebEventServiceTest.h"


CppUnit::Test* ServiceTestSuite::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("ServiceTestSuite");

	pSuite->addTest(ServiceTest::suite());
	pSuite->addTest(ServiceRegistryTest::suite());
	pSuite->addTest(ServiceListenerTest::suite());
	pSuite->addTest(WebEventServiceTest::suite());

	return pSuite;
}
