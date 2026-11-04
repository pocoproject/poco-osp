//
// NetBundleActivatorTest.h
//
// Definition of the NetBundleActivatorTest class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef NetBundleActivatorTest_INCLUDED
#define NetBundleActivatorTest_INCLUDED


#include "CppUnit/TestCase.h"
#include <string>


class NetBundleActivatorTest: public CppUnit::TestCase
{
public:
	NetBundleActivatorTest() = delete;
	explicit NetBundleActivatorTest(const std::string& name);
	~NetBundleActivatorTest() override;

	void testStopClearsProxyConfiguration();

	void setUp() override;
	void tearDown() override;

	static CppUnit::Test* suite();
};


#endif // NetBundleActivatorTest_INCLUDED
