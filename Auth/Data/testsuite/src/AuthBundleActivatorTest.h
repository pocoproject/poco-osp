//
// AuthBundleActivatorTest.h
//
// Definition of the AuthBundleActivatorTest class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef AuthBundleActivatorTest_INCLUDED
#define AuthBundleActivatorTest_INCLUDED


#include "CppUnit/TestCase.h"
#include <string>


class AuthBundleActivatorTest: public CppUnit::TestCase
{
public:
	AuthBundleActivatorTest() = delete;
	explicit AuthBundleActivatorTest(const std::string& name);
	~AuthBundleActivatorTest() override;

	void testUnreachableDatabaseIsNotRetriedAfterStartupTimeout();

	void setUp() override;
	void tearDown() override;

	static CppUnit::Test* suite();
};


#endif // AuthBundleActivatorTest_INCLUDED
