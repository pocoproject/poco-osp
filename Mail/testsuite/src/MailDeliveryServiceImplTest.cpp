//
// MailDeliveryServiceImplTest.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "MailDeliveryServiceImplTest.h"
#include "LogCapture.h"
#include "FakeSMTPServer.h"
#include "Poco/OSP/Mail/MailDeliveryService.h"
#include "Poco/OSP/Mail/MailDeliveryServiceImpl.h"
#include "Poco/Net/MailMessage.h"
#include "Poco/Net/MailRecipient.h"
#include "Poco/Net/NetException.h"
#include "Poco/Net/SMTPClientSession.h"
#include "Poco/Util/MapConfiguration.h"
#include "Poco/AutoPtr.h"
#include "Poco/Bugcheck.h"
#include "Poco/Exception.h"
#include "Poco/Logger.h"
#include "Poco/Message.h"
#include "Poco/Timespan.h"
#include "Poco/Types.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "CppUnit/TestCase.h"
#include "CppUnit/Test.h"
#include <atomic>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>


using Poco::OSP::Mail::MailDeliveryHandle;
using Poco::OSP::Mail::MailDeliveryServiceImpl;
using Poco::OSP::Mail::MailMessagePtr;
using Poco::Util::MapConfiguration;
using namespace std::string_literals;


namespace
{
	const std::string LOGGER_NAME("MailDeliveryServiceImplTest");
	constexpr long WAIT_MILLISECONDS = 10000;

	Poco::AutoPtr<MapConfiguration> bundleProperties()
		/// Returns the settings of the bundle.properties file of the Mail bundle.
	{
		Poco::AutoPtr<MapConfiguration> pDefaults = new MapConfiguration;
		pDefaults->setString("osp.mail.host"s, "localhost"s);
		pDefaults->setInt("osp.mail.port"s, 25);
		pDefaults->setInt("osp.mail.socketTimeout"s, 60);
		pDefaults->setInt("osp.mail.sessionTimeout"s, 30);
		pDefaults->setString("osp.mail.login.method"s, "none"s);
		pDefaults->setInt("osp.mail.limits.maxQueued"s, 100);
		pDefaults->setInt("osp.mail.limits.maxFailedLogins"s, 10);
		return pDefaults;
	}

	MailDeliveryServiceImpl::Configuration configuration(Poco::UInt16 port)
		/// Returns the configuration for the fake SMTP server on the given port.
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

	Poco::Logger& serviceLogger()
		/// Returns the logger of the services under test, which logs nothing
		/// below the fatal level, so that expected delivery errors do not
		/// appear in the test output.
	{
		Poco::Logger& logger = Poco::Logger::get(LOGGER_NAME + ".service"s);
		logger.setLevel(Poco::Message::PRIO_FATAL);
		return logger;
	}

	class FailingLoginService: public MailDeliveryServiceImpl
		/// A mail delivery service whose logins fail, with a Poco::Exception
		/// or with a std::exception.
	{
	public:
		FailingLoginService() = delete;
		FailingLoginService(const FailingLoginService&) = delete;
		FailingLoginService(FailingLoginService&&) = delete;
		FailingLoginService& operator = (const FailingLoginService&) = delete;
		FailingLoginService& operator = (FailingLoginService&&) = delete;

		FailingLoginService(const Configuration& config, bool stdException):
			MailDeliveryServiceImpl(config, serviceLogger()),
			_stdException(stdException)
		{
		}

		~FailingLoginService() override
		{
			try
			{
				stop();
			}
			catch (...)
			{
				poco_unexpected();
			}
		}

		[[nodiscard]] int logins() const
		{
			return _logins;
		}

	protected:
		[[nodiscard]] SMTPClientSessionPtr createSMTPClientSession(const Configuration&) override
		{
			++_logins;
			if (_stdException) throw std::runtime_error("login failed");
			throw Poco::Net::SMTPException("login failed");
		}

	private:
		const bool _stdException;
		std::atomic<int> _logins{0};
	};
}


