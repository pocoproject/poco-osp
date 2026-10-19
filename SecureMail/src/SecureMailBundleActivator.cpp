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


namespace Poco {
namespace OSP {
namespace Mail {


class SecureBundleActivator: public Poco::OSP::BundleActivator
{
public:
	SecureBundleActivator()
	{
	}

	~SecureBundleActivator()
	{
	}

	void start(BundleContext::Ptr pBundleContext)
	{
		_pBundleContext = pBundleContext;
		_pPrefs = ServiceFinder::find<PreferencesService>(pBundleContext);

		bool enabled = _pPrefs->configuration()->getBool("osp.mail.enable", false);
		bool secure  = _pPrefs->configuration()->getBool("osp.mail.secure", false);
		if (enabled && secure)
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
				pBundleContext->logger().warning("Invalid login method specified (%s). Defaulting to none.", loginMethod);

			config.username = _pPrefs->configuration()->getString("osp.mail.login.username", "");
			config.password = _pPrefs->configuration()->getString("osp.mail.login.password", "");
			config.maxQueued = getIntConfig("osp.mail.limits.maxQueued");
			config.maxFailedLogins = getIntConfig("osp.mail.limits.maxFailedLogins");

			std::string modeString = Poco::toLower(getStringConfig("osp.mail.tls.mode", "starttls"));
			SecureMailDeliveryServiceImpl::Mode mode = SecureMailDeliveryServiceImpl::SMTP_STARTTLS;
			if (modeString == "smtpovertls")
				mode = SecureMailDeliveryServiceImpl::SMTP_OVER_TLS;
			else if (modeString == "starttls")
				mode = SecureMailDeliveryServiceImpl::SMTP_STARTTLS;
			else
				_pBundleContext->logger().warning("Invalid TLS mode specified (%s). Defaulting to StartTLS.", modeString);

			Poco::Net::Context::Ptr pTLSContext;
			std::string cipherList = _pPrefs->configuration()->getString("osp.mail.tls.ciphers", "");
			bool extendedVerification = _pPrefs->configuration()->getBool("osp.mail.tls.extendedCertificateVerification", false);
			std::string caLocation = _pPrefs->configuration()->getString("osp.mail.tls.caLocation", "");
			std::string privateKey = _pPrefs->configuration()->getString("osp.mail.tls.privateKey", "");
			std::string certificate = _pPrefs->configuration()->getString("osp.mail.tls.certificate", "");

			if (!cipherList.empty())
			{
				pTLSContext = new Poco::Net::Context(Poco::Net::Context::TLSV1_CLIENT_USE, privateKey, certificate, caLocation, Poco::Net::Context::VERIFY_RELAXED, 5, true, cipherList);
				pTLSContext->enableExtendedCertificateVerification(extendedVerification);
			}

			SecureMailDeliveryServiceImpl::Ptr pMailDeliveryService = new SecureMailDeliveryServiceImpl(mode, config, pBundleContext->logger(), pTLSContext);
			_pMailDeliveryServiceRef = pBundleContext->registry().registerService(MailDeliveryService::SERVICE_NAME, pMailDeliveryService, Properties());
		}
		else
		{
			pBundleContext->logger().notice("Secure mail delivery service not enabled.");
		}
	}

	void stop(BundleContext::Ptr pBundleContext)
	{
		if (_pMailDeliveryServiceRef) pBundleContext->registry().unregisterService(_pMailDeliveryServiceRef);
		_pMailDeliveryServiceRef = 0;
		_pPrefs = 0;
		_pBundleContext = 0;
	}

protected:
	int getIntConfig(const std::string& key)
	{
		return _pPrefs->configuration()->getInt(key, _pBundleContext->thisBundle()->properties().getInt(key));
	}

	std::string getStringConfig(const std::string& key)
	{
		return _pPrefs->configuration()->getString(key, _pBundleContext->thisBundle()->properties().getString(key));
	}

	std::string getStringConfig(const std::string& key, const std::string& deflt)
	{
		return _pPrefs->configuration()->getString(key, _pBundleContext->thisBundle()->properties().getString(key, deflt));
	}

private:
	BundleContext::Ptr _pBundleContext;
	PreferencesService::Ptr _pPrefs;
	ServiceRef::Ptr _pMailDeliveryServiceRef;
};


} } } // namespace Poco::OSP::Mail


POCO_BEGIN_MANIFEST(Poco::OSP::BundleActivator)
	POCO_EXPORT_CLASS(Poco::OSP::Mail::SecureBundleActivator)
POCO_END_MANIFEST
