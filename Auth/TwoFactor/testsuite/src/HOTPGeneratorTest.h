//
// HOTPGeneratorTest.h
//
// Definition of the HOTPGeneratorTest class.
//
// Copyright (c) 2020, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties,
// copied or duplicated in any form, in whole or in part.
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
