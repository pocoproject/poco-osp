//
// WebEventBundleActivatorTest.h
//
// Definition of the WebEventBundleActivatorTest class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef WebEventBundleActivatorTest_INCLUDED
#define WebEventBundleActivatorTest_INCLUDED


#include "CppUnit/TestCase.h"
#include <string>


class WebEventBundleActivatorTest: public CppUnit::TestCase
{
public:
	WebEventBundleActivatorTest() = delete;
	explicit WebEventBundleActivatorTest(const std::string& name);
	~WebEventBundleActivatorTest() override;

	void testConfigIsReadFromProperties();
	void testConfigKeepsDefaultsWithoutProperties();
	void testActivatorRegistersAndStopsService();
	void testFactoryRequiresService();

	void setUp() override;
	void tearDown() override;

	static CppUnit::Test* suite();
};


#endif // WebEventBundleActivatorTest_INCLUDED
