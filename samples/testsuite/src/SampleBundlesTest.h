//
// SampleBundlesTest.h
//
// Definition of the SampleBundlesTest class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef SampleBundlesTest_INCLUDED
#define SampleBundlesTest_INCLUDED


#include "CppUnit/TestCase.h"


class SampleBundlesTest: public CppUnit::TestCase
{
public:
	SampleBundlesTest() = delete;
	explicit SampleBundlesTest(const std::string& name);
	~SampleBundlesTest() override;

	void testDataOnlyBundlesContainNoLibrary();
	void testExtensionBundleExtendsExtensibleBundle();
	void testWebPageBundleServesPage();

	static CppUnit::Test* suite();
};


#endif // SampleBundlesTest_INCLUDED
