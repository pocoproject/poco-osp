//
// SampleConfigurationTest.h
//
// Definition of the SampleConfigurationTest class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef SampleConfigurationTest_INCLUDED
#define SampleConfigurationTest_INCLUDED


#include "CppUnit/TestCase.h"


class SampleConfigurationTest: public CppUnit::TestCase
{
public:
	SampleConfigurationTest() = delete;
	explicit SampleConfigurationTest(const std::string& name);
	~SampleConfigurationTest() override;

	void testBundleServerListensOnLoopbackAddress();
	void testBundleContainerListensOnLoopbackAddress();
	void testBundleServerVerifiesServerCertificates();

	static CppUnit::Test* suite();

private:
	void checkWebServersListenOnLoopbackAddress(const std::string& program);
};


#endif // SampleConfigurationTest_INCLUDED
