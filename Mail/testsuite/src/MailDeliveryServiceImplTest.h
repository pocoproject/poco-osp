//
// MailDeliveryServiceImplTest.h
//
// Definition of the MailDeliveryServiceImplTest class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef MailDeliveryServiceImplTest_INCLUDED
#define MailDeliveryServiceImplTest_INCLUDED


#include "CppUnit/TestCase.h"
#include <string>


class MailDeliveryServiceImplTest: public CppUnit::TestCase
{
public:
	MailDeliveryServiceImplTest() = delete;
	explicit MailDeliveryServiceImplTest(const std::string& name);
	~MailDeliveryServiceImplTest() override;

	void testLoadConfigurationPrefersSettingsOverDefaults();
	void testLoadConfigurationIgnoresCaseOfLoginMethod();
	void testLoadConfigurationNamesInvalidLoginMethod();
	void testLoadConfigurationRejectsPortOutsideRange();
	void testLoadConfigurationAcceptsPortsOneTo65535();
	void testSessionTimeoutOfOneSecondIsAccepted();
	void testSessionTimeoutBelowOneSecondIsRejected();
	void testSessionTimeoutAbove2147483SecondsIsRejected();
	void testSessionTimeoutAbove2147483647MillisecondsIsRejected();
	void testSocketTimeoutBelowOneSecondIsRejected();
	void testPortZeroIsRejected();
	void testSessionClosedByServerIsReplaced();
	void testSessionIsKeptWhileServerAcceptsReset();
	void testStopFailsQueuedMessages();
	void testSendMessageAfterStopThrows();
	void testStatusWhileDelivering();
	void testStandardExceptionFailsMessage();
	void testStandardExceptionLogNamesMessage();
	void testEveryMessageGetsAllLoginAttempts();
	void testFailedLoginLogNamesMessage();
	void testRepeatedLoginFailureIsLoggedOnce();
	void testMessageIsNotChanged();

	void setUp() override;
	void tearDown() override;

	static CppUnit::Test* suite();
};


#endif // MailDeliveryServiceImplTest_INCLUDED
