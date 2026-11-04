//
// NetBundleActivator.cpp
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#include "Poco/OSP/BundleActivator.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/Bundle.h"
#include "Poco/OSP/ServiceRegistry.h"
#include "Poco/OSP/ServiceRef.h"
#include "Poco/OSP/ServiceFinder.h"
#include "Poco/OSP/PreferencesService.h"
#include "Poco/Net/HTTPStreamFactory.h"
#include "Poco/Net/FTPStreamFactory.h"
#include "Poco/Net/HTTPSessionInstantiator.h"
#include "Poco/Net/HTTPClientSession.h"
#include "Poco/Net/Net.h"
#include "Poco/Net/ProxyConfig.h"
#include "Poco/Exception.h"
#include "Poco/NumberFormatter.h"
#include "Poco/String.h"
#include "Poco/Types.h"
#include "Poco/ClassLibrary.h"
#include <limits>


using Poco::OSP::BundleActivator;
using Poco::OSP::BundleContext;
using Poco::OSP::Bundle;
using Poco::OSP::PreferencesService;
using Poco::OSP::ServiceFinder;
using Poco::Net::HTTPStreamFactory;
using Poco::Net::FTPStreamFactory;
using Poco::Net::HTTPSessionInstantiator;


class NetBundleActivator: public BundleActivator
{
public:
	NetBundleActivator() = default;
	
	~NetBundleActivator() override = default;
	
	void start(BundleContext::Ptr pContext) override
	{
		// Read before anything is registered: the framework does not call stop() when start() fails.
		// The local copy of the password is cleared on every path.
		Poco::Net::ProxyConfig proxyConfig;
		PreferencesService::Ptr pPrefs = ServiceFinder::find<PreferencesService>(pContext);
		if (pPrefs != nullptr)
		{
			const int proxyPort = pPrefs->configuration()->getInt("http.proxy.port", 80);
			if (proxyPort < 1 || proxyPort > std::numeric_limits<Poco::UInt16>::max())
			{
				throw Poco::InvalidArgumentException("http.proxy.port must be between 1 and 65535",
					Poco::NumberFormatter::format(proxyPort));
			}
			proxyConfig.host          = pPrefs->configuration()->getString("http.proxy.host", "");
			proxyConfig.port          = static_cast<Poco::UInt16>(proxyPort);
			proxyConfig.username      = pPrefs->configuration()->getString("http.proxy.username", "");
			proxyConfig.nonProxyHosts = pPrefs->configuration()->getString("http.proxy.nonProxyHosts", "");
			proxyConfig.password      = pPrefs->configuration()->getString("http.proxy.password", "");
		}

		try
		{
			Poco::Net::initializeNetwork();
			HTTPStreamFactory::registerFactory();
			FTPStreamFactory::registerFactory();
			HTTPSessionInstantiator::registerInstantiator();
			if (pPrefs != nullptr && !proxyConfig.host.empty())
			{
				Poco::Net::HTTPClientSession::setGlobalProxyConfig(proxyConfig);
				_proxyConfigured = true;
			}
		}
		catch (...)
		{
			Poco::secureClear(proxyConfig.password);
			throw;
		}
		Poco::secureClear(proxyConfig.password);
	}
		
	void stop(BundleContext::Ptr /*pContext*/) override
	{
		if (_proxyConfigured)
		{
			// HTTPClientSession keeps the configuration in a static object that it returns as a
			// const reference; replacing it alone leaves the password in the buffer of the string.
			Poco::secureClear(const_cast<Poco::Net::ProxyConfig&>(Poco::Net::HTTPClientSession::getGlobalProxyConfig()).password);
			Poco::Net::HTTPClientSession::setGlobalProxyConfig(Poco::Net::ProxyConfig());
			_proxyConfigured = false;
		}
		HTTPSessionInstantiator::unregisterInstantiator();
		HTTPStreamFactory::unregisterFactory();
		FTPStreamFactory::unregisterFactory();
		Poco::Net::uninitializeNetwork();
	}

private:
	bool _proxyConfigured = false;
};


POCO_BEGIN_MANIFEST(BundleActivator)
	POCO_EXPORT_CLASS(NetBundleActivator)
POCO_END_MANIFEST
