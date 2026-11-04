//
// PrometheusTestSuite.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "PrometheusTestSuite.h"
#include "PrometheusBundleActivatorTest.h"
#include "StringExporterTest.h"
#include "WebServerCollectorTest.h"
#include "CppUnit/Test.h"
#include "CppUnit/TestSuite.h"


CppUnit::Test* PrometheusTestSuite::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("PrometheusTestSuite");

	pSuite->addTest(StringExporterTest::suite());
	pSuite->addTest(WebServerCollectorTest::suite());
	pSuite->addTest(PrometheusBundleActivatorTest::suite());

	return pSuite;
}
