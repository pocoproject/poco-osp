//
// SecretGeneratorTest.h
//
// Definition of the SecretGeneratorTest class.
//
// Copyright (c) 2020, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties,
// copied or duplicated in any form, in whole or in part.
//


#ifndef SecretGeneratorTest_INCLUDED
#define SecretGeneratorTest_INCLUDED


#include "Poco/OSP/Auth/Auth.h"
#include "CppUnit/TestCase.h"


class SecretGeneratorTest: public CppUnit::TestCase
{
public:
	SecretGeneratorTest(const std::string& name);
	~SecretGeneratorTest();

	void testGenerate();
	void testEncode();
	void testDecode();
	void testURI();

	void setUp();
	void tearDown();

	static CppUnit::Test* suite();
};


#endif // SecretGeneratorTest_INCLUDED
