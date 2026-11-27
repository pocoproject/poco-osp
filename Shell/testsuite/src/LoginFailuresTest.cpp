//
// LoginFailuresTest.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "LoginFailuresTest.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "CppUnit/TestCase.h"
#include "CppUnit/Test.h"
#include "Poco/OSP/Shell/LoginFailures.h"
#include "Poco/Net/IPAddress.h"
#include <chrono>
#include <string>


using Poco::OSP::Shell::LoginFailures;
using Poco::Net::IPAddress;
using namespace std::string_literals;


namespace
{
	long milliseconds(std::chrono::milliseconds delay)
	{
		return static_cast<long>(delay.count());
	}
}


LoginFailuresTest::LoginFailuresTest(const std::string& name): CppUnit::TestCase(name)
{
}


LoginFailuresTest::~LoginFailuresTest() = default;


void LoginFailuresTest::setUp()
{
}


void LoginFailuresTest::tearDown()
{
}


void LoginFailuresTest::testDelayDoublesUpToMaximum()
{
	LoginFailures failures;
	const IPAddress address("127.0.0.1"s);
	for (const long expected: {1000L, 2000L, 4000L, 8000L, 16000L, 16000L})
	{
		assertEqual (expected, milliseconds(failures.recordFailure(address)));
	}
}


void LoginFailuresTest::testFirstDelayIsConfigurable()
{
	constexpr std::chrono::milliseconds FIRST_DELAY{50};
	LoginFailures failures(FIRST_DELAY);
	const IPAddress address("127.0.0.1"s);
	for (const long expected: {50L, 100L, 200L, 400L, 800L, 800L})
	{
		assertEqual (expected, milliseconds(failures.recordFailure(address)));
	}
}


void LoginFailuresTest::testAddressesAreCountedSeparately()
{
	LoginFailures failures;
	const IPAddress first("192.0.2.1"s);
	const IPAddress second("192.0.2.2"s);
	assertEqual (1000L, milliseconds(failures.recordFailure(first)));
	assertEqual (2000L, milliseconds(failures.recordFailure(first)));
	assertEqual (1000L, milliseconds(failures.recordFailure(second)));
	assertEqual (4000L, milliseconds(failures.recordFailure(first)));
}


CppUnit::Test* LoginFailuresTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("LoginFailuresTest");

	CppUnit_addTest(pSuite, LoginFailuresTest, testDelayDoublesUpToMaximum);
	CppUnit_addTest(pSuite, LoginFailuresTest, testFirstDelayIsConfigurable);
	CppUnit_addTest(pSuite, LoginFailuresTest, testAddressesAreCountedSeparately);

	return pSuite;
}
