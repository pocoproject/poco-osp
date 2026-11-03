//
// WebEventSampleTest.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "WebEventSampleTest.h"
#include "TestApplication.h"
#include "LogCapture.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "CppUnit/TestCase.h"
#include "CppUnit/Test.h"
#include "Poco/OSP/WebEvent/WebEventService.h"
#include "Poco/OSP/Bundle.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/BundleLoader.h"
#include "Poco/OSP/ServiceRef.h"
#include "Poco/Delegate.h"
#include "Poco/Logger.h"
#include "Poco/Message.h"
#include <chrono>
#include <cstddef>
#include <string>
#include <vector>


using Poco::OSP::WebEvent::WebEventService;
using namespace std::string_literals;


namespace
{
	const std::string SAMPLE_BUNDLE("com.appinf.osp.samples.webevent"s);
	const std::string CLIENT_SUBJECT("sample.client"s);
	constexpr std::size_t MESSAGES_PER_CLIENT_NOTIFICATION = 2;
	constexpr std::chrono::milliseconds NOTIFICATION_TIMEOUT{10000};
}


WebEventSampleTest::WebEventSampleTest(const std::string& name): CppUnit::TestCase(name)
{
}


WebEventSampleTest::~WebEventSampleTest() = default;


void WebEventSampleTest::testSubjectDelegatesWorkAfterSampleStops()
{
	checkNotificationAfter(false);
}


void WebEventSampleTest::testSubjectDelegatesWorkAfterSampleIsUninstalled()
{
	checkNotificationAfter(true);
}


void WebEventSampleTest::testNotificationIsLoggedOnOneLine()
{
	TestApplication::Ptr pApp = new TestApplication({"poco.net"s, "osp.web"s, "com.appinf.osp.webevent"s, SAMPLE_BUNDLE});
	pApp->start();
	WebEventService::Ptr pService = pApp->serviceRegistry().findByName(WebEventService::SERVICE_NAME)->castedInstance<WebEventService>();
	Poco::OSP::Bundle::Ptr pSample = pApp->bundleLoader().findBundle(SAMPLE_BUNDLE);
	const std::string loggerName = pApp->bundleLoader().contextForBundle(pSample)->logger().name();
	LogCapture capture(loggerName);
	Poco::Logger::get(loggerName).setLevel(Poco::Message::PRIO_DEBUG);

	pService->notify(CLIENT_SUBJECT, "first line\nsecond line"s);

	const std::vector<Poco::Message> messages = capture.channel().waitForMessages(MESSAGES_PER_CLIENT_NOTIFICATION);
	assertTrue (messages.size() >= MESSAGES_PER_CLIENT_NOTIFICATION);
	for (const auto& message: messages)
	{
		assertTrue (message.getText().find('\n') == std::string::npos);
	}
}


void WebEventSampleTest::testNotificationIsNotLoggedAtInformationLevel()
{
	TestApplication::Ptr pApp = new TestApplication({"poco.net"s, "osp.web"s, "com.appinf.osp.webevent"s, SAMPLE_BUNDLE});
	pApp->start();
	WebEventService::Ptr pService = pApp->serviceRegistry().findByName(WebEventService::SERVICE_NAME)->castedInstance<WebEventService>();
	// Registered after the sample's delegates, which have logged when it is called.
	pService->subjectNotified(CLIENT_SUBJECT) += Poco::delegate(this, &WebEventSampleTest::onSubjectNotified);
	Poco::OSP::Bundle::Ptr pSample = pApp->bundleLoader().findBundle(SAMPLE_BUNDLE);
	LogCapture capture(pApp->bundleLoader().contextForBundle(pSample)->logger().name());

	pService->notify(CLIENT_SUBJECT, "data"s);
	const bool notified = _notified.tryWait(static_cast<long>(NOTIFICATION_TIMEOUT.count()));

	pService->subjectNotified(CLIENT_SUBJECT) -= Poco::delegate(this, &WebEventSampleTest::onSubjectNotified);
	assertTrue (notified);
	assertTrue (capture.channel().waitForMessages(0).empty());
}


void WebEventSampleTest::onSubjectNotified(const WebEventService::NotificationEvent& /*ev*/)
{
	_notified.set();
}


void WebEventSampleTest::checkNotificationAfter(bool uninstall)
{
	TestApplication::Ptr pApp = new TestApplication({"poco.net"s, "osp.web"s, "com.appinf.osp.webevent"s, SAMPLE_BUNDLE});
	if (uninstall) pApp->config().setString("osp.codeCache"s, TestApplication::privateCodeCachePath());
	pApp->start();
	WebEventService::Ptr pService = pApp->serviceRegistry().findByName(WebEventService::SERVICE_NAME)->castedInstance<WebEventService>();
	// Registered after the sample's delegates: a delegate that throws
	// keeps the delegates registered after it from being called.
	pService->subjectNotified(CLIENT_SUBJECT) += Poco::delegate(this, &WebEventSampleTest::onSubjectNotified);

	Poco::OSP::Bundle::Ptr pSample = pApp->bundleLoader().findBundle(SAMPLE_BUNDLE);
	const std::string loggerName = pApp->bundleLoader().contextForBundle(pSample)->logger().name();
	pSample->stop();
	if (uninstall) pSample->uninstall();
	LogCapture capture(loggerName);
	Poco::Logger::get(loggerName).setLevel(Poco::Message::PRIO_DEBUG);
	pService->notify(CLIENT_SUBJECT, "data"s);
	const bool notified = _notified.tryWait(static_cast<long>(NOTIFICATION_TIMEOUT.count()));

	pService->subjectNotified(CLIENT_SUBJECT) -= Poco::delegate(this, &WebEventSampleTest::onSubjectNotified);
	assertTrue (notified);
	// A delegate of the sample that is still registered has logged before the delegate of the test was called.
	assertTrue (capture.channel().waitForMessages(0).empty());
}


CppUnit::Test* WebEventSampleTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("WebEventSampleTest");

	CppUnit_addTest(pSuite, WebEventSampleTest, testSubjectDelegatesWorkAfterSampleStops);
	CppUnit_addTest(pSuite, WebEventSampleTest, testSubjectDelegatesWorkAfterSampleIsUninstalled);
	CppUnit_addTest(pSuite, WebEventSampleTest, testNotificationIsLoggedOnOneLine);
	CppUnit_addTest(pSuite, WebEventSampleTest, testNotificationIsNotLoggedAtInformationLevel);

	return pSuite;
}
