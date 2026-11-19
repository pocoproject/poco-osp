//
// SecretGeneratorTest.h
//
// Definition of the SecretGeneratorTest class.
//
// Copyright (c) 2020, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef SecretGeneratorTest_INCLUDED
#define SecretGeneratorTest_INCLUDED


#include "Poco/OSP/Auth/Auth.h"
#include "CppUnit/TestCase.h"


class SecretGeneratorTest: public CppUnit::TestCase
{
public:
	SecretGeneratorTest(const std::string& name);
	~SecretGeneratorTest() override;

	void testGenerate();
	void testInvalidLengthIsRejected();
	void testRandomSecretRoundTrip();
	void testEncode();
	void testDecode();
	void testURI();

	void setUp() override;
	void tearDown() override;

	static CppUnit::Test* suite();
};


#endif // SecretGeneratorTest_INCLUDED
