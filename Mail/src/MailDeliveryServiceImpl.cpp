//
// MailDeliveryServiceImpl.cpp
//
// Library: OSP/Mail
// Package: Mail
// Module:  MailDeliveryServiceImpl
//
// Copyright (c) 2013, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties,
// copied or duplicated in any form, in whole or in part.
//


#include "Poco/OSP/Mail/MailDeliveryServiceImpl.h"
#include "Poco/Net/MailMessage.h"
#include "Poco/Net/MailRecipient.h"
#include "Poco/Net/SMTPClientSession.h"
#include "Poco/Net/SocketAddress.h"
#include "Poco/Net/StreamSocket.h"
#include "Poco/Util/AbstractConfiguration.h"
#include "Poco/Bugcheck.h"
#include "Poco/Exception.h"
#include "Poco/Format.h"
#include "Poco/Mutex.h"
#include "Poco/Notification.h"
#include "Poco/String.h"
#include "Poco/Types.h"
#include "Poco/UUIDGenerator.h"
#include <exception>
#include <memory>
#include <sstream>


namespace Poco {
namespace OSP {
namespace Mail {


MailDeliveryServiceImpl::MailDeliveryServiceImpl(const Configuration& config, Poco::Logger& logger):
	_config(config),
	_logger(logger)
{
}


MailDeliveryServiceImpl::~MailDeliveryServiceImpl()
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


void MailDeliveryServiceImpl::start()
{
	_thread.start(*this);
}


void MailDeliveryServiceImpl::stop()
{
	{
		Poco::FastMutex::ScopedLock lock(_mutex);
		if (_stopped) return;
		_stopped = true;
	}
	if (_thread.isRunning())
	{
		_shutdown.set();
		_queue.enqueueUrgentNotification(new MailDeliveryHandle(0));
		_thread.join();
		closeSMTPClientSession();
	}
	const Poco::IllegalStateException exc("The mail delivery service has stopped");
	for (Poco::Notification::Ptr pNf = _queue.dequeueNotification(); pNf != nullptr; pNf = _queue.dequeueNotification())
	{
		// A handle without a message tells the delivery thread to quit.
		MailDeliveryHandle::Ptr pHandle = pNf.cast<MailDeliveryHandle>();
		if (failUnfinished(pHandle, exc))
		{
			_logger.error("The message \"%s\" is not delivered: the mail delivery service has stopped.", pHandle->message()->getSubject());
		}
	}
}


MailDeliveryHandle::Ptr MailDeliveryServiceImpl::sendMessage(MailMessagePtr pMessage)
{
	poco_check_ptr (pMessage);

	Poco::FastMutex::ScopedLock lock(_mutex);

	if (_stopped) throw Poco::IllegalStateException("The mail delivery service has stopped");
	if (_queue.size() < _config.maxQueued)
	{
		MailDeliveryHandle::Ptr pHandle = new MailDeliveryHandle(pMessage);
		_queue.enqueueNotification(pHandle);
		return pHandle;
	}
	else throw Poco::RuntimeException("Too many queued mail messages.");
}


MailDeliveryServiceImpl::Configuration MailDeliveryServiceImpl::loadConfiguration(const Poco::Util::AbstractConfiguration& settings, const Poco::Util::AbstractConfiguration& defaults, Poco::Logger& logger)
{
	const auto getString = [&settings, &defaults](const std::string& key)
	{
		return settings.getString(key, defaults.getString(key));
	};
	const auto getInt = [&settings, &defaults](const std::string& key)
	{
		return settings.getInt(key, defaults.getInt(key));
	};

	Configuration config;
	config.host = getString("osp.mail.host");
	config.port = static_cast<Poco::UInt16>(getInt("osp.mail.port"));
	config.clientHost = settings.getString("osp.mail.clientHost", defaults.getString("osp.mail.clientHost", ""));
	config.sender = settings.getString("osp.mail.sender", "");
	config.generateMID = settings.getBool("osp.mail.generateMID", false);
	config.socketTimeout = Poco::Timespan::SECONDS*getInt("osp.mail.socketTimeout");
	config.sessionTimeout = Poco::Timespan::SECONDS*getInt("osp.mail.sessionTimeout");
	const std::string loginMethod = Poco::toLower(getString("osp.mail.login.method"));
	if (loginMethod == "cram-md5")
		config.loginMethod = Poco::Net::SMTPClientSession::AUTH_CRAM_MD5;
	else if (loginMethod == "cram-sha1")
		config.loginMethod = Poco::Net::SMTPClientSession::AUTH_CRAM_SHA1;
	else if (loginMethod == "login")
		config.loginMethod = Poco::Net::SMTPClientSession::AUTH_LOGIN;
	else if (loginMethod == "plain")
		config.loginMethod = Poco::Net::SMTPClientSession::AUTH_PLAIN;
	else if (loginMethod == "xoauth2")
		config.loginMethod = Poco::Net::SMTPClientSession::AUTH_XOAUTH2;
	else if (loginMethod == "ntlm")
		config.loginMethod = Poco::Net::SMTPClientSession::AUTH_NTLM;
	else if (loginMethod != "none")
		logger.warning("Invalid login method specified (%s). Defaulting to none.", loginMethod);
	config.username = settings.getString("osp.mail.login.username", "");
	config.password = settings.getString("osp.mail.login.password", "");
	config.maxQueued = getInt("osp.mail.limits.maxQueued");
	config.maxFailedLogins = getInt("osp.mail.limits.maxFailedLogins");
	return config;
}


std::string MailDeliveryServiceImpl::generateMID(const std::string& clientHost) const
{
	std::string mid("<");
	Poco::UUIDGenerator uuidGenerator;
	mid += uuidGenerator.createRandom().toString();
	mid += '@';
	mid += clientHost;
	mid += '>';
	return mid;
}


const std::type_info& MailDeliveryServiceImpl::type() const
{
	return typeid(MailDeliveryServiceImpl);
}


bool MailDeliveryServiceImpl::isA(const std::type_info& otherType) const
{
	std::string name(typeid(MailDeliveryServiceImpl).name());
	return name == otherType.name() || MailDeliveryService::isA(otherType);
}


MailDeliveryServiceImpl::SMTPClientSessionPtr MailDeliveryServiceImpl::createSMTPClientSession(const Configuration& config)
{
	_logger.debug(Poco::format("Connecting to SMTP server %s, port %hu...", config.host, config.port));
	Poco::Net::StreamSocket socket;
	socket.connect(Poco::Net::SocketAddress(config.host, config.port), config.socketTimeout);
	socket.setSendTimeout(config.socketTimeout);
	SMTPClientSessionPtr pSMTPClientSession = new Poco::Net::SMTPClientSession(socket);
	pSMTPClientSession->setTimeout(config.socketTimeout);
	_logger.debug("Logging in...");
	if (config.clientHost.empty())
		pSMTPClientSession->login(config.loginMethod, config.username, config.password);
	else
		pSMTPClientSession->login(config.clientHost, config.loginMethod, config.username, config.password);
	_logger.debug("Successfully logged in.");
	return pSMTPClientSession;
}


void MailDeliveryServiceImpl::run()
{
	long sessionTimeout = static_cast<long>(_config.sessionTimeout.totalMilliseconds());
	int failedLogins = 0;
	bool loginFailing = false;
	for (;;)
	{
		MailDeliveryHandle::Ptr pHandle;
		std::unique_ptr<Poco::Exception> pError;
		try
		{
			Poco::Notification::Ptr pNf = _queue.waitDequeueNotification(sessionTimeout);
			if (pNf)
			{
				pHandle = pNf.cast<MailDeliveryHandle>();
				if (pHandle)
				{
					if (pHandle->message())
					{
						if (!_pSMTPClientSession)
						{
							try
							{
								_pSMTPClientSession = createSMTPClientSession(_config);
								failedLogins = 0;
								if (loginFailing)
								{
									loginFailing = false;
									_logger.information("Logged in to the SMTP server %s, port %hu, after failed logins.", _config.host, _config.port);
								}
							}
							catch (Poco::Exception& exc)
							{
								if (loginFailing)
									_logger.debug("Failed to log in to the SMTP server %s, port %hu: %s", _config.host, _config.port, exc.displayText());
								else
									_logger.error("Failed to log in to the SMTP server %s, port %hu: %s. Further failed logins are logged at debug level until a login succeeds.", _config.host, _config.port, exc.displayText());
								loginFailing = true;
								failedLogins++;
								if (failedLogins < _config.maxFailedLogins)
								{
									// retry later
									_queue.enqueueNotification(pHandle);
								}
								else
								{
									// give up; the next message gets maxFailedLogins attempts again
									_logger.error("The message \"%s\" is not delivered after %d failed logins.", pHandle->message()->getSubject(), failedLogins);
									pHandle->updateStatus(exc);
									failedLogins = 0;
								}
								const long sleepTime = static_cast<long>(_config.loginRetryDelay.totalMilliseconds());
								_logger.debug("Waiting %ld milliseconds before the next login.", sleepTime);
								if (_shutdown.tryWait(sleepTime)) return; // told to quit
							}
						}
						if (_pSMTPClientSession)
						{
							try
							{
								_logger.debug("Delivering the message \"%s\" from %s.", pHandle->message()->getSubject(), pHandle->message()->getSender());
								pHandle->updateStatus(MailDeliveryHandle::MAIL_DELIVERING);
								deliver(*pHandle->message());
								pHandle->updateStatus(MailDeliveryHandle::MAIL_DELIVERED);
							}
							catch (Poco::Exception& exc)
							{
								pHandle->updateStatus(exc);
								_logger.error("The message \"%s\" is not delivered: %s", pHandle->message()->getSubject(), exc.displayText());
								// The server may have closed the connection, or the SMTP
								// dialogue is in an unknown state: the next message gets a
								// new session.
								closeSMTPClientSession();
							}
						}
					}
					else return; // told to quit
				}
			}
			else
			{
				closeSMTPClientSession();
			}
		}
		catch (Poco::Exception& exc)
		{
			pError.reset(exc.clone());
		}
		catch (std::exception& exc)
		{
			pError = std::make_unique<Poco::Exception>(exc.what());
		}
		catch (...)
		{
			pError = std::make_unique<Poco::Exception>("Unknown exception");
		}
		if (pError != nullptr)
		{
			if (failUnfinished(pHandle, *pError))
				_logger.error("The message \"%s\" is not delivered: %s. The mail delivery service closes the SMTP session and continues with the next message.", pHandle->message()->getSubject(), pError->displayText());
			else
				_logger.error("The mail delivery service caught an error: %s. It closes the SMTP session and continues with the next message.", pError->displayText());
			closeSMTPClientSession();
		}
	}
}


bool MailDeliveryServiceImpl::failUnfinished(MailDeliveryHandle::Ptr pHandle, const Poco::Exception& exc)
{
	if (pHandle != nullptr && pHandle->message() != nullptr)
	{
		const MailDeliveryHandle::MailStatus status = pHandle->status();
		if (status == MailDeliveryHandle::MAIL_QUEUED || status == MailDeliveryHandle::MAIL_DELIVERING)
		{
			pHandle->updateStatus(exc);
			return true;
		}
	}
	return false;
}


void MailDeliveryServiceImpl::deliver(const Poco::Net::MailMessage& message)
{
	const bool addSender = message.getSender().empty();
	const bool addMID = _config.generateMID && !_config.clientHost.empty() && !message.has("Message-ID");
	if (!addSender && !addMID)
	{
		_pSMTPClientSession->sendMessage(message);
		return;
	}

	std::ostringstream ostr;
	if (addSender) ostr << "From: " << _config.sender << "\r\n";
	if (addMID) ostr << "Message-ID: " << generateMID(_config.clientHost) << "\r\n";
	message.write(ostr);

	Poco::Net::SMTPClientSession::Recipients recipients;
	for (const auto& recipient: message.recipients())
	{
		recipients.push_back(recipient.getAddress());
	}
	_pSMTPClientSession->sendAddresses(addSender ? _config.sender : message.getSender(), recipients);
	_pSMTPClientSession->sendData();
	std::istringstream istr(ostr.str());
	_pSMTPClientSession->sendMessage(istr);
}


void MailDeliveryServiceImpl::closeSMTPClientSession()
{
	if (_pSMTPClientSession)
	{
		try
		{
			_logger.debug("Closing SMTP session.");
			_pSMTPClientSession->close();
		}
		catch (Poco::Exception&)
		{
		}
		_pSMTPClientSession = 0;
	}
}


} } } // namespace Poco::OSP::Mail
