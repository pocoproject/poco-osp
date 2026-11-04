//
// JWTValidatorTest.h
//
// Definition of the JWTValidatorTest class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef JWTValidatorTest_INCLUDED
#define JWTValidatorTest_INCLUDED


#include "CppUnit/TestCase.h"
#include <string>


class JWTValidatorTest: public CppUnit::TestCase
{
public:
	JWTValidatorTest() = delete;
	explicit JWTValidatorTest(const std::string& name);
	~JWTValidatorTest() override;

	void testHMACToken();
	void testECTokenWithConfiguredKey();
	void testECTokenWithOtherKeyIsRejected();
	void testAudienceIsChecked();
	void testTokenWithoutSubjectIsRejected();
	void testMalformedTokenIsNotLogged();
	void testMissingKeyIsLoggedAtStart();

	void setUp() override;
	void tearDown() override;

	static CppUnit::Test* suite();
};


#endif // JWTValidatorTest_INCLUDED
