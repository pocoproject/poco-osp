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
#include "Poco/Format.h"
#include "Poco/UUIDGenerator.h"


namespace Poco {
namespace OSP {
namespace Mail {


MailDeliveryServiceImpl::MailDeliveryServiceImpl(const Configuration& config, Poco::Logger& logger):
	_config(config),
	_logger(logger)
{
	_thread.start(*this);
}


MailDeliveryServiceImpl::~MailDeliveryServiceImpl()
{
	try
	{
		_shutdown.set();
		_queue.enqueueUrgentNotification(new MailDeliveryHandle(0));
		_thread.join();
		closeSMTPClientSession();
	}
	catch (...)
	{
		poco_unexpected();
	}
}


MailDeliveryHandle::Ptr MailDeliveryServiceImpl::sendMessage(MailMessagePtr pMessage)
{
	poco_check_ptr (pMessage);

	Poco::FastMutex::ScopedLock lock(_mutex);

	if (_queue.size() < _config.maxQueued)
	{
		if (pMessage->getSender().empty())
		{
			pMessage->setSender(_config.sender);
		}
		if (_config.generateMID && !_config.clientHost.empty() && !pMessage->has("Message-ID"))
		{
			pMessage->set("Message-ID", generateMID(_config.clientHost));
		}
		MailDeliveryHandle::Ptr pHandle = new MailDeliveryHandle(pMessage);
		_queue.enqueueNotification(pHandle);
		return pHandle;
	}
	else throw Poco::RuntimeException("Too many queued mail messages.");
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
	SMTPClientSessionPtr pSMTPClientSession = new Poco::Net::SMTPClientSession(config.host, config.port);
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
	for (;;)
	{
		try
		{
			Poco::Notification::Ptr pNf = _queue.waitDequeueNotification(sessionTimeout);
			if (pNf)
			{
				MailDeliveryHandle::Ptr pHandle = pNf.cast<MailDeliveryHandle>();
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
							}
							catch (Poco::Exception& exc)
							{
								_logger.log(exc);
								failedLogins++;
								if (failedLogins < _config.maxFailedLogins)
								{
									// retry later
									_queue.enqueueNotification(pHandle);
								}
								else
								{
									// give up
									_logger.notice("Too many failed login attempts, discarding message.");
									pHandle->updateStatus(exc);
								}
								long sleepTime = 10000;
								if (failedLogins > _config.maxFailedLogins)
									sleepTime += 10000*((failedLogins - _config.maxFailedLogins)/10);
								if (sleepTime > 120000) sleepTime = 120000;
								_logger.information(Poco::format("Backing off for %ld milliseconds due to repeated failures.", sleepTime));
								if (_shutdown.tryWait(sleepTime)) return; // told to quit
							}
						}
						if (_pSMTPClientSession)
						{
							try
							{
								_logger.information("Delivering message from " + pHandle->message()->getSender());
								pHandle->updateStatus(MailDeliveryHandle::MAIL_DELIVERING);
								_pSMTPClientSession->sendMessage(*pHandle->message());
								pHandle->updateStatus(MailDeliveryHandle::MAIL_DELIVERED);
							}
							catch (Poco::Exception& exc)
							{
								pHandle->updateStatus(exc);
								_logger.log(exc);
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
			_logger.log(exc);
		}
	}
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
