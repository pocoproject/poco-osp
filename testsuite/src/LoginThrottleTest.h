//
// LoginThrottleTest.h
//
// Definition of the LoginThrottleTest class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef LoginThrottleTest_INCLUDED
#define LoginThrottleTest_INCLUDED


#include "Poco/OSP/OSP.h"
#include "CppUnit/TestCase.h"


class LoginThrottleTest: public CppUnit::TestCase
{
public:
	LoginThrottleTest() = delete;
	explicit LoginThrottleTest(const std::string& name);
	~LoginThrottleTest() override;

	void testDefaultSettings();
	void testChecksAreRefusedAfterMaxAttempts();
	void testRefusalIsNotCounted();
	void testFirstRefusalIsReportedOncePerWindow();
	void testRefusalIsLogged();
	void testRetryAfterIsRoundedUpToWholeSeconds();
	void testWindowStartsAtFirstCountedCheck();
	void testSuccessRefundsCheck();
	void testSuccessRefundsCheckOnce();
	void testSuccessDoesNotClearFailures();
	void testRefundFromEarlierWindowIsIgnored();
	void testRefusalRefundsNothing();
	void testMovedFromAttemptRefundsNothing();
	void testExemptAttemptCountsNothing();
	void testInvalidSettingsAreRejected();
	void testZeroMaxAttemptsTurnsLimitOff();
	void testParallelChecksCannotExceedLimit();
	void testParallelRefundsAreCounted();
	void testFullKeyTableRemovesWindowThatEndsFirst();
	void testRenewedWindowIsRemovedLast();
	void testDroppedWindowIsLoggedOncePerMinute();
	void testDroppedWindowWithoutCountedChecksIsNotLogged();

	void setUp() override;
	void tearDown() override;

	static CppUnit::Test* suite();
};


#endif // LoginThrottleTest_INCLUDED
