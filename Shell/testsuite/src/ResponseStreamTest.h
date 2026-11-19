//
// ResponseStreamTest.h
//
// Definition of the ResponseStreamTest class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef ResponseStreamTest_INCLUDED
#define ResponseStreamTest_INCLUDED


#include "CppUnit/TestCase.h"
#include <string>


class ResponseStreamTest: public CppUnit::TestCase
{
public:
	ResponseStreamTest() = delete;
	explicit ResponseStreamTest(const std::string& name);
	~ResponseStreamTest() override;

	void setUp() override;
	void tearDown() override;

	void testSingleLineBlock();
	void testMultiLineBlock();
	void testStatusChangeEndsBlock();
	void testFlushEndsBlock();
	void testGetStatus();

	static CppUnit::Test* suite();
};


#endif // ResponseStreamTest_INCLUDED
