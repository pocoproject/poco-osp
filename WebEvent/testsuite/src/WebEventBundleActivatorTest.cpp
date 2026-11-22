//
// WebEventBundleActivatorTest.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "WebEventBundleActivatorTest.h"
#include "LogCapture.h"
#include "TemporaryBundle.h"
#include "Poco/OSP/WebEvent/WebEventService.h"
#include "Poco/OSP/WebEvent/WebEventServiceImpl.h"
#include "Poco/OSP/Web/WebRequestHandlerFactory.h"
#include "Poco/OSP/BundleActivator.h"
#include "Poco/OSP/PreferencesService.h"
#include "Poco/OSP/Properties.h"
#include "Poco/OSP/ServiceRef.h"
#include "Poco/Net/HTTPClientSession.h"
#include "Poco/Net/HTTPMessage.h"
#include "Poco/Net/HTTPRequest.h"
#include "Poco/Net/HTTPRequestHandlerFactory.h"
#include "Poco/Net/HTTPResponse.h"
#include "Poco/Net/HTTPServer.h"
#include "Poco/Net/HTTPServerParams.h"
#include "Poco/Net/NetException.h"
#include "Poco/Net/ServerSocket.h"
#include "Poco/Net/SocketAddress.h"
#include "Poco/Net/WebSocket.h"
#include "Poco/Util/MapConfiguration.h"
#include "Poco/AutoPtr.h"
#include "Poco/ClassLoader.h"
#include "Poco/Exception.h"
#include "Poco/Logger.h"
#include "Poco/Message.h"
#include "Poco/Path.h"
#include "Poco/TemporaryFile.h"
#include "Poco/ThreadPool.h"
#include "Poco/Types.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "CppUnit/TestCase.h"
#include "CppUnit/Test.h"
#include <chrono>
#include <memory>
#include <string>
#include <vector>


using Poco::OSP::WebEvent::WebEventService;
using Poco::OSP::WebEvent::WebEventServiceImpl;
using Poco::OSP::Web::WebRequestHandlerFactory;
using Poco::OSP::PreferencesService;
using namespace std::string_literals;


namespace
{
	void registerPreferences(TemporaryBundle& bundles, const Poco::TemporaryFile& persistency,
		Poco::Util::MapConfiguration* pSettings)
		/// Registers a PreferencesService with the given settings.
	{
		bundles.registry().registerService(PreferencesService::SERVICE_NAME,
			new PreferencesService(Poco::Path(persistency.path()), pSettings), Poco::OSP::Properties());
	}


	int upgradeStatus(Poco::UInt16 port, const std::string& origin)
		/// Returns the status of a WebSocket upgrade request with the given
		/// Origin header.
	{
		Poco::Net::HTTPClientSession session("127.0.0.1"s, port);
		Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, "/webevent"s,
			Poco::Net::HTTPMessage::HTTP_1_1);
		request.set("Origin"s, origin);
		Poco::Net::HTTPResponse response;
		try
		{
			Poco::Net::WebSocket ws(session, request, response);
			ws.close();
		}
		catch (Poco::Net::WebSocketException&)
		{
		}
		return static_cast<int>(response.getStatus());
	}


	const std::string CHECK_LOGGER("WebEventBundleActivatorTest");
		/// The logger the checks of the bundle properties log to.
}


WebEventBundleActivatorTest::WebEventBundleActivatorTest(const std::string& name): CppUnit::TestCase(name)
{
}


WebEventBundleActivatorTest::~WebEventBundleActivatorTest() = default;


