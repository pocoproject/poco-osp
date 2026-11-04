//
// WebEventSampleTest.h
//
// Definition of the WebEventSampleTest class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef WebEventSampleTest_INCLUDED
#define WebEventSampleTest_INCLUDED


#include "Poco/OSP/WebEvent/WebEventService.h"
#include "CppUnit/TestCase.h"
#include "Poco/Event.h"


class WebEventSampleTest: public CppUnit::TestCase
{
public:
	WebEventSampleTest() = delete;
	explicit WebEventSampleTest(const std::string& name);
	~WebEventSampleTest() override;

	void testSubjectDelegatesWorkAfterSampleStops();
	void testSubjectDelegatesWorkAfterSampleIsUninstalled();
	void testNotificationIsLoggedOnOneLine();
	void testNotificationIsNotLoggedAtInformationLevel();

	static CppUnit::Test* suite();

private:
	void onSubjectNotified(const Poco::OSP::WebEvent::WebEventService::NotificationEvent& ev);
	void checkNotificationAfter(bool uninstall);

	Poco::Event _notified;
};


#endif // WebEventSampleTest_INCLUDED
