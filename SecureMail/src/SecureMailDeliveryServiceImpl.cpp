//
// SecureMailDeliveryServiceImpl.cpp
//
// $Id: //poco/1.11/OSP/SecureMail/src/SecureMailDeliveryServiceImpl.cpp#1 $
//
// Library: OSP/SecureMail
// Package: SecureMail
// Module:  SecureMailDeliveryServiceImpl
//
// Copyright (c) 2017, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties,
// copied or duplicated in any form, in whole or in part.
//


#include "SecureMailDeliveryServiceImpl.h"
#include "Poco/Net/SecureSMTPClientSession.h"
#include "Poco/Net/SecureStreamSocket.h"
#include "Poco/Net/SSLManager.h"
#include "Poco/Format.h"


namespace Poco {
namespace OSP {
namespace Mail {


SecureMailDeliveryServiceImpl::SecureMailDeliveryServiceImpl(Mode mode, const MailDeliveryServiceImpl::Configuration& config, Poco::Logger& logger, Poco::Net::Context::Ptr pContext):
	MailDeliveryServiceImpl(config, logger),
	_mode(mode),
	_pContext(pContext),
	_logger(logger)
{
}


SecureMailDeliveryServiceImpl::~SecureMailDeliveryServiceImpl()
{
}


const std::type_info& SecureMailDeliveryServiceImpl::type() const
{
	return typeid(SecureMailDeliveryServiceImpl);
}


bool SecureMailDeliveryServiceImpl::isA(const std::type_info& otherType) const
{
	std::string name(typeid(SecureMailDeliveryServiceImpl).name());
	return name == otherType.name() || MailDeliveryServiceImpl::isA(otherType);
}


MailDeliveryServiceImpl::SMTPClientSessionPtr SecureMailDeliveryServiceImpl::createSMTPClientSession(const MailDeliveryServiceImpl::Configuration& config)
{
	switch (_mode)
	{
	case SMTP_STARTTLS:
		return createSMTPClientSessionStartTLS(config);

	case SMTP_OVER_TLS:
		return createSMTPClientSessionOverTLS(config);

	default:
		poco_bugcheck();
		return MailDeliveryServiceImpl::SMTPClientSessionPtr();
	}
}


MailDeliveryServiceImpl::SMTPClientSessionPtr SecureMailDeliveryServiceImpl::createSMTPClientSessionStartTLS(const MailDeliveryServiceImpl::Configuration& config)
{
	_logger.debug(Poco::format("Connecting to SMTP server %s, port %hu...", config.host, config.port));
	Poco::SharedPtr<Poco::Net::SecureSMTPClientSession> pSecureSMTPClientSession = new Poco::Net::SecureSMTPClientSession(config.host, config.port);
	pSecureSMTPClientSession->setTimeout(config.socketTimeout);

	_logger.debug("Starting TLS...");
	if (config.clientHost.empty())
		pSecureSMTPClientSession->login();
	else
		pSecureSMTPClientSession->login(config.clientHost);

	if (_pContext)
		pSecureSMTPClientSession->startTLS(_pContext);
	else
		pSecureSMTPClientSession->startTLS();

	_logger.debug("Logging in...");
	if (config.clientHost.empty())
		pSecureSMTPClientSession->login(config.loginMethod, config.username, config.password);
	else
		pSecureSMTPClientSession->login(config.clientHost, config.loginMethod, config.username, config.password);
	_logger.debug("Successfully logged in.");

	return pSecureSMTPClientSession;
}


MailDeliveryServiceImpl::SMTPClientSessionPtr SecureMailDeliveryServiceImpl::createSMTPClientSessionOverTLS(const MailDeliveryServiceImpl::Configuration& config)
{
	_logger.debug(Poco::format("Connecting to SMTP server %s, port %hu over TLS...", config.host, config.port));

	Poco::Net::Context::Ptr pContext = _pContext;
	if (!pContext) pContext = Poco::Net::SSLManager::instance().defaultClientContext();
	Poco::Net::SocketAddress addr(config.host, config.port);
	Poco::Net::SecureStreamSocket sss(addr, config.host, pContext);

	Poco::SharedPtr<Poco::Net::SecureSMTPClientSession> pSecureSMTPClientSession = new Poco::Net::SecureSMTPClientSession(sss);
	pSecureSMTPClientSession->setTimeout(config.socketTimeout);

	_logger.debug("Logging in...");
	if (config.clientHost.empty())
		pSecureSMTPClientSession->login(config.loginMethod, config.username, config.password);
	else
		pSecureSMTPClientSession->login(config.clientHost, config.loginMethod, config.username, config.password);
	_logger.debug("Successfully logged in.");

	return pSecureSMTPClientSession;
}


} } } // namespace Poco::OSP::Mail
