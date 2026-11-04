//
// LoginFailuresTest.h
//
// Definition of the LoginFailuresTest class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef LoginFailuresTest_INCLUDED
#define LoginFailuresTest_INCLUDED


#include "CppUnit/TestCase.h"
#include <string>


class LoginFailuresTest: public CppUnit::TestCase
{
public:
	LoginFailuresTest() = delete;
	explicit LoginFailuresTest(const std::string& name);
	~LoginFailuresTest() override;

	void setUp() override;
	void tearDown() override;

	void testDelayDoublesUpToMaximum();
	void testFirstDelayIsConfigurable();
	void testAddressesAreCountedSeparately();

	static CppUnit::Test* suite();
};


#endif // LoginFailuresTest_INCLUDED
