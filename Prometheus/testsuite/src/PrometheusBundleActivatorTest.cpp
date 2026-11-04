//
// PrometheusBundleActivatorTest.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "PrometheusBundleActivatorTest.h"
#include "LogCapture.h"
#include "TemporaryBundle.h"
#include "Poco/OSP/Auth/AuthService.h"
#include "Poco/OSP/BundleActivator.h"
#include "Poco/OSP/PreferencesService.h"
#include "Poco/OSP/Properties.h"
#include "Poco/OSP/ServiceRef.h"
#include "Poco/OSP/Web/MediaTypeMapper.h"
#include "Poco/OSP/Web/WebServerDispatcher.h"
#include "Poco/OSP/Web/WebSessionManager.h"
#include "Poco/Prometheus/Registry.h"
#include "Poco/Net/HTTPBasicCredentials.h"
#include "Poco/Net/HTTPClientSession.h"
#include "Poco/Net/HTTPMessage.h"
#include "Poco/Net/HTTPRequest.h"
#include "Poco/Net/HTTPResponse.h"
#include "Poco/Net/ServerSocket.h"
#include "Poco/Net/Socket.h"
#include "Poco/Net/SocketAddress.h"
#include "Poco/Net/StreamSocket.h"
#include "Poco/Util/MapConfiguration.h"
#include "Poco/AutoPtr.h"
#include "Poco/ClassLoader.h"
#include "Poco/Exception.h"
#include "Poco/Message.h"
#include "Poco/NullStream.h"
#include "Poco/Path.h"
#include "Poco/StreamCopier.h"
#include "Poco/String.h"
#include "Poco/TemporaryFile.h"
#include "Poco/Timespan.h"
#include "Poco/Types.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "CppUnit/TestCase.h"
#include "CppUnit/Test.h"
#include <array>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>


using Poco::OSP::PreferencesService;
using namespace std::string_literals;


namespace
{
	const Poco::Timespan TIMEOUT(10, 0);

	Poco::UInt16 unusedPort()
		/// Returns a port on 127.0.0.1 that no socket uses at the moment.
		/// The metrics server does not start on port 0.
	{
		Poco::Net::ServerSocket socket(Poco::Net::SocketAddress("127.0.0.1"s, 0));
		const Poco::UInt16 port = socket.address().port();
		socket.close();
		return port;
	}

	class TestAuthService: public Poco::OSP::Auth::AuthService
		/// Knows the users "alice", with the permission "metrics", and "bob",
		/// both with the password "secret".
	{
	public:
		TestAuthService() = default;

		~TestAuthService() override = default;

		TestAuthService(const TestAuthService&) = delete;
		TestAuthService(TestAuthService&&) = delete;
		TestAuthService& operator = (const TestAuthService&) = delete;
		TestAuthService& operator = (TestAuthService&&) = delete;

		bool authenticate(const std::string& userName, const std::string& credentials) const override
		{
			return userExists(userName) && credentials == "secret";
		}

		bool authorize(const std::string& userName, const std::string& permission) const override
		{
			return userName == "alice" && permission == "metrics";
		}

		bool authorize(const std::string& userName, const std::string& scope, const std::string& permission) const override
		{
			return authorize(userName, permission);
		}

		bool userExists(const std::string& userName) const override
		{
			return userName == "alice" || userName == "bob";
		}
	};

	Poco::Net::HTTPResponse::HTTPStatus getMetrics(Poco::UInt16 port, const std::string& username, const std::string& password)
		/// Requests the metrics, with Basic credentials if username is not empty,
		/// and returns the status of the response.
	{
		Poco::Net::HTTPClientSession session("127.0.0.1"s, port);
		session.setTimeout(TIMEOUT);
		Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, "/metrics"s, Poco::Net::HTTPMessage::HTTP_1_1);
		if (!username.empty()) Poco::Net::HTTPBasicCredentials(username, password).authenticate(request);
		session.sendRequest(request);
		Poco::Net::HTTPResponse response;
		Poco::NullOutputStream discard;
		Poco::StreamCopier::copyStream(session.receiveResponse(response), discard);
		return response.getStatus();
	}

	std::string receiveResponse(Poco::Net::StreamSocket& socket)
		/// Receives one chunked HTTP response.
	{
		std::string response;
		std::array<char, 4096> buffer{};
		while (!Poco::endsWith(response, "\r\n0\r\n\r\n"s))
		{
			const int n = socket.receiveBytes(buffer.data(), static_cast<int>(buffer.size()));
			if (n <= 0) break;
			response.append(buffer.data(), static_cast<std::size_t>(n));
		}
		return response;
	}
}


