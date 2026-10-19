//
// MailBundleActivator.cpp
//
// Copyright (c) 2013, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties,
// copied or duplicated in any form, in whole or in part.
//


#include "Poco/OSP/Mail/MailDeliveryServiceImpl.h"
#include "Poco/OSP/BundleActivator.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/Bundle.h"
#include "Poco/OSP/ServiceRegistry.h"
#include "Poco/OSP/ServiceRef.h"
#include "Poco/OSP/ServiceFinder.h"
#include "Poco/OSP/PreferencesService.h"
#include "Poco/String.h"
#include "Poco/ClassLibrary.h"


using Poco::OSP::BundleContext;
using Poco::OSP::ServiceRegistry;
using Poco::OSP::ServiceRef;
using Poco::OSP::Properties;
using Poco::OSP::ServiceFinder;
using Poco::OSP::PreferencesService;


namespace Poco {
namespace OSP {
namespace Mail {


class BundleActivator: public Poco::OSP::BundleActivator
{
public:
	BundleActivator()
	{
	}

	~BundleActivator()
	{
	}

	void start(BundleContext::Ptr pContext)
	{
		_pContext = pContext;
		_pPrefs = ServiceFinder::find<PreferencesService>(pContext);

		bool enabled = _pPrefs->configuration()->getBool("osp.mail.enable", false);
		bool secure = _pPrefs->configuration()->getBool("osp.mail.secure", false);
		if (enabled && !secure)
		{
			MailDeliveryServiceImpl::Configuration config;
			config.host = getStringConfig("osp.mail.host");
			config.port = static_cast<Poco::UInt16>(getIntConfig("osp.mail.port"));
			config.clientHost = getStringConfig("osp.mail.clientHost", "");
			config.sender = _pPrefs->configuration()->getString("osp.mail.sender", "");
			config.generateMID = _pPrefs->configuration()->getBool("osp.mail.generateMID", false);
			config.socketTimeout = Poco::Timespan::SECONDS*getIntConfig("osp.mail.socketTimeout");
			config.sessionTimeout = Poco::Timespan::SECONDS*getIntConfig("osp.mail.sessionTimeout");
			std::string loginMethod = Poco::toLower(getStringConfig("osp.mail.login.method"));
			config.loginMethod = Poco::Net::SMTPClientSession::AUTH_NONE;
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
				pContext->logger().warning("Invalid login method specified. Defaulting to none.");
			config.username = _pPrefs->configuration()->getString("osp.mail.login.username", "");
			config.password = _pPrefs->configuration()->getString("osp.mail.login.password", "");
			config.maxQueued = getIntConfig("osp.mail.limits.maxQueued");
			config.maxFailedLogins = getIntConfig("osp.mail.limits.maxFailedLogins");

			MailDeliveryServiceImpl::Ptr pMailDeliveryService = new MailDeliveryServiceImpl(config, pContext->logger());
			_pMailDeliveryServiceRef = pContext->registry().registerService(MailDeliveryService::SERVICE_NAME, pMailDeliveryService, Properties());
		}
		else
		{
			pContext->logger().notice("Mail delivery service not enabled.");
		}
	}

	void stop(BundleContext::Ptr pContext)
	{
		if (_pMailDeliveryServiceRef) pContext->registry().unregisterService(_pMailDeliveryServiceRef);
		_pMailDeliveryServiceRef = 0;
		_pPrefs = 0;
		_pContext = 0;
	}

protected:
	int getIntConfig(const std::string& key)
	{
		return _pPrefs->configuration()->getInt(key, _pContext->thisBundle()->properties().getInt(key));
	}

	std::string getStringConfig(const std::string& key)
	{
		return _pPrefs->configuration()->getString(key, _pContext->thisBundle()->properties().getString(key));
	}

	std::string getStringConfig(const std::string& key, const std::string& deflt)
	{
		return _pPrefs->configuration()->getString(key, _pContext->thisBundle()->properties().getString(key, deflt));
	}

private:
	BundleContext::Ptr _pContext;
	PreferencesService::Ptr _pPrefs;
	ServiceRef::Ptr _pMailDeliveryServiceRef;
};


} } } // namespace Poco::OSP::Mail


POCO_BEGIN_MANIFEST(Poco::OSP::BundleActivator)
	POCO_EXPORT_CLASS(Poco::OSP::Mail::BundleActivator)
POCO_END_MANIFEST
