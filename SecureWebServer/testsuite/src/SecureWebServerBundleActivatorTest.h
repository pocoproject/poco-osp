//
// SecureWebServerBundleActivatorTest.h
//
// Definition of the SecureWebServerBundleActivatorTest class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef SecureWebServerBundleActivatorTest_INCLUDED
#define SecureWebServerBundleActivatorTest_INCLUDED


#include "CppUnit/TestCase.h"
#include <string>


class SecureWebServerBundleActivatorTest: public CppUnit::TestCase
{
public:
	SecureWebServerBundleActivatorTest() = delete;
	explicit SecureWebServerBundleActivatorTest(const std::string& name);
	~SecureWebServerBundleActivatorTest() override;

	void testServesRequestsOverTLS();

	void setUp() override;
	void tearDown() override;

	static CppUnit::Test* suite();
};


#endif // SecureWebServerBundleActivatorTest_INCLUDED
