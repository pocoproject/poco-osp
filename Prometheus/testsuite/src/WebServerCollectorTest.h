//
// WebServerCollectorTest.h
//
// Definition of the WebServerCollectorTest class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef WebServerCollectorTest_INCLUDED
#define WebServerCollectorTest_INCLUDED


#include "CppUnit/TestCase.h"
#include <string>


class WebServerCollectorTest: public CppUnit::TestCase
{
public:
	WebServerCollectorTest() = delete;
	explicit WebServerCollectorTest(const std::string& name);
	~WebServerCollectorTest() override;

	void testOneHeaderAndOneSamplePerServer();
	void testRemovedServerHasNoSamples();

	void setUp() override;
	void tearDown() override;

	static CppUnit::Test* suite();
};


#endif // WebServerCollectorTest_INCLUDED
