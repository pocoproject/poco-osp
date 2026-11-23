//
// WebBundleActivatorTest.h
//
// Definition of the WebBundleActivatorTest class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef WebBundleActivatorTest_INCLUDED
#define WebBundleActivatorTest_INCLUDED


#include "Poco/OSP/Web/Web.h"
#include "CppUnit/TestCase.h"


class WebBundleActivatorTest: public CppUnit::TestCase
{
public:
	WebBundleActivatorTest() = delete;
	explicit WebBundleActivatorTest(const std::string& name);
	~WebBundleActivatorTest() override;

	void setUp() override;
	void tearDown() override;

	void testLoginSettingsOfBundleAndApplication();
	void testInvalidLoginSettingFailsStart();

	static CppUnit::Test* suite();
};


#endif // WebBundleActivatorTest_INCLUDED
