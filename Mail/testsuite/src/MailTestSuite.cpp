//
// MailTestSuite.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "MailTestSuite.h"
#include "MailDeliveryServiceImplTest.h"
#include "CppUnit/Test.h"
#include "CppUnit/TestSuite.h"


CppUnit::Test* MailTestSuite::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("MailTestSuite");

	pSuite->addTest(MailDeliveryServiceImplTest::suite());

	return pSuite;
}