MailDeliveryServiceImplTest::MailDeliveryServiceImplTest(const std::string& name): CppUnit::TestCase(name)
{
}


MailDeliveryServiceImplTest::~MailDeliveryServiceImplTest() = default;


void MailDeliveryServiceImplTest::testLoadConfigurationPrefersSettingsOverDefaults()
{
	MapConfiguration settings;
	settings.setString("osp.mail.host"s, "mail.example.com"s);
	settings.setInt("osp.mail.socketTimeout"s, 5);
	settings.setString("osp.mail.sender"s, "osp@example.com"s);

	const MailDeliveryServiceImpl::Configuration config = MailDeliveryServiceImpl::loadConfiguration(settings, *bundleProperties(), Poco::Logger::get(LOGGER_NAME));

	assertEqual ("mail.example.com"s, config.host);
	assertEqual (25, static_cast<int>(config.port));
	assertEqual ("osp@example.com"s, config.sender);
	assertTrue (config.socketTimeout == Poco::Timespan(5, 0));
	assertTrue (config.sessionTimeout == Poco::Timespan(30, 0));
	assertEqual (100, config.maxQueued);
	assertEqual (10, config.maxFailedLogins);
	assertTrue (config.loginMethod == Poco::Net::SMTPClientSession::AUTH_NONE);
}


void MailDeliveryServiceImplTest::testLoadConfigurationIgnoresCaseOfLoginMethod()
{
	MapConfiguration settings;
	settings.setString("osp.mail.login.method"s, "PLAIN"s);

	const MailDeliveryServiceImpl::Configuration config = MailDeliveryServiceImpl::loadConfiguration(settings, *bundleProperties(), Poco::Logger::get(LOGGER_NAME));

	assertTrue (config.loginMethod == Poco::Net::SMTPClientSession::AUTH_PLAIN);
}


void MailDeliveryServiceImplTest::testLoadConfigurationNamesInvalidLoginMethod()
{
	LogCapture capture(LOGGER_NAME);
	MapConfiguration settings;
	settings.setString("osp.mail.login.method"s, "digest"s);

	const MailDeliveryServiceImpl::Configuration config = MailDeliveryServiceImpl::loadConfiguration(settings, *bundleProperties(), Poco::Logger::get(LOGGER_NAME));

	assertTrue (config.loginMethod == Poco::Net::SMTPClientSession::AUTH_NONE);
	const std::vector<Poco::Message> messages = capture.channel().waitForMessages(1);
	assertEqual (1, static_cast<int>(messages.size()));
	assertTrue (messages[0].getPriority() == Poco::Message::PRIO_WARNING);
	assertTrue (messages[0].getText().find("digest"s) != std::string::npos);
}


void MailDeliveryServiceImplTest::testNewSessionAfterFailedDelivery()
{
	FakeSMTPServer server;
	server.setDropAfterMessages(1);
	MailDeliveryServiceImpl::Ptr pService = new MailDeliveryServiceImpl(configuration(server.port()), serviceLogger());
	pService->start();

	MailDeliveryHandle::Ptr pFirst = pService->sendMessage(message());
	MailDeliveryHandle::Ptr pSecond = pService->sendMessage(message());
	MailDeliveryHandle::Ptr pThird = pService->sendMessage(message());

	assertTrue (pThird->tryWait(WAIT_MILLISECONDS));
	assertTrue (pFirst->status() == MailDeliveryHandle::MAIL_DELIVERED);
	assertTrue (pSecond->status() == MailDeliveryHandle::MAIL_FAILED);
	assertTrue (pThird->status() == MailDeliveryHandle::MAIL_DELIVERED);
	assertEqual (2, server.connections());
	pService->stop();
}