void WebEventBundleActivatorTest::testConfigIsReadFromProperties()
{
	Poco::AutoPtr<Poco::Util::MapConfiguration> pSettings = new Poco::Util::MapConfiguration;
	pSettings->setInt("osp.web.event.workers"s, 3);
	pSettings->setInt("osp.web.event.maxWebSockets"s, 7);
	pSettings->setInt("osp.web.event.maxPendingFrames"s, 9);
	pSettings->setInt("osp.web.event.pingInterval"s, 11);
	pSettings->setBool("osp.web.event.clientNotify"s, true);
	pSettings->setInt("osp.web.event.maxSubjects"s, 13);
	pSettings->setBool("osp.web.event.requireSubjectAuthorizer"s, true);

	const WebEventServiceImpl::Config config = WebEventServiceImpl::Config::fromConfiguration(*pSettings);

	assertEqual (3, config.workerCount);
	assertEqual (7, config.maxSockets);
	assertEqual (9, config.maxPendingFrames);
	assertTrue (config.pingInterval == std::chrono::seconds(11));
	assertTrue (config.clientNotify);
	assertEqual (13, config.maxSubjects);
	assertTrue (config.requireSubjectAuthorizer);
}


void WebEventBundleActivatorTest::testConfigKeepsDefaultsWithoutProperties()
{
	Poco::AutoPtr<Poco::Util::MapConfiguration> pSettings = new Poco::Util::MapConfiguration;
	const WebEventServiceImpl::Config defaults;

	const WebEventServiceImpl::Config config = WebEventServiceImpl::Config::fromConfiguration(*pSettings);

	assertEqual (defaults.workerCount, config.workerCount);
	assertEqual (defaults.maxSockets, config.maxSockets);
	assertEqual (defaults.maxPendingFrames, config.maxPendingFrames);
	assertTrue (config.pingInterval == defaults.pingInterval);
	assertTrue (config.clientNotify == defaults.clientNotify);
	assertEqual (defaults.maxSubjects, config.maxSubjects);
	assertFalse (config.requireSubjectAuthorizer);
}


void WebEventBundleActivatorTest::testCurrentBundlePropertiesAreAccepted()
	/// The osp.web.event settings in the bundle properties are not reported.
{
	LogCapture capture(CHECK_LOGGER);
	Poco::AutoPtr<Poco::Util::MapConfiguration> pBundleProperties = new Poco::Util::MapConfiguration;
	pBundleProperties->setString("osp.web.event.session"s, "test.session"s);
	Poco::AutoPtr<Poco::Util::MapConfiguration> pSettings = new Poco::Util::MapConfiguration;

	WebEventServiceImpl::checkBundleProperties(*pBundleProperties, *pSettings, Poco::Logger::get(CHECK_LOGGER));

	assertTrue (capture.channel().waitForMessages(0).empty());
}


void WebEventBundleActivatorTest::testOldSessionPropertyIsReported()
	/// The bundle property websession.id is not read: an error names the
	/// setting that replaces it.
{
	LogCapture capture(CHECK_LOGGER);
	Poco::AutoPtr<Poco::Util::MapConfiguration> pBundleProperties = new Poco::Util::MapConfiguration;
	pBundleProperties->setString("websession.id"s, "test.session"s);
	Poco::AutoPtr<Poco::Util::MapConfiguration> pSettings = new Poco::Util::MapConfiguration;

	WebEventServiceImpl::checkBundleProperties(*pBundleProperties, *pSettings, Poco::Logger::get(CHECK_LOGGER));

	const std::vector<Poco::Message> messages = capture.channel().waitForMessages(1);
	assertEqual (1, messages.size());
	assertTrue (messages[0].getPriority() == Poco::Message::PRIO_ERROR);
	assertTrue (messages[0].getText().find("osp.web.event.session"s) != std::string::npos);
}


