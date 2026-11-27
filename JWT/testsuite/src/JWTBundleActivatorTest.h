//
// JWTBundleActivatorTest.h
//
// Definition of the JWTBundleActivatorTest class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef JWTBundleActivatorTest_INCLUDED
#define JWTBundleActivatorTest_INCLUDED


#include "CppUnit/TestCase.h"
#include <string>


class JWTBundleActivatorTest: public CppUnit::TestCase
{
public:
	JWTBundleActivatorTest() = delete;
	explicit JWTBundleActivatorTest(const std::string& name);
	~JWTBundleActivatorTest() override;

	void testStopRemovesOnlyItsOwnService();

	void setUp() override;
	void tearDown() override;

	static CppUnit::Test* suite();
};


#endif // JWTBundleActivatorTest_INCLUDED
