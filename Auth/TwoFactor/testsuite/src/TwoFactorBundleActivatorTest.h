//
// TwoFactorBundleActivatorTest.h
//
// Definition of the TwoFactorBundleActivatorTest class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef TwoFactorBundleActivatorTest_INCLUDED
#define TwoFactorBundleActivatorTest_INCLUDED


#include "CppUnit/TestCase.h"
#include <memory>
#include <string>


class TwoFactorBundleActivatorTest: public CppUnit::TestCase
{
public:
	TwoFactorBundleActivatorTest() = delete;
	explicit TwoFactorBundleActivatorTest(const std::string& name);
	~TwoFactorBundleActivatorTest() override;

	void testServiceUsesAuthServiceRegisteredAtCall();
	void testServiceWithoutAuthServiceThrows();
	void testGeneratedKeySaltIsKeptInPreferences();
	void testConfiguredKeySaltIsUsed();

	void setUp() override;
	void tearDown() override;

	static CppUnit::Test* suite();

private:
	struct Fixture;
	std::unique_ptr<Fixture> _pFixture;
};


#endif // TwoFactorBundleActivatorTest_INCLUDED