void WebEventBundleActivatorTest::testOldSessionPropertyIsReportedWhenReplaced()
	/// The bundle property websession.id is not read: with osp.web.event.session
	/// set, the error asks to remove the bundle property and does not claim that
	/// browsers are not authenticated by the cookie.
{
	LogCapture capture(CHECK_LOGGER);
	Poco::AutoPtr<Poco::Util::MapConfiguration> pBundleProperties = new Poco::Util::MapConfiguration;
	pBundleProperties->setString("websession.id"s, "test.session"s);
	Poco::AutoPtr<Poco::Util::MapConfiguration> pSettings = new Poco::Util::MapConfiguration;
	pSettings->setString("osp.web.event.session"s, "test.session"s);

	WebEventServiceImpl::checkBundleProperties(*pBundleProperties, *pSettings, Poco::Logger::get(CHECK_LOGGER));

	const std::vector<Poco::Message> messages = capture.channel().waitForMessages(1);
	assertEqual (1, messages.size());
	assertTrue (messages[0].getPriority() == Poco::Message::PRIO_ERROR);
	assertTrue (messages[0].getText().find("Remove the bundle property"s) != std::string::npos);
	assertTrue (messages[0].getText().find("not authenticated"s) == std::string::npos);
}


void WebEventBundleActivatorTest::testOldPermissionPropertyIsReportedWhenReplaced()
	/// The bundle property permission is not read: with osp.web.event.permission
	/// set, an error names that setting, and the bundle starts.
{
	LogCapture capture(CHECK_LOGGER);
	Poco::AutoPtr<Poco::Util::MapConfiguration> pBundleProperties = new Poco::Util::MapConfiguration;
	pBundleProperties->setString("permission"s, "test.events.read"s);
	Poco::AutoPtr<Poco::Util::MapConfiguration> pSettings = new Poco::Util::MapConfiguration;
	pSettings->setString("osp.web.event.permission"s, "test.events.read"s);

	WebEventServiceImpl::checkBundleProperties(*pBundleProperties, *pSettings, Poco::Logger::get(CHECK_LOGGER));

	const std::vector<Poco::Message> messages = capture.channel().waitForMessages(1);
	assertEqual (1, messages.size());
	assertTrue (messages[0].getPriority() == Poco::Message::PRIO_ERROR);
	assertTrue (messages[0].getText().find("osp.web.event.permission"s) != std::string::npos);
}


void WebEventBundleActivatorTest::testOldSessionPropertyIsReportedWithEmptySession()
	/// With osp.web.event.session set to an empty value, the error says that
	/// browsers are not admitted by a session cookie, instead of naming a web
	/// session.
{
	LogCapture capture(CHECK_LOGGER);
	Poco::AutoPtr<Poco::Util::MapConfiguration> pBundleProperties = new Poco::Util::MapConfiguration;
	pBundleProperties->setString("websession.id"s, "test.session"s);
	Poco::AutoPtr<Poco::Util::MapConfiguration> pSettings = new Poco::Util::MapConfiguration;
	pSettings->setString("osp.web.event.session"s, ""s);

	WebEventServiceImpl::checkBundleProperties(*pBundleProperties, *pSettings, Poco::Logger::get(CHECK_LOGGER));

	const std::vector<Poco::Message> messages = capture.channel().waitForMessages(1);
	assertEqual (1, messages.size());
	assertTrue (messages[0].getPriority() == Poco::Message::PRIO_ERROR);
	assertTrue (messages[0].getText().find("osp.web.event.session is empty"s) != std::string::npos);
}


void WebEventBundleActivatorTest::testOldPermissionPropertyIsReportedWithEmptyPermission()
	/// With osp.web.event.permission set to an empty value, the error says
	/// that the web server does not authenticate the upgrade requests, instead
	/// of naming a permission.
{
	LogCapture capture(CHECK_LOGGER);
	Poco::AutoPtr<Poco::Util::MapConfiguration> pBundleProperties = new Poco::Util::MapConfiguration;
	pBundleProperties->setString("permission"s, "test.events.read"s);
	Poco::AutoPtr<Poco::Util::MapConfiguration> pSettings = new Poco::Util::MapConfiguration;
	pSettings->setString("osp.web.event.permission"s, ""s);

	WebEventServiceImpl::checkBundleProperties(*pBundleProperties, *pSettings, Poco::Logger::get(CHECK_LOGGER));

	const std::vector<Poco::Message> messages = capture.channel().waitForMessages(1);
	assertEqual (1, messages.size());
	assertTrue (messages[0].getPriority() == Poco::Message::PRIO_ERROR);
	assertTrue (messages[0].getText().find("osp.web.event.permission is empty"s) != std::string::npos);
	assertTrue (messages[0].getText().find("does not authenticate"s) != std::string::npos);
}


