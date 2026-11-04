//
// SecureMailDeliveryServiceImplTest.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "SecureMailDeliveryServiceImplTest.h"
#include "SecureMailDeliveryServiceImpl.h"
#include "FakeSMTPServer.h"
#include "TemporaryBundle.h"
#include "Poco/OSP/Mail/MailDeliveryService.h"
#include "Poco/OSP/BundleActivator.h"
#include "Poco/OSP/PreferencesService.h"
#include "Poco/OSP/Properties.h"
#include "Poco/OSP/ServiceRef.h"
#include "Poco/Net/Context.h"
#include "Poco/Net/MailMessage.h"
#include "Poco/Net/MailRecipient.h"
#include "Poco/Net/NetSSL.h"
#include "Poco/Net/SSLException.h"
#include "Poco/Net/SMTPClientSession.h"
#include "Poco/Net/SecureStreamSocket.h"
#include "Poco/Net/StreamSocket.h"
#include "Poco/Util/MapConfiguration.h"
#include "Poco/AutoPtr.h"
#include "Poco/ClassLoader.h"
#include "Poco/Environment.h"
#include "Poco/FileStream.h"
#include "Poco/Logger.h"
#include "Poco/Message.h"
#include "Poco/Path.h"
#include "Poco/StreamCopier.h"
#include "Poco/TemporaryFile.h"
#include "Poco/Timespan.h"
#include "Poco/Types.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "CppUnit/TestCase.h"
#include "CppUnit/Test.h"
#include <memory>
#include <string>


using Poco::OSP::Mail::MailDeliveryHandle;
using Poco::OSP::Mail::MailDeliveryService;
using Poco::OSP::Mail::MailDeliveryServiceImpl;
using Poco::OSP::Mail::MailMessagePtr;
using Poco::OSP::Mail::SecureMailDeliveryServiceImpl;
using namespace std::string_literals;


namespace
{
	constexpr long WAIT_MILLISECONDS = 10000;

	MailDeliveryServiceImpl::Configuration configuration(Poco::UInt16 port)
	{
		MailDeliveryServiceImpl::Configuration config;
		config.host = "127.0.0.1"s;
		config.port = port;
		config.sender = "osp@example.com"s;
		config.socketTimeout = Poco::Timespan(5, 0);
		config.sessionTimeout = Poco::Timespan(30, 0);
		config.maxQueued = 10;
		config.maxFailedLogins = 1;
		return config;
	}

	MailMessagePtr message()
	{
		MailMessagePtr pMessage = new Poco::Net::MailMessage;
		pMessage->addRecipient(Poco::Net::MailRecipient(Poco::Net::MailRecipient::PRIMARY_RECIPIENT, "user@example.com"s));
		pMessage->setSubject("Test"s);
		pMessage->setContent("Hello"s);
		return pMessage;
	}

	Poco::Logger& logger()
		/// Returns the logger of the services under test, which logs nothing
		/// below the fatal level, so that expected delivery errors do not
		/// appear in the test output.
	{
		Poco::Logger& logger = Poco::Logger::get("SecureMailDeliveryServiceImplTest"s);
		logger.setLevel(Poco::Message::PRIO_FATAL);
		return logger;
	}

	std::string sourceFile(const std::string& relativePath)
		/// Returns the path of the file with the given path (in UNIX notation)
		/// relative to the SecureMail sources.
	{
		Poco::Path path(Poco::Environment::get("POCO_BASE"s));
		path.makeDirectory();
		path.pushDirectory("SecureMail"s);
		path.append(Poco::Path(relativePath, Poco::Path::PATH_UNIX));
		return path.toString();
	}

	std::string dataFile(const std::string& name)
		/// Returns the path of the given file in the test data directory.
	{
		return sourceFile("testsuite/data/" + name);
	}

	Poco::Net::Context::Params serverParams()
		/// Returns the parameters of a server context with the certificate for localhost.
	{
		Poco::Net::Context::Params params;
		params.privateKeyFile = dataFile("localhost.key"s);
		params.certificateFile = dataFile("localhost.pem"s);
		params.verificationMode = Poco::Net::Context::VERIFY_NONE;
		return params;
	}

	FakeSMTPServer::SecureFunction secureServer(const Poco::Net::Context::Ptr& pContext)
		/// Returns the function that starts TLS on the server side with the given context.
	{
		return [pContext](const Poco::Net::StreamSocket& socket)
			{
				return Poco::Net::StreamSocket(Poco::Net::SecureStreamSocket::attach(socket, pContext));
			};
	}

