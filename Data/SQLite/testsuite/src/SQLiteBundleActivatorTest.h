//
// SQLiteBundleActivatorTest.h
//
// Definition of the SQLiteBundleActivatorTest class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef SQLiteBundleActivatorTest_INCLUDED
#define SQLiteBundleActivatorTest_INCLUDED


#include "CppUnit/TestCase.h"
#include <string>


class SQLiteBundleActivatorTest: public CppUnit::TestCase
{
public:
	SQLiteBundleActivatorTest() = delete;
	explicit SQLiteBundleActivatorTest(const std::string& name);
	~SQLiteBundleActivatorTest() override;

	void testSoftHeapLimitIsSetInBytes();
	void testFailedStartDoesNotKeepConnector();

	void setUp() override;
	void tearDown() override;

	static CppUnit::Test* suite();
};


#endif // SQLiteBundleActivatorTest_INCLUDED