void WebEventBundleActivatorTest::testOldPermissionPropertyAloneStopsStart()
	/// The bundle property permission without osp.web.event.permission would
	/// leave the WebEvent endpoint without the configured restriction.
{
	Poco::AutoPtr<Poco::Util::MapConfiguration> pBundleProperties = new Poco::Util::MapConfiguration;
	pBundleProperties->setString("permission"s, "test.events.read"s);
	Poco::AutoPtr<Poco::Util::MapConfiguration> pSettings = new Poco::Util::MapConfiguration;

	try
	{
		WebEventServiceImpl::checkBundleProperties(*pBundleProperties, *pSettings, Poco::Logger::get(CHECK_LOGGER));
		fail("the bundle must not start with the bundle property permission alone");
	}
	catch (Poco::InvalidArgumentException&)
	{
	}
}


void WebEventBundleActivatorTest::testActivatorStartFailsWithOldPermissionPropertyAlone()
	/// The start checks the bundle properties: with the bundle property
	/// permission alone it fails, and no service is registered; with
	/// osp.web.event.permission set as well, the same bundle starts.
{
	TemporaryBundle bundles("permission = test.events.read\n"s);
	Poco::TemporaryFile persistency;
	Poco::AutoPtr<Poco::Util::MapConfiguration> pSettings = new Poco::Util::MapConfiguration;
	registerPreferences(bundles, persistency, pSettings);

	Poco::ClassLoader<Poco::OSP::BundleActivator> activatorLoader;
	activatorLoader.loadLibrary(WEBEVENT_BUNDLE_LIBRARY);
	std::unique_ptr<Poco::OSP::BundleActivator> pActivator(
		activatorLoader.create("Poco::OSP::WebEvent::WebEventBundleActivator"s));
	try
	{
		pActivator->start(bundles.context());
		fail("the bundle must not start with the bundle property permission alone");
	}
	catch (Poco::InvalidArgumentException&)
	{
	}
	assertTrue (bundles.registry().findByName(WebEventService::SERVICE_NAME) == nullptr);

	pSettings->setString("osp.web.event.permission"s, "test.events.read"s);
	pActivator->start(bundles.context());
	assertTrue (bundles.registry().findByName(WebEventService::SERVICE_NAME) != nullptr);
	pActivator->stop(bundles.context());
}


void WebEventBundleActivatorTest::testActivatorRegistersAndStopsService()
	/// The request handler factory accepts an origin listed in osp.web.event.allowedOrigins and rejects another.
{
	TemporaryBundle bundles(""s);
	Poco::TemporaryFile persistency;
	Poco::AutoPtr<Poco::Util::MapConfiguration> pSettings = new Poco::Util::MapConfiguration;
	pSettings->setString("osp.web.event.allowedOrigins"s, "https://app.example.com"s);
	pSettings->setInt("osp.web.event.workers"s, 1);
	registerPreferences(bundles, persistency, pSettings);

	Poco::ClassLoader<Poco::OSP::BundleActivator> activatorLoader;
	activatorLoader.loadLibrary(WEBEVENT_BUNDLE_LIBRARY);
	std::unique_ptr<Poco::OSP::BundleActivator> pActivator(
		activatorLoader.create("Poco::OSP::WebEvent::WebEventBundleActivator"s));
	pActivator->start(bundles.context());
	Poco::OSP::ServiceRef::Ptr pRef = bundles.registry().findByName(WebEventService::SERVICE_NAME);
	assertTrue (pRef != nullptr);
	WebEventServiceImpl::Ptr pService = pRef->castedInstance<WebEventServiceImpl>();

	Poco::ClassLoader<WebRequestHandlerFactory> factoryLoader;
	factoryLoader.loadLibrary(WEBEVENT_BUNDLE_LIBRARY, "WebServer"s);
	{
		WebRequestHandlerFactory* pFactory =
			factoryLoader.create("Poco::OSP::WebEvent::WebEventRequestHandlerFactory"s);
		Poco::Net::HTTPRequestHandlerFactory::Ptr pFactoryPtr(pFactory);
		pFactory->init(bundles.context());
		Poco::ThreadPool pool;
		Poco::Net::ServerSocket serverSocket(Poco::Net::SocketAddress("127.0.0.1"s, 0));
		Poco::Net::HTTPServer server(pFactoryPtr, pool, serverSocket, new Poco::Net::HTTPServerParams);
		server.start();

		const int allowed = upgradeStatus(serverSocket.address().port(), "https://app.example.com"s);
		const int other = upgradeStatus(serverSocket.address().port(), "https://other.example.com"s);

		server.stopAll(true);
		server.stop();
		pool.joinAll();
		assertEqual (static_cast<int>(Poco::Net::HTTPResponse::HTTP_SWITCHING_PROTOCOLS), allowed);
		assertEqual (static_cast<int>(Poco::Net::HTTPResponse::HTTP_FORBIDDEN), other);
	}

	pActivator->stop(bundles.context());

	assertTrue (bundles.registry().findByName(WebEventService::SERVICE_NAME) == nullptr);
	assertTrue (pService->stopped());
}