	FakeSMTPServer::SecureFunction secureServer()
		/// Returns the function that starts TLS on the server side, with the
		/// certificate for localhost.
	{
		return secureServer(new Poco::Net::Context(Poco::Net::Context::TLS_SERVER_USE, serverParams()));
	}

	Poco::Net::Context::Ptr strictClientContext(bool verifyHostName)
		/// Returns a client context that trusts the certificate for localhost and
		/// uses VERIFY_STRICT, with which the host name of the certificate is also
		/// checked for a server at a loopback address.
	{
		Poco::Net::Context::Params params;
		params.caLocation = dataFile("localhost.pem"s);
		params.verificationMode = Poco::Net::Context::VERIFY_STRICT;
		Poco::Net::Context::Ptr pContext = new Poco::Net::Context(Poco::Net::Context::TLS_CLIENT_USE, params);
		pContext->enableExtendedCertificateVerification(verifyHostName);
		return pContext;
	}

	FakeSMTPServer::TLSMode serverMode(SecureMailDeliveryServiceImpl::Mode mode)
		/// Returns the TLS mode of the server for the given mode of the client.
	{
		return mode == SecureMailDeliveryServiceImpl::SMTP_STARTTLS ? FakeSMTPServer::TLSMode::STARTTLS : FakeSMTPServer::TLSMode::ON_CONNECT;
	}

	MailDeliveryServiceImpl::Configuration loginConfiguration(Poco::UInt16 port, const std::string& host)
		/// Returns the configuration for a server at the given host name,
		/// with a login that sends the password.
	{
		MailDeliveryServiceImpl::Configuration config = configuration(port);
		config.host = host;
		config.loginMethod = Poco::Net::SMTPClientSession::AUTH_PLAIN;
		config.username = "mailuser"s;
		config.password = "secret"s;
		return config;
	}

	const std::string LEGACY_CIPHERS("ALL:@SECLEVEL=0"s);
		/// A cipher list with which OpenSSL also accepts TLS 1.1.

	Poco::Net::Context::Ptr clientContext()
		/// Returns the client context that trusts the certificate for localhost.
	{
		Poco::Util::MapConfiguration settings;
		settings.setString("osp.mail.tls.caLocation"s, dataFile("localhost.pem"s));
		return SecureMailDeliveryServiceImpl::createContext(settings);
	}
}


SecureMailDeliveryServiceImplTest::SecureMailDeliveryServiceImplTest(const std::string& name): CppUnit::TestCase(name)
{
}


SecureMailDeliveryServiceImplTest::~SecureMailDeliveryServiceImplTest() = default;


void SecureMailDeliveryServiceImplTest::testDestroyWhileConnecting()
{
	FakeSMTPServer server;
	server.setGreetingDelay(Poco::Timespan(0, 500000));
	MailDeliveryHandle::Ptr pHandle;
	{
		// The service holds the only reference to the TLS context, which it
		// releases when it is destroyed.
		SecureMailDeliveryServiceImpl::Ptr pService = new SecureMailDeliveryServiceImpl(SecureMailDeliveryServiceImpl::SMTP_STARTTLS, configuration(server.port()), logger(),
			new Poco::Net::Context(Poco::Net::Context::TLS_CLIENT_USE, Poco::Net::Context::Params()));
		pService->start();
		pHandle = pService->sendMessage(message());
		assertTrue (server.waitForConnections(1));
	}
	assertTrue (pHandle->tryWait(0));
}


void SecureMailDeliveryServiceImplTest::testRefusedStartTLSFailsDelivery()
{
	FakeSMTPServer server;
	MailDeliveryServiceImpl::Configuration config = configuration(server.port());
	config.loginMethod = Poco::Net::SMTPClientSession::AUTH_PLAIN;
	config.username = "mailuser"s;
	config.password = "secret"s;
	SecureMailDeliveryServiceImpl::Ptr pService = new SecureMailDeliveryServiceImpl(SecureMailDeliveryServiceImpl::SMTP_STARTTLS, config, logger(),
		new Poco::Net::Context(Poco::Net::Context::TLS_CLIENT_USE, Poco::Net::Context::Params()));
	pService->start();

	MailDeliveryHandle::Ptr pHandle = pService->sendMessage(message());

	assertTrue (pHandle->tryWait(WAIT_MILLISECONDS));
	assertTrue (pHandle->status() == MailDeliveryHandle::MAIL_FAILED);
	assertTrue (server.hasReceived("STARTTLS"s));
	assertTrue (!server.hasReceived("AUTH"s));
	assertTrue (!server.hasReceived("MAIL FROM"s));
	pService->stop();
}


