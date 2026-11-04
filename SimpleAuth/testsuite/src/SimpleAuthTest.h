//
// SimpleAuthTest.h
//
// Definition of the SimpleAuthTest class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef SimpleAuthTest_INCLUDED
#define SimpleAuthTest_INCLUDED


#include "CppUnit/TestCase.h"


class SimpleAuthTest: public CppUnit::TestCase
{
public:
	SimpleAuthTest() = delete;
	explicit SimpleAuthTest(const std::string& name);
	~SimpleAuthTest() override;

	void testPBKDF2HashIsVerified();
	void testMD5HashIsVerified();
	void testScopedAuthorizationIsDenied();
	void testMissingPreferencesServiceIsHandled();

	void setUp() override;
	void tearDown() override;

	static CppUnit::Test* suite();
};


#endif // SimpleAuthTest_INCLUDED
