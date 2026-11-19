//
// AbstractWebServerBundleActivatorTest.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "AbstractWebServerBundleActivatorTest.h"
#include "TemporaryBundle.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "CppUnit/TestCase.h"
#include "CppUnit/Test.h"
#include "Poco/OSP/Web/AbstractWebServerBundleActivator.h"
#include "Poco/OSP/Web/WebServerDispatcher.h"
#include "Poco/OSP/Web/WebServerService.h"
#include "Poco/OSP/Properties.h"
#include "Poco/OSP/PreferencesService.h"
#include "Poco/OSP/ServiceRegistry.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/ServiceRef.h"
#include "Poco/Net/HTTPClientSession.h"
#include "Poco/Net/HTTPRequest.h"
#include "Poco/Net/HTTPResponse.h"
#include "Poco/Net/ServerSocket.h"
#include "Poco/Net/SocketAddress.h"
#include "Poco/Net/HTTPServerParams.h"
#include "Poco/Util/MapConfiguration.h"
#include "Poco/AutoPtr.h"
#include "Poco/Exception.h"
#include <memory>


using Poco::OSP::Web::AbstractWebServerBundleActivator;
using Poco::OSP::Web::WebServerDispatcher;
using Poco::OSP::Web::WebServerService;
using Poco::OSP::BundleContext;
using Poco::OSP::ServiceRef;


namespace
{
	class TestActivator: public AbstractWebServerBundleActivator
		/// Listens on an ephemeral port of the loopback interface,
		/// whatever port the settings name.
	{
	public:
		explicit TestActivator(bool secure):
			AbstractWebServerBundleActivator(secure)
		{
		}

	protected:
		[[nodiscard]] Poco::Net::ServerSocket createServerSocket(const Poco::Net::SocketAddress& /*address*/) override
		{
			return Poco::Net::ServerSocket(Poco::Net::SocketAddress("127.0.0.1", 0));
		}
	};

	const std::string SERVER_PROPERTIES("port = 1\nsecurePort = 1\nmaxThreads = 2\n");
	constexpr int MAX_THREADS = 2;

	[[nodiscard]] Poco::AutoPtr<WebServerDispatcher> registerDispatcher(TemporaryBundle& bundle)
	{
		WebServerDispatcher::Config config;
		config.pContext = bundle.context();
		Poco::AutoPtr<WebServerDispatcher> pDispatcher(new WebServerDispatcher(config));
		bundle.registry().registerService(WebServerDispatcher::SERVICE_NAME, pDispatcher, Poco::OSP::Properties());
		return pDispatcher;
	}

	[[nodiscard]] Poco::Net::HTTPServerParams::Ptr serverParams(TemporaryBundle& bundle)
		/// Returns the parameters of the HTTP server that the WebServerService
		/// "osp.web.server" of the bundle's registry runs.
	{
		ServiceRef::Ptr pRef = bundle.registry().findByName("osp.web.server");
		if (pRef == nullptr) throw Poco::NotFoundException("osp.web.server");
		return pRef->castedInstance<WebServerService>()->params();
	}
}


AbstractWebServerBundleActivatorTest::AbstractWebServerBundleActivatorTest(const std::string& name): CppUnit::TestCase(name)
{
}


AbstractWebServerBundleActivatorTest::~AbstractWebServerBundleActivatorTest() = default;


void AbstractWebServerBundleActivatorTest::setUp()
{
}


void AbstractWebServerBundleActivatorTest::tearDown()
{
}


void AbstractWebServerBundleActivatorTest::testStartServesRequests()
{
	auto pBundle = std::make_unique<TemporaryBundle>(SERVER_PROPERTIES);
	Poco::AutoPtr<WebServerDispatcher> pDispatcher = registerDispatcher(*pBundle);
	TestActivator activator(false);

	activator.start(pBundle->context());

	ServiceRef::Ptr pRef = pBundle->registry().findByName("osp.web.server");
	assertTrue (pRef != nullptr);
	assertEqual (std::string("http"), pRef->properties().get("protocol"));
	Poco::AutoPtr<WebServerService> pService = pRef->castedInstance<WebServerService>();
	assertFalse (pService->secure());
	Poco::Net::HTTPClientSession session("127.0.0.1", pService->port());
	Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, "/missing");
	session.sendRequest(request);
	Poco::Net::HTTPResponse response;
	session.receiveResponse(response);
	assertTrue (response.getStatus() == Poco::Net::HTTPResponse::HTTP_NOT_FOUND);

	activator.stop(pBundle->context());
}


void AbstractWebServerBundleActivatorTest::testStopReleasesServerAndThreads()
{
	auto pBundle = std::make_unique<TemporaryBundle>(SERVER_PROPERTIES);
	Poco::AutoPtr<WebServerDispatcher> pDispatcher = registerDispatcher(*pBundle);
	const int capacity = pDispatcher->threadPool().capacity();
	TestActivator activator(false);

	activator.start(pBundle->context());
	assertEqual (capacity + MAX_THREADS, pDispatcher->threadPool().capacity());
	activator.stop(pBundle->context());

	assertEqual (capacity, pDispatcher->threadPool().capacity());
	assertTrue (pBundle->registry().findByName("osp.web.server") == nullptr);
}


void AbstractWebServerBundleActivatorTest::testRestart()
{
	auto pBundle = std::make_unique<TemporaryBundle>(SERVER_PROPERTIES);
	Poco::AutoPtr<WebServerDispatcher> pDispatcher = registerDispatcher(*pBundle);
	const int capacity = pDispatcher->threadPool().capacity();
	TestActivator activator(false);

	activator.start(pBundle->context());
	activator.stop(pBundle->context());
	activator.start(pBundle->context());

	assertEqual (capacity + MAX_THREADS, pDispatcher->threadPool().capacity());
	assertTrue (pBundle->registry().findByName("osp.web.server") != nullptr);
	activator.stop(pBundle->context());
}


