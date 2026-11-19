//
// SecureWebServerBundleActivatorTest.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "SecureWebServerBundleActivatorTest.h"
#include "SecureWebServerBundleActivator.h"
#include "TemporaryBundle.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "CppUnit/TestCase.h"
#include "CppUnit/Test.h"
#include "Poco/OSP/Web/WebServerDispatcher.h"
#include "Poco/OSP/Web/WebServerService.h"
#include "Poco/OSP/Properties.h"
#include "Poco/OSP/ServiceRegistry.h"
#include "Poco/OSP/ServiceRef.h"
#include "Poco/Net/Context.h"
#include "Poco/Net/HTTPRequest.h"
#include "Poco/Net/HTTPResponse.h"
#include "Poco/Net/HTTPSClientSession.h"
// The handler pointers that initializeServer() takes delete these types.
#include "Poco/Net/InvalidCertificateHandler.h" // IWYU pragma: keep
#include "Poco/Net/NetSSL.h"
#include "Poco/Net/PrivateKeyPassphraseHandler.h" // IWYU pragma: keep
#include "Poco/Net/SSLManager.h"
#include "Poco/Net/ServerSocket.h"
#include "Poco/Net/SocketAddress.h"
#include "Poco/AutoPtr.h"
#include <string>


using Poco::OSP::Web::WebServerDispatcher;
using Poco::OSP::Web::WebServerService;
using Poco::OSP::ServiceRef;
using Poco::Net::Context;
using namespace std::string_literals;


namespace
{
	class TestActivator: public SecureWebServerBundleActivator
		/// Listens on an ephemeral port of the loopback interface,
		/// whatever port the settings name.
	{
	protected:
		[[nodiscard]] Poco::Net::ServerSocket createServerSocket(const Poco::Net::SocketAddress& /*address*/) override
		{
			return SecureWebServerBundleActivator::createServerSocket(Poco::Net::SocketAddress("127.0.0.1"s, 0));
		}
	};

	const std::string SERVER_PROPERTIES("securePort = 1\nmaxThreads = 2\n");
	const std::string CERTIFICATE_FILE(OSPSECUREWEBSERVER_TEST_DATA "certificate.pem");
	const std::string PRIVATE_KEY_FILE(OSPSECUREWEBSERVER_TEST_DATA "privatekey.pem");
}


SecureWebServerBundleActivatorTest::SecureWebServerBundleActivatorTest(const std::string& name): CppUnit::TestCase(name)
{
}


SecureWebServerBundleActivatorTest::~SecureWebServerBundleActivatorTest() = default;


void SecureWebServerBundleActivatorTest::setUp()
{
	Poco::Net::initializeSSL();
	// The server socket of the activator uses the default server context.
	Poco::Net::SSLManager::instance().initializeServer(nullptr, nullptr,
		new Context(Context::TLS_SERVER_USE, PRIVATE_KEY_FILE, CERTIFICATE_FILE, ""s, Context::VERIFY_NONE));
}


void SecureWebServerBundleActivatorTest::tearDown()
{
	Poco::Net::uninitializeSSL();
}


void SecureWebServerBundleActivatorTest::testServesRequestsOverTLS()
{
	TemporaryBundle bundle(SERVER_PROPERTIES);
	WebServerDispatcher::Config config;
	config.pContext = bundle.context();
	Poco::AutoPtr<WebServerDispatcher> pDispatcher(new WebServerDispatcher(config));
	bundle.registry().registerService(WebServerDispatcher::SERVICE_NAME, pDispatcher, Poco::OSP::Properties());
	TestActivator activator;

	activator.start(bundle.context());

	ServiceRef::Ptr pRef = bundle.registry().findByName("osp.web.server.secure"s);
	assertTrue (pRef != nullptr);
	Context::Ptr pClientContext = new Context(Context::TLS_CLIENT_USE, CERTIFICATE_FILE, Context::VERIFY_RELAXED);
	Poco::Net::HTTPSClientSession session("127.0.0.1"s, pRef->castedInstance<WebServerService>()->port(),
		pClientContext);
	Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, "/missing"s);
	session.sendRequest(request);
	Poco::Net::HTTPResponse response;
	session.receiveResponse(response);
	assertTrue (response.getStatus() == Poco::Net::HTTPResponse::HTTP_NOT_FOUND);
	assertEqual ("127.0.0.1"s, session.serverCertificate().commonName());
	activator.stop(bundle.context());
}


CppUnit::Test* SecureWebServerBundleActivatorTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("SecureWebServerBundleActivatorTest");

	CppUnit_addTest(pSuite, SecureWebServerBundleActivatorTest, testServesRequestsOverTLS);

	return pSuite;
}
