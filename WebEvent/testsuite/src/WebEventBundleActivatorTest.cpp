//
// WebEventBundleActivatorTest.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "WebEventBundleActivatorTest.h"
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

	const WebEventServiceImpl::Config config = WebEventServiceImpl::Config::fromConfiguration(*pSettings);

	assertEqual (3, config.workerCount);
	assertEqual (7, config.maxSockets);
	assertEqual (9, config.maxPendingFrames);
	assertTrue (config.pingInterval == std::chrono::seconds(11));
	assertTrue (config.clientNotify);
	assertEqual (13, config.maxSubjects);
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
	CppUnit_addTest(pSuite, WebEventBundleActivatorTest, testActivatorRegistersAndStopsService);
	CppUnit_addTest(pSuite, WebEventBundleActivatorTest, testFactoryRequiresService);

	return pSuite;
}