void AbstractWebServerBundleActivatorTest::testSecureServerService()
{
	auto pBundle = std::make_unique<TemporaryBundle>(SERVER_PROPERTIES);
	Poco::AutoPtr<WebServerDispatcher> pDispatcher = registerDispatcher(*pBundle);
	TestActivator activator(true);

	activator.start(pBundle->context());

	ServiceRef::Ptr pRef = pBundle->registry().findByName("osp.web.server.secure");
	assertTrue (pRef != nullptr);
	assertEqual (std::string("https"), pRef->properties().get("protocol"));
	assertTrue (pRef->castedInstance<WebServerService>()->secure());
	assertTrue (pBundle->registry().findByName("osp.web.server") == nullptr);
	activator.stop(pBundle->context());
}


void AbstractWebServerBundleActivatorTest::testPortZeroDisablesServer()
{
	auto pBundle = std::make_unique<TemporaryBundle>("port = 0\n");
	Poco::AutoPtr<WebServerDispatcher> pDispatcher = registerDispatcher(*pBundle);
	const int capacity = pDispatcher->threadPool().capacity();
	TestActivator activator(false);

	activator.start(pBundle->context());

	assertTrue (pBundle->registry().findByName("osp.web.server") == nullptr);
	assertEqual (capacity, pDispatcher->threadPool().capacity());
	activator.stop(pBundle->context());
}


void AbstractWebServerBundleActivatorTest::testPortOutOfRangeDisablesServer()
{
	for (const char* properties: {"port = 65536\n", "port = -1\n"})
	{
		auto pBundle = std::make_unique<TemporaryBundle>(properties);
		Poco::AutoPtr<WebServerDispatcher> pDispatcher = registerDispatcher(*pBundle);
		const int capacity = pDispatcher->threadPool().capacity();
		TestActivator activator(false);

		activator.start(pBundle->context());

		assertTrue (pBundle->registry().findByName("osp.web.server") == nullptr);
		assertEqual (capacity, pDispatcher->threadPool().capacity());
		activator.stop(pBundle->context());
	}
}


void AbstractWebServerBundleActivatorTest::testWithoutDispatcher()
{
	auto pBundle = std::make_unique<TemporaryBundle>(SERVER_PROPERTIES);
	TestActivator activator(false);

	activator.start(pBundle->context());

	assertTrue (pBundle->registry().findByName("osp.web.server") == nullptr);
	activator.stop(pBundle->context());
}


void AbstractWebServerBundleActivatorTest::testHeadersNotDecodedByDefault()
{
	auto pBundle = std::make_unique<TemporaryBundle>(SERVER_PROPERTIES);
	Poco::AutoPtr<WebServerDispatcher> pDispatcher = registerDispatcher(*pBundle);
	TestActivator activator(false);

	activator.start(pBundle->context());

	assertFalse (serverParams(*pBundle)->getAutoDecodeHeaders());
	activator.stop(pBundle->context());
}


void AbstractWebServerBundleActivatorTest::testAutoDecodeHeadersSetting()
{
	auto pBundle = std::make_unique<TemporaryBundle>(SERVER_PROPERTIES + "autoDecodeHeaders = true\n");
	Poco::AutoPtr<WebServerDispatcher> pDispatcher = registerDispatcher(*pBundle);
	TestActivator activator(false);

	activator.start(pBundle->context());
	assertTrue (serverParams(*pBundle)->getAutoDecodeHeaders());
	activator.stop(pBundle->context());

	// The global application configuration overrides the bundle property.
	Poco::AutoPtr<Poco::Util::MapConfiguration> pGlobalConfig(new Poco::Util::MapConfiguration);
	pGlobalConfig->setBool("osp.web.server.autoDecodeHeaders", false);
	pBundle->registry().registerService(Poco::OSP::PreferencesService::SERVICE_NAME,
		new Poco::OSP::PreferencesService(pBundle->context()->persistentDirectory(), pGlobalConfig), Poco::OSP::Properties());
	activator.start(pBundle->context());
	assertFalse (serverParams(*pBundle)->getAutoDecodeHeaders());
	activator.stop(pBundle->context());
}


CppUnit::Test* AbstractWebServerBundleActivatorTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("AbstractWebServerBundleActivatorTest");

	CppUnit_addTest(pSuite, AbstractWebServerBundleActivatorTest, testStartServesRequests);
	CppUnit_addTest(pSuite, AbstractWebServerBundleActivatorTest, testStopReleasesServerAndThreads);
	CppUnit_addTest(pSuite, AbstractWebServerBundleActivatorTest, testRestart);
	CppUnit_addTest(pSuite, AbstractWebServerBundleActivatorTest, testSecureServerService);
	CppUnit_addTest(pSuite, AbstractWebServerBundleActivatorTest, testPortZeroDisablesServer);
	CppUnit_addTest(pSuite, AbstractWebServerBundleActivatorTest, testPortOutOfRangeDisablesServer);
	CppUnit_addTest(pSuite, AbstractWebServerBundleActivatorTest, testWithoutDispatcher);
	CppUnit_addTest(pSuite, AbstractWebServerBundleActivatorTest, testHeadersNotDecodedByDefault);
	CppUnit_addTest(pSuite, AbstractWebServerBundleActivatorTest, testAutoDecodeHeadersSetting);

	return pSuite;
}
