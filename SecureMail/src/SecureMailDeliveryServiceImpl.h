//
// SecureMailDeliveryServiceImpl.h
//
// $Id: //poco/1.11/OSP/SecureMail/src/SecureMailDeliveryServiceImpl.h#1 $
//
// Library: OSP/SecureMail
// Package: SecureMail
// Module:  SecureMailDeliveryServiceImpl
//
// Definition of the SecureMailDeliveryServiceImpl class.
//
// Copyright (c) 2017, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties,
// copied or duplicated in any form, in whole or in part.
//


#ifndef OSP_Mail_SecureMailDeliveryServiceImpl_INCLUDED
#define OSP_Mail_SecureMailDeliveryServiceImpl_INCLUDED


#include "Poco/OSP/Mail/MailDeliveryServiceImpl.h"
#include "Poco/Net/Context.h"
#include "Poco/Util/AbstractConfiguration.h"


namespace Poco::OSP::Mail {


class SecureMailDeliveryServiceImpl: public MailDeliveryServiceImpl
{
public:
	using Ptr = Poco::AutoPtr<SecureMailDeliveryServiceImpl>;

	enum Mode
	{
		SMTP_STARTTLS = 0,
		SMTP_OVER_TLS = 1
	};

	SecureMailDeliveryServiceImpl() = delete;

	SecureMailDeliveryServiceImpl(Mode mode, const MailDeliveryServiceImpl::Configuration& config, Poco::Logger& logger,
		Poco::Net::Context::Ptr pContext = nullptr);
	~SecureMailDeliveryServiceImpl() override;

	SecureMailDeliveryServiceImpl(const SecureMailDeliveryServiceImpl&) = delete;
	SecureMailDeliveryServiceImpl(SecureMailDeliveryServiceImpl&&) = delete;
	SecureMailDeliveryServiceImpl& operator = (const SecureMailDeliveryServiceImpl&) = delete;
	SecureMailDeliveryServiceImpl& operator = (SecureMailDeliveryServiceImpl&&) = delete;

	[[nodiscard]] static Poco::Net::Context::Ptr createContext(const Poco::Util::AbstractConfiguration& settings);
		/// Creates the TLS client context from the osp.mail.tls.* settings.
		///
		/// The context requires TLS 1.2 or later, trusts the certificates in caLocation
		/// and the default CA certificates, presents the client certificate and private key
		/// if they are given, uses the cipher list if it is given, and checks that the
		/// server certificate names the mail server unless extendedCertificateVerification
		/// is false.

	// Service
	[[nodiscard]] const std::type_info& type() const override;
	[[nodiscard]] bool isA(const std::type_info& otherType) const override;

protected:
	// MailDeliveryServiceImpl
	[[nodiscard]] SMTPClientSessionPtr createSMTPClientSession(
		const MailDeliveryServiceImpl::Configuration& config) override;
	[[nodiscard]] SMTPClientSessionPtr createSMTPClientSessionStartTLS(
		const MailDeliveryServiceImpl::Configuration& config);
	[[nodiscard]] SMTPClientSessionPtr createSMTPClientSessionOverTLS(
		const MailDeliveryServiceImpl::Configuration& config);


private:
	Mode _mode;
	Poco::Net::Context::Ptr _pContext;
	Poco::Logger& _logger;
};


} // namespace Poco::OSP::Mail


#endif // OSP_Mail_SecureMailDeliveryServiceImpl_INCLUDED
