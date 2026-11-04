//
// BundleTest.h
//
// Definition of the BundleTest class.
//
// Copyright (c) 2011, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef BundleTest_INCLUDED
#define BundleTest_INCLUDED


#include "Poco/OSP/Unit/TestCase.h"


class BundleTest: public Poco::OSP::Unit::TestCase
{
public:
	BundleTest(const std::string& name);
	~BundleTest() override;

	void testSetUp();
	void testSuccess();
	[[noreturn]] void testError();
	[[noreturn]] void testFailure();
	void testTornDown();

	void setUp() override;
	void tearDown() override;

	static CppUnit::Test* suite();

private:
	bool _setUp = false;
	bool _tornDown = false;
};


#endif // BundleTest_INCLUDED