struct PrometheusBundleActivatorTest::Fixture
	/// The bundle activator of the bundle library, with a bundle context in
	/// whose registry the preferences service is registered.
{
	Fixture():
		bundles(""s),
		pSettings(new Poco::Util::MapConfiguration),
		pPreferences(new PreferencesService(Poco::Path(persistency.path()), pSettings))
	{
		bundles.registry().registerService(PreferencesService::SERVICE_NAME, pPreferences, Poco::OSP::Properties());
		loader.loadLibrary(PROMETHEUS_BUNDLE_LIBRARY);
		pActivator.reset(loader.create("Poco::OSP::Prometheus::BundleActivator"s));
		pSettings->setBool("prometheus.enable"s, true);
		pSettings->setString("prometheus.host"s, "127.0.0.1"s);
		pSettings->setInt("prometheus.port"s, 0);
	}

	~Fixture() = default;

	Fixture(const Fixture&) = delete;
	Fixture(Fixture&&) = delete;
	Fixture& operator = (const Fixture&) = delete;
	Fixture& operator = (Fixture&&) = delete;

	void start()
	{
		pActivator->start(bundles.context());
		started = true;
	}

	void stop()
	{
		started = false;
		pActivator->stop(bundles.context());
	}

	TemporaryBundle bundles;
	Poco::TemporaryFile persistency;
	Poco::AutoPtr<Poco::Util::MapConfiguration> pSettings;
	PreferencesService::Ptr pPreferences;
	Poco::ClassLoader<Poco::OSP::BundleActivator> loader;
	// Declared after the loader, so that it is destroyed before the library is unloaded.
	std::unique_ptr<Poco::OSP::BundleActivator> pActivator;
	bool started = false;
};


PrometheusBundleActivatorTest::PrometheusBundleActivatorTest(const std::string& name): CppUnit::TestCase(name)
{
}


PrometheusBundleActivatorTest::~PrometheusBundleActivatorTest() = default;


void PrometheusBundleActivatorTest::testKeepAliveTimeoutIsInSeconds()
{
	const Poco::UInt16 port = unusedPort();
	_pFixture->pSettings->setInt("prometheus.port"s, port);
	_pFixture->pSettings->setBool("prometheus.server.keepAlive"s, true);
	_pFixture->start();

	Poco::Net::StreamSocket socket(Poco::Net::SocketAddress("127.0.0.1"s, port));
	socket.setReceiveTimeout(TIMEOUT);
	const std::string request("GET /metrics HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n"s);
	socket.sendBytes(request.data(), static_cast<int>(request.size()));
	assertTrue (Poco::startsWith(receiveResponse(socket), "HTTP/1.1 200"s));

	// With the default keep-alive time of 10 seconds, the server keeps the
	// connection open for the next request.
	assertTrue (!socket.poll(Poco::Timespan(1, 0), Poco::Net::Socket::SELECT_READ));
	_pFixture->stop();
}


void PrometheusBundleActivatorTest::testFailedStartCanBeRepeated()
{
	Poco::Net::ServerSocket blocker;
	blocker.bind(Poco::Net::SocketAddress("127.0.0.1"s, 0), false, false);
	blocker.listen();
	_pFixture->pSettings->setInt("prometheus.port"s, blocker.address().port());
	try
	{
		_pFixture->start();
		fail("the metrics port is in use");
	}
	catch (Poco::IOException&)
	{
	}
	_pFixture->started = false;
	assertTrue (Poco::Prometheus::Registry::defaultRegistry().findCollector("osp_web_server"s) == nullptr);

	blocker.close();
	_pFixture->start();

	assertTrue (Poco::Prometheus::Registry::defaultRegistry().findCollector("osp_web_server"s) != nullptr);
	_pFixture->stop();
}


void PrometheusBundleActivatorTest::testDispatcherCanRegisterAgain()
{
	_pFixture->start();
	Poco::OSP::Web::WebServerDispatcher::Config config;
	config.pContext = _pFixture->bundles.context();
	config.pMediaTypeMapper = new Poco::OSP::Web::MediaTypeMapper;
	Poco::OSP::Web::WebServerDispatcher::Ptr pDispatcher = new Poco::OSP::Web::WebServerDispatcher(config);
	Poco::OSP::ServiceRef::Ptr pRef = _pFixture->bundles.registry().registerService("osp.web.dispatcher"s, pDispatcher, Poco::OSP::Properties());
	_pFixture->bundles.registry().unregisterService(pRef);

	pRef = _pFixture->bundles.registry().registerService("osp.web.dispatcher"s, pDispatcher, Poco::OSP::Properties());

	assertTrue (Poco::Prometheus::Registry::defaultRegistry().findCollector("osp_web_requests_total"s) != nullptr);
	_pFixture->stop();
	_pFixture->bundles.registry().unregisterService(pRef);
}