void WebEventBundleActivatorTest::testFactoryRequiresService()
{
	TemporaryBundle bundles(""s);
	Poco::TemporaryFile persistency;
	Poco::AutoPtr<Poco::Util::MapConfiguration> pSettings = new Poco::Util::MapConfiguration;
	registerPreferences(bundles, persistency, pSettings);

	Poco::ClassLoader<WebRequestHandlerFactory> factoryLoader;
	factoryLoader.loadLibrary(WEBEVENT_BUNDLE_LIBRARY, "WebServer"s);
	std::unique_ptr<WebRequestHandlerFactory> pFactory(
		factoryLoader.create("Poco::OSP::WebEvent::WebEventRequestHandlerFactory"s));

	try
	{
		pFactory->init(bundles.context());
		fail("the factory must not be initialized without the WebEventService");
	}
	catch (Poco::NotFoundException&)
	{
	}
}


void WebEventBundleActivatorTest::setUp()
{
}


void WebEventBundleActivatorTest::tearDown()
{
}


CppUnit::Test* WebEventBundleActivatorTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("WebEventBundleActivatorTest");

	CppUnit_addTest(pSuite, WebEventBundleActivatorTest, testConfigIsReadFromProperties);
	CppUnit_addTest(pSuite, WebEventBundleActivatorTest, testConfigKeepsDefaultsWithoutProperties);
	CppUnit_addTest(pSuite, WebEventBundleActivatorTest, testCurrentBundlePropertiesAreAccepted);
	CppUnit_addTest(pSuite, WebEventBundleActivatorTest, testOldSessionPropertyIsReported);
	CppUnit_addTest(pSuite, WebEventBundleActivatorTest, testOldSessionPropertyIsReportedWhenReplaced);
	CppUnit_addTest(pSuite, WebEventBundleActivatorTest, testOldPermissionPropertyIsReportedWhenReplaced);
	CppUnit_addTest(pSuite, WebEventBundleActivatorTest, testOldSessionPropertyIsReportedWithEmptySession);
	CppUnit_addTest(pSuite, WebEventBundleActivatorTest, testOldPermissionPropertyIsReportedWithEmptyPermission);
	CppUnit_addTest(pSuite, WebEventBundleActivatorTest, testOldPermissionPropertyAloneStopsStart);
	CppUnit_addTest(pSuite, WebEventBundleActivatorTest, testActivatorStartFailsWithOldPermissionPropertyAlone);
	CppUnit_addTest(pSuite, WebEventBundleActivatorTest, testActivatorRegistersAndStopsService);
	CppUnit_addTest(pSuite, WebEventBundleActivatorTest, testFactoryRequiresService);

	return pSuite;
}
