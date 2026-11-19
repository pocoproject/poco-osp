//
// TwoFactorTestSuite.cpp
//
// Copyright (c) 2020, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "TwoFactorTestSuite.h"
#include "HOTPGeneratorTest.h"
#include "SecretGeneratorTest.h"
#include "TwoFactorBundleActivatorTest.h"
#include "TwoFactorServiceImplTest.h"


CppUnit::Test* TwoFactorTestSuite::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("TwoFactorTestSuite");

	pSuite->addTest(HOTPGeneratorTest::suite());
	pSuite->addTest(SecretGeneratorTest::suite());
	pSuite->addTest(TwoFactorServiceImplTest::suite());
	pSuite->addTest(TwoFactorBundleActivatorTest::suite());

	return pSuite;
}
