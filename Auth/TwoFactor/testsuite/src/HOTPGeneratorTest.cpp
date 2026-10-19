//
// HOTPGeneratorTest.cpp
//
// Copyright (c) 2020, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties,
// copied or duplicated in any form, in whole or in part.
//


#include "Poco/OSP/Auth/TwoFactor/HOTPGenerator.h"
#include "Poco/Timestamp.h"
#include "HOTPGeneratorTest.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include <iostream>


using namespace Poco::OSP::Auth::TwoFactor;
using namespace std::string_literals;


HOTPGeneratorTest::HOTPGeneratorTest(const std::string& name): CppUnit::TestCase(name)
{
}


HOTPGeneratorTest::~HOTPGeneratorTest()
{
}


void HOTPGeneratorTest::testGenerate()
{
	// Test vectors from RFC 4226
	const std::string secret = "12345678901234567890";
	const std::vector<std::string> expectedHOTPs = {"755224"s, "287082"s, "359152"s, "969429"s, "338314"s, "254676"s, "287922"s, "162583"s, "399871"s, "520489"s};

	HOTPGenerator<Poco::SHA1Engine> generator(secret);

	for (Poco::UInt64 i = 0; i < expectedHOTPs.size(); i++)
	{
		assert (generator.generate(i) == expectedHOTPs[i]);
	}
}


void HOTPGeneratorTest::testGenerateTOTP()
{
	const std::string secret = "0123456789";
	HOTPGenerator<Poco::SHA1Engine> generator(secret);
	Poco::UInt64 movingFactor = static_cast<Poco::UInt64>(Poco::Timestamp().epochTime())/30;
	std::string code = generator.generate(movingFactor);
	std::cout << code << std::endl;
}


void HOTPGeneratorTest::setUp()
{
}


void HOTPGeneratorTest::tearDown()
{
}


CppUnit::Test* HOTPGeneratorTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("HOTPGeneratorTest");

	CppUnit_addTest(pSuite, HOTPGeneratorTest, testGenerate);
	CppUnit_addTest(pSuite, HOTPGeneratorTest, testGenerateTOTP);

	return pSuite;
}
