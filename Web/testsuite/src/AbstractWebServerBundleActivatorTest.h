//
// AbstractWebServerBundleActivatorTest.h
//
// Definition of the AbstractWebServerBundleActivatorTest class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef AbstractWebServerBundleActivatorTest_INCLUDED
#define AbstractWebServerBundleActivatorTest_INCLUDED


#include "Poco/OSP/Web/Web.h"
#include "CppUnit/TestCase.h"


class AbstractWebServerBundleActivatorTest: public CppUnit::TestCase
{
public:
	AbstractWebServerBundleActivatorTest() = delete;
	explicit AbstractWebServerBundleActivatorTest(const std::string& name);
	~AbstractWebServerBundleActivatorTest() override;

	void setUp() override;
	void tearDown() override;

	void testStartServesRequests();
	void testStopReleasesServerAndThreads();
	void testRestart();
	void testSecureServerService();
	void testPortZeroDisablesServer();
	void testPortOutOfRangeDisablesServer();
	void testWithoutDispatcher();

	static CppUnit::Test* suite();
};


#endif // AbstractWebServerBundleActivatorTest_INCLUDED