void MailDeliveryServiceImplTest::testStopFailsQueuedMessages()
{
	FakeSMTPServer server;
	server.setSilent(true);
	MailDeliveryServiceImpl::Configuration config = configuration(server.port());
	config.socketTimeout = Poco::Timespan(1, 0);
	MailDeliveryServiceImpl::Ptr pService = new MailDeliveryServiceImpl(config, serviceLogger());
	LogCapture capture(LOGGER_NAME + ".service"s);
	pService->start();
	MailDeliveryHandle::Ptr pConnecting = pService->sendMessage(message());
	assertTrue (server.waitForConnections(1));
	MailMessagePtr pMessage = message();
	pMessage->setSubject("Queued report"s);
	MailDeliveryHandle::Ptr pQueued = pService->sendMessage(pMessage);

	pService = nullptr;

	assertTrue (pConnecting->tryWait(0));
	assertTrue (pConnecting->status() == MailDeliveryHandle::MAIL_FAILED);
	assertTrue (pQueued->tryWait(0));
	assertTrue (pQueued->status() == MailDeliveryHandle::MAIL_FAILED);
	assertTrue (dynamic_cast<Poco::IllegalStateException*>(pQueued->exception().get()) != nullptr);
	bool logged = false;
	for (const auto& msg: capture.channel().waitForMessages(1))
	{
		if (msg.getText() == "The message \"Queued report\" is not delivered: the mail delivery service has stopped.") logged = true;
		// The handle that tells the delivery thread to quit is no error.
		assertTrue (msg.getText().find("caught an error"s) == std::string::npos);
	}
	assertTrue (logged);
}


void MailDeliveryServiceImplTest::testSendMessageAfterStopThrows()
{
	MailDeliveryServiceImpl::Ptr pService = new MailDeliveryServiceImpl(configuration(25), serviceLogger());
	pService->start();
	pService->stop();

	try
	{
		pService->sendMessage(message());
		fail("a stopped service must not accept messages");
	}
	catch (Poco::IllegalStateException&)
	{
	}
}


void MailDeliveryServiceImplTest::testStatusWhileDelivering()
{
	FakeSMTPServer server;
	MailDeliveryServiceImpl::Ptr pService = new MailDeliveryServiceImpl(configuration(server.port()), serviceLogger());
	pService->start();
	MailDeliveryHandle::Ptr pHandle = pService->sendMessage(message());

	std::thread reader([pHandle]()
		{
			while (pHandle->status() != MailDeliveryHandle::MAIL_DELIVERED && pHandle->status() != MailDeliveryHandle::MAIL_FAILED)
			{
				static_cast<void>(pHandle->exception());
			}
		});
	const bool finished = pHandle->tryWait(WAIT_MILLISECONDS);
	reader.join();

	assertTrue (finished);
	assertTrue (pHandle->status() == MailDeliveryHandle::MAIL_DELIVERED);
	assertTrue (pHandle->exception().isNull());
	pService->stop();
}


void MailDeliveryServiceImplTest::testStandardExceptionFailsMessage()
{
	MailDeliveryServiceImpl::Configuration config = configuration(25);
	config.loginRetryDelay = 0;
	Poco::AutoPtr<FailingLoginService> pService = new FailingLoginService(config, true);
	pService->start();

	MailDeliveryHandle::Ptr pFirst = pService->sendMessage(message());
	MailDeliveryHandle::Ptr pSecond = pService->sendMessage(message());

	assertTrue (pFirst->tryWait(WAIT_MILLISECONDS));
	assertTrue (pFirst->status() == MailDeliveryHandle::MAIL_FAILED);
	assertTrue (pSecond->tryWait(WAIT_MILLISECONDS));
	assertTrue (pSecond->status() == MailDeliveryHandle::MAIL_FAILED);
	assertEqual (2, pService->logins());
}


