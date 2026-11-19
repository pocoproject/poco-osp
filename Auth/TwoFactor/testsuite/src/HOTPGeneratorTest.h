//
// HOTPGeneratorTest.h
//
// Definition of the HOTPGeneratorTest class.
//
// Copyright (c) 2020, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef HOTPGeneratorTest_INCLUDED
#define HOTPGeneratorTest_INCLUDED


#include "Poco/OSP/Auth/Auth.h"
#include "CppUnit/TestCase.h"


class HOTPGeneratorTest: public CppUnit::TestCase
{
public:
	HOTPGeneratorTest(const std::string& name);
	~HOTPGeneratorTest() override;

	void testGenerate();
	void testGenerateTOTPTestVectors();

	void setUp() override;
	void tearDown() override;

	static CppUnit::Test* suite();
};


#endif // HOTPGeneratorTest_INCLUDED
