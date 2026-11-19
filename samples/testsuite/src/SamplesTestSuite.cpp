//
// SamplesTestSuite.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "SamplesTestSuite.h"
#include "GreetingServiceTest.h"
#include "SampleBundlesTest.h"
#include "SampleConfigurationTest.h"
#include "SampleProgramsTest.h"
#include "SimpleAuthServiceTest.h"
#include "WebEchoTest.h"
#include "WebEventSampleTest.h"
#include "WebIndexTest.h"
#include "WebInfoTest.h"
#include "WebSessionSampleTest.h"
#include "CppUnit/Test.h"
#include "CppUnit/TestSuite.h"


CppUnit::Test* SamplesTestSuite::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("SamplesTestSuite");

	pSuite->addTest(GreetingServiceTest::suite());
	pSuite->addTest(SampleBundlesTest::suite());
	pSuite->addTest(SampleConfigurationTest::suite());
	pSuite->addTest(SampleProgramsTest::suite());
	pSuite->addTest(SimpleAuthServiceTest::suite());
	pSuite->addTest(WebEchoTest::suite());
	pSuite->addTest(WebEventSampleTest::suite());
	pSuite->addTest(WebIndexTest::suite());
	pSuite->addTest(WebInfoTest::suite());
	pSuite->addTest(WebSessionSampleTest::suite());

	return pSuite;
}
