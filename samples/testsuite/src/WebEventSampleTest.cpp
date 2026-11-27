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
#include "Poco/OSP/Web/WebServerDispatcher.h"
#include "Poco/OSP/Web/WebServerRequestHandlerFactory.h"
#include "Poco/OSP/Bundle.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/BundleLoader.h"
#include "Poco/OSP/ServiceRef.h"
#include "Poco/Net/HTMLForm.h"
#include "Poco/Net/HTTPClientSession.h"
#include "Poco/Net/HTTPCookie.h"
#include "Poco/Net/HTTPMessage.h"
#include "Poco/Net/HTTPRequest.h"
#include "Poco/Net/HTTPResponse.h"
#include "Poco/Net/HTTPServer.h"
#include "Poco/Net/HTTPServerParams.h"
#include "Poco/Net/NameValueCollection.h"
#include "Poco/Net/NetException.h"
#include "Poco/Net/ServerSocket.h"
#include "Poco/Net/SocketAddress.h"
#include "Poco/Net/WebSocket.h"
#include "Poco/Util/PropertyFileConfiguration.h"
#include "Poco/AutoPtr.h"
#include "Poco/Bugcheck.h"
#include "Poco/Delegate.h"
#include "Poco/Logger.h"
#include "Poco/Message.h"
#include "Poco/ThreadPool.h"
#include "Poco/Types.h"
#include <chrono>
#include <cstddef>
#include <ios>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>


using Poco::OSP::WebEvent::WebEventService;
using Poco::Net::HTTPResponse;
using namespace std::string_literals;


namespace
{
	const std::string SAMPLE_BUNDLE("com.appinf.osp.samples.webevent"s);
	const std::string CLIENT_SUBJECT("sample.client"s);
	constexpr std::size_t MESSAGES_PER_CLIENT_NOTIFICATION = 2;
	constexpr std::chrono::milliseconds NOTIFICATION_TIMEOUT{10000};
	const std::string AUTH_SAMPLE_BUNDLE("com.appinf.osp.samples.auth"s);
	const std::string ADMIN_PASSWORD("test.admin-password"s);
	const std::string SESSION_SETTING("osp.web.event.session"s);

	class LoopbackServer
		/// Serves the requests of the web server dispatcher of an application on a loopback port.
	{
	public:
		LoopbackServer() = delete;

		explicit LoopbackServer(TestApplication& app):
			_pDispatcher(app.serviceRegistry().findByName(Poco::OSP::Web::WebServerDispatcher::SERVICE_NAME)
				->castedInstance<Poco::OSP::Web::WebServerDispatcher>())
		{
			Poco::Net::ServerSocket serverSocket(Poco::Net::SocketAddress("127.0.0.1"s, 0));
			_port = serverSocket.address().port();
			_pServer = std::make_unique<Poco::Net::HTTPServer>(
				new Poco::OSP::Web::WebServerRequestHandlerFactory(*_pDispatcher, false),
				_pool, serverSocket, new Poco::Net::HTTPServerParams);
			_pServer->start();
		}

		~LoopbackServer()
		{
			try
			{
				_pServer->stopAll(true);
				_pool.joinAll();
			}
			catch (...)
			{
				poco_unexpected();
			}
		}

		LoopbackServer(const LoopbackServer&) = delete;
		LoopbackServer(LoopbackServer&&) = delete;
		LoopbackServer& operator = (const LoopbackServer&) = delete;
		LoopbackServer& operator = (LoopbackServer&&) = delete;

		[[nodiscard]] Poco::Net::HTTPClientSession session() const
			/// Returns a client session for the server.
		{
			return Poco::Net::HTTPClientSession("127.0.0.1"s, _port);
		}

	private:
		Poco::OSP::Web::WebServerDispatcher::Ptr _pDispatcher;
		Poco::ThreadPool _pool;
		Poco::UInt16 _port = 0;
		std::unique_ptr<Poco::Net::HTTPServer> _pServer;
	};

	[[nodiscard]] HTTPResponse::HTTPStatus upgradeStatus(
		const LoopbackServer& server,
		const Poco::Net::NameValueCollection& cookies)
		/// Asks for a WebSocket connection to the WebEvent service with the given cookies, and returns
		/// the status of the response: 101 if the server upgrades the connection, which is closed again.
	{
		Poco::Net::HTTPClientSession session = server.session();
		Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, "/webevent"s,
			Poco::Net::HTTPMessage::HTTP_1_1);
		request.setCookies(cookies);
		HTTPResponse response;
		try
		{
			Poco::Net::WebSocket ws(session, request, response);
		}
		catch (Poco::Net::WebSocketException&)
		{
		}
		return response.getStatus();
	}
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


void WebEventSampleTest::testWebSocketNeedsLogin()
{
	TestApplication::Ptr pApp =
		new TestApplication({"poco.net"s, "osp.web"s, "com.appinf.osp.webevent"s, AUTH_SAMPLE_BUNDLE, SAMPLE_BUNDLE});
	Poco::AutoPtr<Poco::Util::PropertyFileConfiguration> pSampleConfig =
		new Poco::Util::PropertyFileConfiguration(TestApplication::sampleConfiguration("BundleServer"s));
	pApp->config().setString(SESSION_SETTING, pSampleConfig->getString(SESSION_SETTING));
	pApp->config().setString("adminPassword"s, ADMIN_PASSWORD);
	pApp->start();
	LoopbackServer server(*pApp);

	assertTrue (upgradeStatus(server, Poco::Net::NameValueCollection()) == HTTPResponse::HTTP_UNAUTHORIZED);

	for (const auto& [password, status]: {std::make_pair("wrong"s, HTTPResponse::HTTP_UNAUTHORIZED),
		std::make_pair(ADMIN_PASSWORD, HTTPResponse::HTTP_NO_CONTENT)})
	{
		Poco::Net::HTMLForm form;
		form.set("username"s, "admin"s);
		form.set("password"s, password);
		Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_POST, "/webeventsample/login"s,
			Poco::Net::HTTPMessage::HTTP_1_1);
		form.prepareSubmit(request);
		Poco::Net::HTTPClientSession session = server.session();
		form.write(session.sendRequest(request));
		HTTPResponse response;
		session.receiveResponse(response).ignore(std::numeric_limits<std::streamsize>::max());
		assertTrue (response.getStatus() == status);
		if (status != HTTPResponse::HTTP_NO_CONTENT) continue;

		std::vector<Poco::Net::HTTPCookie> cookies;
		response.getCookies(cookies);
		Poco::Net::NameValueCollection sessionCookies;
		for (const auto& cookie: cookies)
		{
			sessionCookies.set(cookie.getName(), cookie.getValue());
		}
		assertFalse (sessionCookies.empty());
		assertTrue (upgradeStatus(server, sessionCookies) == HTTPResponse::HTTP_SWITCHING_PROTOCOLS);
	}
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
	CppUnit_addTest(pSuite, WebEventSampleTest, testWebSocketNeedsLogin);

	return pSuite;
}
