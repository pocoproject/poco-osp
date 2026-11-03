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
#include "Poco/SHA2Engine.h"
#include "HOTPGeneratorTest.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include <vector>


using namespace Poco::OSP::Auth::TwoFactor;
using namespace std::string_literals;


HOTPGeneratorTest::HOTPGeneratorTest(const std::string& name): CppUnit::TestCase(name)
{
}


HOTPGeneratorTest::~HOTPGeneratorTest() = default;


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


void HOTPGeneratorTest::testGenerateTOTPTestVectors()
{
	// Test vectors from RFC 6238, Appendix B: time in seconds, time step of 30 s, 8 digits.
	struct Vector
	{
		Poco::UInt64 time;
		const char* sha1;
		const char* sha256;
		const char* sha512;
	};
	const std::vector<Vector> vectors =
	{
		{59, "94287082", "46119246", "90693936"},
		{1111111109, "07081804", "68084774", "25091201"},
		{1111111111, "14050471", "67062674", "99943326"},
		{1234567890, "89005924", "91819424", "93441116"},
		{2000000000, "69279037", "90698825", "38618901"},
		{20000000000, "65353130", "77737706", "47863826"}
	};
	constexpr Poco::UInt64 TIME_STEP = 30;

	HOTPGenerator<Poco::SHA1Engine> sha1("12345678901234567890"s, 8);
	HOTPGenerator<Poco::SHA2Engine256> sha256("12345678901234567890123456789012"s, 8);
	HOTPGenerator<Poco::SHA2Engine512> sha512("1234567890123456789012345678901234567890123456789012345678901234"s, 8);
	for (const auto& v: vectors)
	{
		assertEqual (std::string(v.sha1), sha1.generate(v.time/TIME_STEP));
		assertEqual (std::string(v.sha256), sha256.generate(v.time/TIME_STEP));
		assertEqual (std::string(v.sha512), sha512.generate(v.time/TIME_STEP));
	}
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
	CppUnit_addTest(pSuite, HOTPGeneratorTest, testGenerateTOTPTestVectors);

	return pSuite;
}