void SecureMailDeliveryServiceImplTest::testStartTLSWithTrustedCertificate()
{
	FakeSMTPServer server(FakeSMTPServer::TLSMode::STARTTLS, secureServer());
	MailDeliveryServiceImpl::Configuration config = configuration(server.port());
	config.loginMethod = Poco::Net::SMTPClientSession::AUTH_PLAIN;
	config.username = "mailuser"s;
	config.password = "secret"s;
	SecureMailDeliveryServiceImpl::Ptr pService = new SecureMailDeliveryServiceImpl(SecureMailDeliveryServiceImpl::SMTP_STARTTLS, config, logger(), clientContext());
	pService->start();

	MailDeliveryHandle::Ptr pHandle = pService->sendMessage(message());

	assertTrue (pHandle->tryWait(WAIT_MILLISECONDS));
	assertTrue (pHandle->status() == MailDeliveryHandle::MAIL_DELIVERED);
	assertEqual (1, server.messages());
	pService->stop();
}


void SecureMailDeliveryServiceImplTest::testStartTLSRejectsUntrustedCertificate()
{
	FakeSMTPServer server(FakeSMTPServer::TLSMode::STARTTLS, secureServer());
	MailDeliveryServiceImpl::Configuration config = configuration(server.port());
	config.loginMethod = Poco::Net::SMTPClientSession::AUTH_PLAIN;
	config.username = "mailuser"s;
	config.password = "secret"s;
	SecureMailDeliveryServiceImpl::Ptr pService = new SecureMailDeliveryServiceImpl(SecureMailDeliveryServiceImpl::SMTP_STARTTLS, config, logger(),
		SecureMailDeliveryServiceImpl::createContext(Poco::Util::MapConfiguration()));
	pService->start();

	MailDeliveryHandle::Ptr pHandle = pService->sendMessage(message());

	assertTrue (pHandle->tryWait(WAIT_MILLISECONDS));
	assertTrue (pHandle->status() == MailDeliveryHandle::MAIL_FAILED);
	assertTrue (!server.hasReceived("AUTH"s));
	assertEqual (0, server.messages());
	pService->stop();
}


void SecureMailDeliveryServiceImplTest::testCertificateForConfiguredHostNameIsAccepted()
{
	for (const SecureMailDeliveryServiceImpl::Mode mode: {SecureMailDeliveryServiceImpl::SMTP_STARTTLS, SecureMailDeliveryServiceImpl::SMTP_OVER_TLS})
	{
		FakeSMTPServer server(serverMode(mode), secureServer(), "localhost"s);
		SecureMailDeliveryServiceImpl::Ptr pService = new SecureMailDeliveryServiceImpl(mode, loginConfiguration(server.port(), "localhost"s), logger(), strictClientContext(true));
		pService->start();

		MailDeliveryHandle::Ptr pHandle = pService->sendMessage(message());

		assertTrue (pHandle->tryWait(WAIT_MILLISECONDS));
		assertTrue (pHandle->status() == MailDeliveryHandle::MAIL_DELIVERED);
		assertEqual (1, server.messages());
		pService->stop();
	}
}


void SecureMailDeliveryServiceImplTest::testCertificateForOtherHostNameIsRejected()
{
	for (const SecureMailDeliveryServiceImpl::Mode mode: {SecureMailDeliveryServiceImpl::SMTP_STARTTLS, SecureMailDeliveryServiceImpl::SMTP_OVER_TLS})
	{
		// The certificate is for localhost, the configured host name is 127.0.0.1.
		FakeSMTPServer server(serverMode(mode), secureServer());
		SecureMailDeliveryServiceImpl::Ptr pService = new SecureMailDeliveryServiceImpl(mode, loginConfiguration(server.port(), "127.0.0.1"s), logger(), strictClientContext(true));
		pService->start();

		MailDeliveryHandle::Ptr pHandle = pService->sendMessage(message());

		assertTrue (pHandle->tryWait(WAIT_MILLISECONDS));
		assertTrue (pHandle->status() == MailDeliveryHandle::MAIL_FAILED);
		assertTrue (dynamic_cast<Poco::Net::CertificateValidationException*>(pHandle->exception().get()) != nullptr);
		assertTrue (!server.hasReceived("AUTH"s));
		assertEqual (0, server.messages());
		pService->stop();
	}
}


