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


namespace Poco {
namespace OSP {
namespace Mail {


class SecureMailDeliveryServiceImpl: public MailDeliveryServiceImpl
{
public:
	using Ptr = Poco::AutoPtr<SecureMailDeliveryServiceImpl>;

	enum Mode
	{
		SMTP_STARTTLS = 0,
		SMTP_OVER_TLS = 1
	};

	SecureMailDeliveryServiceImpl(Mode mode, const MailDeliveryServiceImpl::Configuration& config, Poco::Logger& logger, Poco::Net::Context::Ptr pContext = 0);
	~SecureMailDeliveryServiceImpl();

	// Service
	const std::type_info& type() const;
	bool isA(const std::type_info& otherType) const;

protected:
	// MailDeliveryServiceImpl
	SMTPClientSessionPtr createSMTPClientSession(const MailDeliveryServiceImpl::Configuration& config);
	SMTPClientSessionPtr createSMTPClientSessionStartTLS(const MailDeliveryServiceImpl::Configuration& config);
	SMTPClientSessionPtr createSMTPClientSessionOverTLS(const MailDeliveryServiceImpl::Configuration& config);


private:
	Mode _mode;
	Poco::Net::Context::Ptr _pContext;
	Poco::Logger& _logger;
};


} } } // namespace Poco::OSP::Mail


#endif // OSP_Mail_SecureMailDeliveryServiceImpl_INCLUDED