void MailDeliveryServiceImplTest::testStandardExceptionLogNamesMessage()
{
	MailDeliveryServiceImpl::Configuration config = configuration(25);
	config.loginRetryDelay = 0;
	Poco::AutoPtr<FailingLoginService> pService = new FailingLoginService(config, true);
	LogCapture capture(LOGGER_NAME + ".service"s);
	pService->start();
	MailMessagePtr pMessage = message();
	pMessage->setSubject("Quarterly report"s);

	MailDeliveryHandle::Ptr pHandle = pService->sendMessage(pMessage);

	assertTrue (pHandle->tryWait(WAIT_MILLISECONDS));
	pService->stop();
	const std::vector<Poco::Message> messages = capture.channel().waitForMessages(1);
	assertEqual (1, static_cast<int>(messages.size()));
	assertTrue (messages[0].getPriority() == Poco::Message::PRIO_ERROR);
	assertTrue (messages[0].getText().find("\"Quarterly report\" is not delivered"s) != std::string::npos);
	assertTrue (messages[0].getText().find("login failed"s) != std::string::npos);
}


void MailDeliveryServiceImplTest::testEveryMessageGetsAllLoginAttempts()
{
	MailDeliveryServiceImpl::Configuration config = configuration(25);
	config.maxFailedLogins = 2;
	config.loginRetryDelay = 0;
	Poco::AutoPtr<FailingLoginService> pService = new FailingLoginService(config, false);
	pService->start();

	MailDeliveryHandle::Ptr pFirst = pService->sendMessage(message());
	assertTrue (pFirst->tryWait(WAIT_MILLISECONDS));
	assertEqual (2, pService->logins());
	MailDeliveryHandle::Ptr pSecond = pService->sendMessage(message());
	assertTrue (pSecond->tryWait(WAIT_MILLISECONDS));

	assertTrue (pSecond->status() == MailDeliveryHandle::MAIL_FAILED);
	assertEqual (4, pService->logins());
}


void MailDeliveryServiceImplTest::testFailedLoginLogNamesMessage()
{
	FakeSMTPServer server;
	server.setRejectLogin(true);
	MailDeliveryServiceImpl::Configuration config = configuration(server.port());
	config.loginMethod = Poco::Net::SMTPClientSession::AUTH_PLAIN;
	config.username = "mailuser"s;
	config.password = "secret"s;
	config.loginRetryDelay = 0;
	MailDeliveryServiceImpl::Ptr pService = new MailDeliveryServiceImpl(config, serviceLogger());
	LogCapture capture(LOGGER_NAME + ".service"s);
	pService->start();
	MailMessagePtr pMessage = message();
	pMessage->setSubject("Quarterly report"s);

	MailDeliveryHandle::Ptr pHandle = pService->sendMessage(pMessage);

	assertTrue (pHandle->tryWait(WAIT_MILLISECONDS));
	pService->stop();
	bool named = false;
	for (const auto& msg: capture.channel().waitForMessages(2))
	{
		if (msg.getPriority() == Poco::Message::PRIO_ERROR && msg.getText().find("Quarterly report"s) != std::string::npos) named = true;
	}
	assertTrue (named);
}


void MailDeliveryServiceImplTest::testRepeatedLoginFailureIsLoggedOnce()
{
	FakeSMTPServer server;
	server.setRejectLogin(true);
	MailDeliveryServiceImpl::Configuration config = configuration(server.port());
	config.loginMethod = Poco::Net::SMTPClientSession::AUTH_PLAIN;
	config.username = "mailuser"s;
	config.password = "secret"s;
	config.maxFailedLogins = 3;
	config.loginRetryDelay = 0;
	MailDeliveryServiceImpl::Ptr pService = new MailDeliveryServiceImpl(config, serviceLogger());
	LogCapture capture(LOGGER_NAME + ".service"s);
	pService->start();

	MailDeliveryHandle::Ptr pRefused = pService->sendMessage(message());
	assertTrue (pRefused->tryWait(WAIT_MILLISECONDS));
	server.setRejectLogin(false);
	MailDeliveryHandle::Ptr pDelivered = pService->sendMessage(message());
	assertTrue (pDelivered->tryWait(WAIT_MILLISECONDS));
	pService->stop();

	assertTrue (pRefused->status() == MailDeliveryHandle::MAIL_FAILED);
	assertTrue (pDelivered->status() == MailDeliveryHandle::MAIL_DELIVERED);
	const std::vector<Poco::Message> messages = capture.channel().waitForMessages(3);
	assertEqual (3, static_cast<int>(messages.size()));
	assertTrue (messages[0].getPriority() == Poco::Message::PRIO_ERROR);
	assertTrue (messages[0].getText().find("Failed to log in"s) != std::string::npos);
	assertTrue (messages[1].getPriority() == Poco::Message::PRIO_ERROR);
	assertTrue (messages[1].getText().find("after 3 failed logins"s) != std::string::npos);
	assertTrue (messages[2].getPriority() == Poco::Message::PRIO_INFORMATION);
	assertTrue (messages[2].getText().find("Logged in"s) != std::string::npos);
}


