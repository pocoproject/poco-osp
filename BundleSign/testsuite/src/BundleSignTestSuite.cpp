//
// BundleSignTestSuite.cpp
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "BundleSignTestSuite.h"
#include "SignUtilityTest.h"
#include "SignatureFileTest.h"
#include "BundleSignerTest.h"
#include "BundleVerifierTest.h"


CppUnit::Test* BundleSignTestSuite::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("BundleSignTestSuite");

	pSuite->addTest(SignUtilityTest::suite());
	pSuite->addTest(SignatureFileTest::suite());
	pSuite->addTest(BundleSignerTest::suite());
	pSuite->addTest(BundleVerifierTest::suite());

	return pSuite;
}