void PrometheusBundleActivatorTest::testStopUnregistersCollectors()
{
	_pFixture->start();
	Poco::OSP::Web::WebSessionManager::Ptr pSessionManager = new Poco::OSP::Web::WebSessionManager(_pFixture->bundles.context());
	Poco::OSP::ServiceRef::Ptr pRef = _pFixture->bundles.registry().registerService("osp.web.session"s, pSessionManager, Poco::OSP::Properties());
	assertTrue (Poco::Prometheus::Registry::defaultRegistry().findCollector("osp_web_session"s) != nullptr);

	_pFixture->stop();

	const Poco::Prometheus::Registry& registry = Poco::Prometheus::Registry::defaultRegistry();
	assertTrue (registry.findCollector("osp_web_session"s) == nullptr);
	assertTrue (registry.findCollector("osp_web_server"s) == nullptr);
	assertTrue (registry.findCollector("process"s) == nullptr);
	_pFixture->bundles.registry().unregisterService(pRef);
}


void PrometheusBundleActivatorTest::testMetricsNeedConfiguredPermission()
{
	const Poco::UInt16 port = unusedPort();
	_pFixture->pSettings->setInt("prometheus.port"s, port);
	_pFixture->pSettings->setString("prometheus.permission"s, "metrics"s);
	Poco::OSP::ServiceRef::Ptr pRef = _pFixture->bundles.registry().registerService("osp.auth"s, new TestAuthService, Poco::OSP::Properties());
	_pFixture->start();

	assertTrue (getMetrics(port, ""s, ""s) == Poco::Net::HTTPResponse::HTTP_UNAUTHORIZED);
	assertTrue (getMetrics(port, "alice"s, "wrong"s) == Poco::Net::HTTPResponse::HTTP_UNAUTHORIZED);
	assertTrue (getMetrics(port, "bob"s, "secret"s) == Poco::Net::HTTPResponse::HTTP_FORBIDDEN);
	assertTrue (getMetrics(port, "alice"s, "secret"s) == Poco::Net::HTTPResponse::HTTP_OK);
	_pFixture->stop();
	_pFixture->bundles.registry().unregisterService(pRef);
}


void PrometheusBundleActivatorTest::testMetricsWithoutPermission()
{
	const Poco::UInt16 port = unusedPort();
	_pFixture->pSettings->setInt("prometheus.port"s, port);
	_pFixture->start();

	assertTrue (getMetrics(port, ""s, ""s) == Poco::Net::HTTPResponse::HTTP_OK);
	_pFixture->stop();
}


void PrometheusBundleActivatorTest::testMissingAuthServiceIsLoggedOnce()
{
	const Poco::UInt16 port = unusedPort();
	_pFixture->pSettings->setInt("prometheus.port"s, port);
	_pFixture->pSettings->setString("prometheus.permission"s, "metrics"s);
	_pFixture->start();
	LogCapture capture(_pFixture->bundles.context()->logger().name());

	assertTrue (getMetrics(port, "alice"s, "secret"s) == Poco::Net::HTTPResponse::HTTP_SERVICE_UNAVAILABLE);
	assertTrue (getMetrics(port, "alice"s, "secret"s) == Poco::Net::HTTPResponse::HTTP_SERVICE_UNAVAILABLE);
	Poco::OSP::ServiceRef::Ptr pRef = _pFixture->bundles.registry().registerService("osp.auth"s, new TestAuthService, Poco::OSP::Properties());
	assertTrue (getMetrics(port, "alice"s, "secret"s) == Poco::Net::HTTPResponse::HTTP_OK);
	assertTrue (getMetrics(port, "alice"s, "secret"s) == Poco::Net::HTTPResponse::HTTP_OK);

	// The server logs before it sends the response, so every message is there.
	const std::vector<Poco::Message> messages = capture.channel().waitForMessages(2);
	assertEqual (2, static_cast<int>(messages.size()));
	assertTrue (messages[0].getPriority() == Poco::Message::PRIO_WARNING);
	assertTrue (messages[1].getPriority() == Poco::Message::PRIO_INFORMATION);
	_pFixture->stop();
	_pFixture->bundles.registry().unregisterService(pRef);
}


void PrometheusBundleActivatorTest::setUp()
{
	_pFixture = std::make_unique<Fixture>();
}


void PrometheusBundleActivatorTest::tearDown()
{
	if (_pFixture->started) _pFixture->stop();
	_pFixture.reset();
}


CppUnit::Test* PrometheusBundleActivatorTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("PrometheusBundleActivatorTest");

	CppUnit_addTest(pSuite, PrometheusBundleActivatorTest, testKeepAliveTimeoutIsInSeconds);
	CppUnit_addTest(pSuite, PrometheusBundleActivatorTest, testFailedStartCanBeRepeated);
	CppUnit_addTest(pSuite, PrometheusBundleActivatorTest, testDispatcherCanRegisterAgain);
	CppUnit_addTest(pSuite, PrometheusBundleActivatorTest, testStopUnregistersCollectors);
	CppUnit_addTest(pSuite, PrometheusBundleActivatorTest, testMetricsNeedConfiguredPermission);
	CppUnit_addTest(pSuite, PrometheusBundleActivatorTest, testMetricsWithoutPermission);
	CppUnit_addTest(pSuite, PrometheusBundleActivatorTest, testMissingAuthServiceIsLoggedOnce);

	return pSuite;
}