void MailDeliveryServiceImplTest::testMessageIsNotChanged()
{
	FakeSMTPServer server;
	MailDeliveryServiceImpl::Configuration config = configuration(server.port());
	config.clientHost = "client.example.com"s;
	config.generateMID = true;
	MailDeliveryServiceImpl::Ptr pService = new MailDeliveryServiceImpl(config, serviceLogger());
	pService->start();
	MailMessagePtr pMessage = message();

	MailDeliveryHandle::Ptr pHandle = pService->sendMessage(pMessage);

	assertTrue (pHandle->tryWait(WAIT_MILLISECONDS));
	assertTrue (pHandle->status() == MailDeliveryHandle::MAIL_DELIVERED);
	assertTrue (pMessage->getSender().empty());
	assertTrue (!pMessage->has("Message-ID"s));
	assertTrue (server.hasReceived("MAIL FROM: <osp@example.com>"s));
	const std::vector<std::string> data = server.messageData();
	assertEqual (1, static_cast<int>(data.size()));
	assertTrue (data[0].find("From: osp@example.com\r\n"s) != std::string::npos);
	assertTrue (data[0].find("@client.example.com>\r\n"s) != std::string::npos);
	pService->stop();
}


void MailDeliveryServiceImplTest::setUp()
{
}


void MailDeliveryServiceImplTest::tearDown()
{
}


CppUnit::Test* MailDeliveryServiceImplTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("MailDeliveryServiceImplTest");

	CppUnit_addTest(pSuite, MailDeliveryServiceImplTest, testLoadConfigurationPrefersSettingsOverDefaults);
	CppUnit_addTest(pSuite, MailDeliveryServiceImplTest, testLoadConfigurationIgnoresCaseOfLoginMethod);
	CppUnit_addTest(pSuite, MailDeliveryServiceImplTest, testLoadConfigurationNamesInvalidLoginMethod);
	CppUnit_addTest(pSuite, MailDeliveryServiceImplTest, testNewSessionAfterFailedDelivery);
	CppUnit_addTest(pSuite, MailDeliveryServiceImplTest, testStopFailsQueuedMessages);
	CppUnit_addTest(pSuite, MailDeliveryServiceImplTest, testSendMessageAfterStopThrows);
	CppUnit_addTest(pSuite, MailDeliveryServiceImplTest, testStatusWhileDelivering);
	CppUnit_addTest(pSuite, MailDeliveryServiceImplTest, testStandardExceptionFailsMessage);
	CppUnit_addTest(pSuite, MailDeliveryServiceImplTest, testStandardExceptionLogNamesMessage);
	CppUnit_addTest(pSuite, MailDeliveryServiceImplTest, testEveryMessageGetsAllLoginAttempts);
	CppUnit_addTest(pSuite, MailDeliveryServiceImplTest, testFailedLoginLogNamesMessage);
	CppUnit_addTest(pSuite, MailDeliveryServiceImplTest, testRepeatedLoginFailureIsLoggedOnce);
	CppUnit_addTest(pSuite, MailDeliveryServiceImplTest, testMessageIsNotChanged);

	return pSuite;
}
