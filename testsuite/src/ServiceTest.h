//
// ServiceTest.h
//
// Definition of the ServiceTest class.
//
// Copyright (c) 2021-2025, Aleph ONE Software Engineering LLC.
// All rights reserved.
//
// License: Aleph ONE Software and Source Code License Agreement
//


#ifndef ServiceTest_INCLUDED
#define ServiceTest_INCLUDED


#include "Poco/OSP/OSP.h"
#include "CppUnit/TestCase.h"


class ServiceTest: public CppUnit::TestCase
{
public:
	ServiceTest(const std::string& name);
	~ServiceTest() override;

	void testServiceType();
	void testServiceConfig();
	void testServiceSetConfig();
	void testServiceSetConfigWithRemove();
	void testServiceConfigMerge();
	void testServiceBundleConfig();
	void testServiceNameAfterPreviousInstance();
	void testLoggerIsNamedAfterService();
	void testServiceConfigIncludesConfigurationOfPreferencesService();
	void testServiceFinderFindsServiceOfType();
	void testAuthServiceEnablesEveryUserByDefault();

	void setUp() override;
	void tearDown() override;

	static CppUnit::Test* suite();
};


#endif // ServiceTest_INCLUDED
