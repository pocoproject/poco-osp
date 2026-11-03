//
// SecureMailBundleActivator.cpp
//
// $Id: //poco/1.11/OSP/SecureMail/src/SecureMailBundleActivator.cpp#1 $
//
// Copyright (c) 2017, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties,
// copied or duplicated in any form, in whole or in part.
//


#include "SecureMailDeliveryServiceImpl.h"
#include "Poco/OSP/Mail/MailDeliveryService.h"
#include "Poco/OSP/Mail/MailDeliveryServiceImpl.h"
#include "Poco/OSP/BundleActivator.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/Bundle.h"
#include "Poco/OSP/ServiceRegistry.h"
#include "Poco/OSP/ServiceRef.h"
#include "Poco/OSP/ServiceFinder.h"
#include "Poco/OSP/PreferencesService.h"
#include "Poco/Net/Context.h"
#include "Poco/String.h"
#include "Poco/ClassLibrary.h"


using Poco::OSP::BundleContext;
using Poco::OSP::ServiceRegistry;
using Poco::OSP::ServiceRef;
using Poco::OSP::Properties;
using Poco::OSP::ServiceFinder;
using Poco::OSP::PreferencesService;


namespace Poco::OSP::Mail {


class SecureBundleActivator: public Poco::OSP::BundleActivator
{
public:
	SecureBundleActivator() = default;

	~SecureBundleActivator() override = default;

	void start(BundleContext::Ptr pBundleContext) override
	{
		_pBundleContext = pBundleContext;
		_pPrefs = ServiceFinder::find<PreferencesService>(pBundleContext);

		bool enabled = _pPrefs->configuration()->getBool("osp.mail.enable", false);
		bool secure  = _pPrefs->configuration()->getBool("osp.mail.secure", false);
		if (enabled && secure)
		{
			const MailDeliveryServiceImpl::Configuration config = MailDeliveryServiceImpl::loadConfiguration(*_pPrefs->configuration(), pBundleContext->thisBundle()->properties(), pBundleContext->logger());

			std::string modeString = Poco::toLower(getStringConfig("osp.mail.tls.mode", "starttls"));
			SecureMailDeliveryServiceImpl::Mode mode = SecureMailDeliveryServiceImpl::SMTP_STARTTLS;
			if (modeString == "smtpovertls")
				mode = SecureMailDeliveryServiceImpl::SMTP_OVER_TLS;
			else if (modeString == "starttls")
				mode = SecureMailDeliveryServiceImpl::SMTP_STARTTLS;
			else
				_pBundleContext->logger().warning("Invalid TLS mode specified (%s). Defaulting to StartTLS.", modeString);

			Poco::Net::Context::Ptr pTLSContext = SecureMailDeliveryServiceImpl::createContext(*_pPrefs->configuration());
			pBundleContext->logger().debug("TLS settings: mode %s, CA location \"%s\", certificate \"%s\", host name verification %b.",
				modeString,
				_pPrefs->configuration()->getString("osp.mail.tls.caLocation", ""),
				_pPrefs->configuration()->getString("osp.mail.tls.certificate", ""),
				pTLSContext->extendedCertificateVerificationEnabled());

			_pMailDeliveryService = new SecureMailDeliveryServiceImpl(mode, config, pBundleContext->logger(), pTLSContext);
			_pMailDeliveryService->start();
			_pMailDeliveryServiceRef = pBundleContext->registry().registerService(MailDeliveryService::SERVICE_NAME, _pMailDeliveryService, Properties());
		}
		else
		{
			pBundleContext->logger().notice("Secure mail delivery service not enabled.");
		}
	}

	void stop(BundleContext::Ptr pBundleContext) override
	{
		if (_pMailDeliveryService != nullptr) _pMailDeliveryService->stop();
		if (_pMailDeliveryServiceRef != nullptr) pBundleContext->registry().unregisterService(_pMailDeliveryServiceRef);
		_pMailDeliveryServiceRef = nullptr;
		_pMailDeliveryService = nullptr;
		_pPrefs = nullptr;
		_pBundleContext = nullptr;
	}

protected:
	[[nodiscard]] std::string getStringConfig(const std::string& key, const std::string& deflt)
	{
		return _pPrefs->configuration()->getString(key, _pBundleContext->thisBundle()->properties().getString(key, deflt));
	}

private:
	BundleContext::Ptr _pBundleContext;
	PreferencesService::Ptr _pPrefs;
	SecureMailDeliveryServiceImpl::Ptr _pMailDeliveryService;
	ServiceRef::Ptr _pMailDeliveryServiceRef;
};


} // namespace Poco::OSP::Mail


POCO_BEGIN_MANIFEST(Poco::OSP::BundleActivator)
	POCO_EXPORT_CLASS(Poco::OSP::Mail::SecureBundleActivator)
POCO_END_MANIFEST