void SecureMailDeliveryServiceImplTest::testOtherHostNameIsAcceptedWithoutHostNameVerification()
{
	for (const SecureMailDeliveryServiceImpl::Mode mode: {SecureMailDeliveryServiceImpl::SMTP_STARTTLS, SecureMailDeliveryServiceImpl::SMTP_OVER_TLS})
	{
		FakeSMTPServer server(serverMode(mode), secureServer());
		SecureMailDeliveryServiceImpl::Ptr pService = new SecureMailDeliveryServiceImpl(mode, loginConfiguration(server.port(), "127.0.0.1"s), logger(), strictClientContext(false));
		pService->start();

		MailDeliveryHandle::Ptr pHandle = pService->sendMessage(message());

		assertTrue (pHandle->tryWait(WAIT_MILLISECONDS));
		assertTrue (pHandle->status() == MailDeliveryHandle::MAIL_DELIVERED);
		pService->stop();
	}
}


void SecureMailDeliveryServiceImplTest::testServerWithoutTLS12IsRefused()
{
	Poco::Net::Context::Params tls11Params = serverParams();
	tls11Params.cipherList = LEGACY_CIPHERS;
	Poco::Net::Context::Ptr pTLS11Context = new Poco::Net::Context(Poco::Net::Context::TLS_SERVER_USE, tls11Params);
	pTLS11Context->disableProtocols(Poco::Net::Context::PROTO_TLSV1_2 | Poco::Net::Context::PROTO_TLSV1_3);
	FakeSMTPServer server(FakeSMTPServer::TLSMode::ON_CONNECT, secureServer(pTLS11Context));
	Poco::Util::MapConfiguration settings;
	settings.setString("osp.mail.tls.caLocation"s, dataFile("localhost.pem"s));
	settings.setString("osp.mail.tls.ciphers"s, LEGACY_CIPHERS);
	SecureMailDeliveryServiceImpl::Ptr pService = new SecureMailDeliveryServiceImpl(SecureMailDeliveryServiceImpl::SMTP_OVER_TLS, configuration(server.port()), logger(),
		SecureMailDeliveryServiceImpl::createContext(settings));
	pService->start();
	// The same client context, but without a minimum protocol version.
	Poco::Net::Context::Params legacyParams;
	legacyParams.caLocation = dataFile("localhost.pem"s);
	legacyParams.cipherList = LEGACY_CIPHERS;
	SecureMailDeliveryServiceImpl::Ptr pLegacyService = new SecureMailDeliveryServiceImpl(SecureMailDeliveryServiceImpl::SMTP_OVER_TLS, configuration(server.port()), logger(),
		new Poco::Net::Context(Poco::Net::Context::TLS_CLIENT_USE, legacyParams));
	pLegacyService->start();

	MailDeliveryHandle::Ptr pHandle = pService->sendMessage(message());
	assertTrue (pHandle->tryWait(WAIT_MILLISECONDS));
	MailDeliveryHandle::Ptr pLegacyHandle = pLegacyService->sendMessage(message());
	assertTrue (pLegacyHandle->tryWait(WAIT_MILLISECONDS));

	assertTrue (pHandle->status() == MailDeliveryHandle::MAIL_FAILED);
	assertTrue (pLegacyHandle->status() == MailDeliveryHandle::MAIL_DELIVERED);
	assertEqual (1, server.messages());
	pService->stop();
	pLegacyService->stop();
}


