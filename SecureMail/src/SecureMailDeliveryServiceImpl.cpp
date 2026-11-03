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
#include "Poco/Net/NetException.h"
#include "Poco/Net/SecureSMTPClientSession.h"
#include "Poco/Net/SecureStreamSocket.h"
#include "Poco/Net/SSLManager.h"
#include "Poco/Net/SocketAddress.h"
#include "Poco/Net/StreamSocket.h"
#include "Poco/Util/AbstractConfiguration.h"
#include "Poco/Bugcheck.h"
#include "Poco/Format.h"
#include "Poco/SharedPtr.h"
#include <string>


namespace Poco::OSP::Mail {


namespace
{
	class ConnectedSMTPClientSession: public Poco::Net::SecureSMTPClientSession
		/// A SecureSMTPClientSession on a connected socket that checks the server
		/// certificate against the given host name after STARTTLS. The socket
		/// constructor of SecureSMTPClientSession has no host name for that check.
	{
	public:
		ConnectedSMTPClientSession() = delete;

		ConnectedSMTPClientSession(const Poco::Net::StreamSocket& socket, const std::string& host):
			Poco::Net::SecureSMTPClientSession(socket),
			_host(host)
		{
		}

		~ConnectedSMTPClientSession() override = default;

		ConnectedSMTPClientSession(const ConnectedSMTPClientSession&) = delete;
		ConnectedSMTPClientSession(ConnectedSMTPClientSession&&) = delete;
		ConnectedSMTPClientSession& operator = (const ConnectedSMTPClientSession&) = delete;
		ConnectedSMTPClientSession& operator = (ConnectedSMTPClientSession&&) = delete;

		[[nodiscard]] bool startTLS(Poco::Net::Context::Ptr pContext)
			/// Returns false if the server does not accept STARTTLS.
		{
			std::string response;
			const int status = sendCommand("STARTTLS", response);
			if (!isPositiveCompletion(status)) return false;

			socket() = Poco::Net::SecureStreamSocket::attach(socket(), _host, pContext);
			return true;
		}

	private:
		std::string _host;
	};
}


SecureMailDeliveryServiceImpl::SecureMailDeliveryServiceImpl(Mode mode, const MailDeliveryServiceImpl::Configuration& config, Poco::Logger& logger, Poco::Net::Context::Ptr pContext):
	MailDeliveryServiceImpl(config, logger),
	_mode(mode),
	_pContext(pContext),
	_logger(logger)
{
}


SecureMailDeliveryServiceImpl::~SecureMailDeliveryServiceImpl()
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


Poco::Net::Context::Ptr SecureMailDeliveryServiceImpl::createContext(const Poco::Util::AbstractConfiguration& settings)
{
	Poco::Net::Context::Params params;
	params.privateKeyFile = settings.getString("osp.mail.tls.privateKey", "");
	params.certificateFile = settings.getString("osp.mail.tls.certificate", "");
	params.caLocation = settings.getString("osp.mail.tls.caLocation", "");
	params.verificationMode = Poco::Net::Context::VERIFY_RELAXED;
	params.loadDefaultCAs = true;
	params.cipherList = settings.getString("osp.mail.tls.ciphers", params.cipherList);
	Poco::Net::Context::Ptr pContext = new Poco::Net::Context(Poco::Net::Context::TLS_CLIENT_USE, params);
	pContext->requireMinimumProtocol(Poco::Net::Context::PROTO_TLSV1_2);
	pContext->enableExtendedCertificateVerification(settings.getBool("osp.mail.tls.extendedCertificateVerification", true));
	return pContext;
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
	Poco::Net::StreamSocket socket;
	socket.connect(Poco::Net::SocketAddress(config.host, config.port), config.socketTimeout);
	socket.setSendTimeout(config.socketTimeout);
	Poco::SharedPtr<ConnectedSMTPClientSession> pSecureSMTPClientSession = new ConnectedSMTPClientSession(socket, config.host);
	pSecureSMTPClientSession->setTimeout(config.socketTimeout);

	_logger.debug("Starting TLS...");
	if (config.clientHost.empty())
		pSecureSMTPClientSession->login();
	else
		pSecureSMTPClientSession->login(config.clientHost);

	Poco::Net::Context::Ptr pContext = _pContext;
	if (pContext == nullptr) pContext = Poco::Net::SSLManager::instance().defaultClientContext();
	if (!pSecureSMTPClientSession->startTLS(pContext)) throw Poco::Net::SMTPException("The SMTP server does not support STARTTLS", config.host);

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
	if (pContext == nullptr) pContext = Poco::Net::SSLManager::instance().defaultClientContext();
	// The timeout of connect() also applies to the TLS handshake.
	Poco::Net::SecureStreamSocket sss(config.host, pContext);
	sss.connect(Poco::Net::SocketAddress(config.host, config.port), config.socketTimeout);
	sss.setSendTimeout(config.socketTimeout);

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


} // namespace Poco::OSP::Mail
