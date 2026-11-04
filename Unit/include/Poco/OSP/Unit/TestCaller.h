//
// TestCaller.h
//
// Library: OSP/Unit
// Package: Unit
// Module:  TestCaller
//
// Definition of the TestCaller class.
//
// Copyright (c) 2011, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties,
// copied or duplicated in any form, in whole or in part.
//


#ifndef OSP_Unit_TestCaller_INCLUDED
#define OSP_Unit_TestCaller_INCLUDED


#include "CppUnit/CppUnit.h"
#include "Poco/OSP/Unit/TestCase.h"
#include <memory>


namespace Poco::OSP::Unit {


template <class Fixture>
class TestCaller: public Poco::OSP::Unit::TestCase
	/// TestCaller class. This is a replacement for the
	/// CppUnit::TestCaller that enforces calling of the
	/// OSP Unit's TestCase class.
{
public:
	TestCaller(const TestCaller& other) = delete;
	TestCaller& operator = (const TestCaller& other) = delete;

	using TestMethod = void (Fixture::*)();

	TestCaller(const std::string& name, TestMethod test):
		TestCase(name),
		_test(test),
		_fixture(new Fixture(name))
	{
	}

protected:
	void runTest() override
	{
		(_fixture.get()->*_test)();
	}

	void setUp() override
	{
		_fixture.get()->setUp();
	}

	void tearDown() override
	{
		_fixture.get()->tearDown();
	}

private:
	TestMethod             _test;
	std::unique_ptr<Fixture> _fixture;
};


} // namespace Poco::OSP::Unit


#define OSPUnit_addTest(suite, cls, mth) \
	suite->addTest(new Poco::OSP::Unit::TestCaller<cls>(#mth, &cls::mth))


#endif // OSP_Unit_TestCaller_INCLUDED
