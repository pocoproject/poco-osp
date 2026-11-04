//
// SecretGeneratorTest.cpp
//
// Copyright (c) 2020, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties,
// copied or duplicated in any form, in whole or in part.
//


#include "Poco/OSP/Auth/TwoFactor/SecretGenerator.h"
#include "Poco/Exception.h"
#include "SecretGeneratorTest.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include <cstddef>


using namespace Poco::OSP::Auth::TwoFactor;
using namespace std::string_literals;


SecretGeneratorTest::SecretGeneratorTest(const std::string& name): CppUnit::TestCase(name)
{
}


SecretGeneratorTest::~SecretGeneratorTest()
{
}


void SecretGeneratorTest::testGenerate()
{
	SecretGenerator generator(80);
	std::string secret = generator.generate();
	assert (secret.size() == 10);

	SecretGenerator generator160(160);
	secret = generator160.generate();
	assert (secret.size() == 20);
}


void SecretGeneratorTest::testInvalidLengthIsRejected()
{
	for (std::size_t bits: {std::size_t{72}, std::size_t{84}})
	{
		try
		{
			SecretGenerator generator(bits);
			fail("a secret length below 80 bits or not a multiple of 8 must be rejected");
		}
		catch (Poco::AssertionViolationException&)
		{
		}
	}
}


void SecretGeneratorTest::testRandomSecretRoundTrip()
{
	SecretGenerator generator(160);
	for (int i = 0; i < 20; ++i)
	{
		const std::string secret = generator.generate();
		assertTrue (SecretGenerator::fromBase32(SecretGenerator::toBase32(secret)) == secret);
	}
}


void SecretGeneratorTest::testEncode()
{
	const std::string secret = "0123456789";
	assert (SecretGenerator::toBase32(secret) == "GAYTEMZUGU3DOOBZ");
}


void SecretGeneratorTest::testDecode()
{
	const std::string base32 = "GAYTEMZUGU3DOOBZ";
	assert (SecretGenerator::fromBase32(base32) == "0123456789");
}


void SecretGeneratorTest::testURI()
{
	const std::string secret = "0123456789";
	const std::string issuer = "Example";
	const std::string account = "alice@google.com";

	assert (SecretGenerator::toURI(secret, issuer, account) == "otpauth://totp/Example:alice@google.com?secret=GAYTEMZUGU3DOOBZ&issuer=Example");
}


void SecretGeneratorTest::setUp()
{
}


void SecretGeneratorTest::tearDown()
{
}


CppUnit::Test* SecretGeneratorTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("SecretGeneratorTest");

	CppUnit_addTest(pSuite, SecretGeneratorTest, testGenerate);
	CppUnit_addTest(pSuite, SecretGeneratorTest, testInvalidLengthIsRejected);
	CppUnit_addTest(pSuite, SecretGeneratorTest, testRandomSecretRoundTrip);
	CppUnit_addTest(pSuite, SecretGeneratorTest, testEncode);
	CppUnit_addTest(pSuite, SecretGeneratorTest, testDecode);
	CppUnit_addTest(pSuite, SecretGeneratorTest, testURI);

	return pSuite;
}
