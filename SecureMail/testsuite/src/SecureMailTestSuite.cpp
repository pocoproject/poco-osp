//
// SecureMailTestSuite.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "SecureMailTestSuite.h"
#include "SecureMailDeliveryServiceImplTest.h"
#include "CppUnit/Test.h"
#include "CppUnit/TestSuite.h"


CppUnit::Test* SecureMailTestSuite::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("SecureMailTestSuite");

	pSuite->addTest(SecureMailDeliveryServiceImplTest::suite());

	return pSuite;
}