void SecureMailDeliveryServiceImplTest::testActivatorAppliesCALocation()
{
	FakeSMTPServer server(FakeSMTPServer::TLSMode::STARTTLS, secureServer(), "localhost"s);
	std::string bundleProperties;
	{
		Poco::FileInputStream istr(sourceFile("bundle/bundle.properties"s));
		Poco::StreamCopier::copyToString(istr, bundleProperties);
	}
	TemporaryBundle bundles(bundleProperties);
	Poco::TemporaryFile persistency;
	Poco::AutoPtr<Poco::Util::MapConfiguration> pSettings = new Poco::Util::MapConfiguration;
	pSettings->setBool("osp.mail.enable"s, true);
	pSettings->setBool("osp.mail.secure"s, true);
	pSettings->setString("osp.mail.host"s, "localhost"s);
	pSettings->setInt("osp.mail.port"s, server.port());
	pSettings->setString("osp.mail.sender"s, "osp@example.com"s);
	pSettings->setString("osp.mail.tls.caLocation"s, dataFile("localhost.pem"s));
	bundles.registry().registerService(Poco::OSP::PreferencesService::SERVICE_NAME, new Poco::OSP::PreferencesService(Poco::Path(persistency.path()), pSettings), Poco::OSP::Properties());
	Poco::ClassLoader<Poco::OSP::BundleActivator> loader;
	loader.loadLibrary(SECUREMAIL_BUNDLE_LIBRARY);
	std::unique_ptr<Poco::OSP::BundleActivator> pActivator(loader.create("Poco::OSP::Mail::SecureBundleActivator"s));
	pActivator->start(bundles.context());
	Poco::OSP::ServiceRef::Ptr pRef = bundles.registry().findByName(MailDeliveryService::SERVICE_NAME);
	assertTrue (pRef != nullptr);

	MailDeliveryHandle::Ptr pHandle = pRef->castedInstance<MailDeliveryService>()->sendMessage(message());

	assertTrue (pHandle->tryWait(WAIT_MILLISECONDS));
	assertTrue (pHandle->status() == MailDeliveryHandle::MAIL_DELIVERED);
	assertEqual (1, server.messages());
	pActivator->stop(bundles.context());
}


void SecureMailDeliveryServiceImplTest::testHandshakeTimesOut()
{
	// Declared before the server, so that the server, which ends the connection,
	// is destroyed first even if the handshake does not time out.
	SecureMailDeliveryServiceImpl::Ptr pService;
	FakeSMTPServer server(FakeSMTPServer::TLSMode::ON_CONNECT, secureServer());
	server.setSilent(true);
	MailDeliveryServiceImpl::Configuration config = configuration(server.port());
	config.socketTimeout = Poco::Timespan(1, 0);
	pService = new SecureMailDeliveryServiceImpl(SecureMailDeliveryServiceImpl::SMTP_OVER_TLS, config, logger(), clientContext());
	pService->start();

	MailDeliveryHandle::Ptr pHandle = pService->sendMessage(message());

	assertTrue (pHandle->tryWait(5000));
	assertTrue (pHandle->status() == MailDeliveryHandle::MAIL_FAILED);
	pService->stop();
}


void SecureMailDeliveryServiceImplTest::testContextVerifiesHostNameByDefault()
{
	assertTrue (SecureMailDeliveryServiceImpl::createContext(Poco::Util::MapConfiguration())->extendedCertificateVerificationEnabled());
}


void SecureMailDeliveryServiceImplTest::setUp()
{
	Poco::Net::initializeSSL();
}


void SecureMailDeliveryServiceImplTest::tearDown()
{
	Poco::Net::uninitializeSSL();
}


CppUnit::Test* SecureMailDeliveryServiceImplTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("SecureMailDeliveryServiceImplTest");

	CppUnit_addTest(pSuite, SecureMailDeliveryServiceImplTest, testDestroyWhileConnecting);
	CppUnit_addTest(pSuite, SecureMailDeliveryServiceImplTest, testRefusedStartTLSFailsDelivery);
	CppUnit_addTest(pSuite, SecureMailDeliveryServiceImplTest, testStartTLSWithTrustedCertificate);
	CppUnit_addTest(pSuite, SecureMailDeliveryServiceImplTest, testStartTLSRejectsUntrustedCertificate);
	CppUnit_addTest(pSuite, SecureMailDeliveryServiceImplTest, testContextVerifiesHostNameByDefault);
	CppUnit_addTest(pSuite, SecureMailDeliveryServiceImplTest, testCertificateForConfiguredHostNameIsAccepted);
	CppUnit_addTest(pSuite, SecureMailDeliveryServiceImplTest, testCertificateForOtherHostNameIsRejected);
	CppUnit_addTest(pSuite, SecureMailDeliveryServiceImplTest, testOtherHostNameIsAcceptedWithoutHostNameVerification);
	CppUnit_addTest(pSuite, SecureMailDeliveryServiceImplTest, testServerWithoutTLS12IsRefused);
	CppUnit_addTest(pSuite, SecureMailDeliveryServiceImplTest, testActivatorAppliesCALocation);
	CppUnit_addTest(pSuite, SecureMailDeliveryServiceImplTest, testHandshakeTimesOut);

	return pSuite;
}
