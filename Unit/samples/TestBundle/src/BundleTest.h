//
// BundleTest.h
//
// Definition of the BundleTest class.
//
// Copyright (c) 2011, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties, 
// copied or duplicated in any form, in whole or in part.
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
