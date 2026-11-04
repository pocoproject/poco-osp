//
// StringExporterTest.h
//
// Definition of the StringExporterTest class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef StringExporterTest_INCLUDED
#define StringExporterTest_INCLUDED


#include "CppUnit/TestCase.h"
#include <string>


class StringExporterTest: public CppUnit::TestCase
{
public:
	StringExporterTest() = delete;
	explicit StringExporterTest(const std::string& name);
	~StringExporterTest() override;

	void testSamplesAreSeparatedBySpaces();
	void testSpecialValues();

	void setUp() override;
	void tearDown() override;

	static CppUnit::Test* suite();
};


#endif // StringExporterTest_INCLUDED
