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
#include "Poco/OSP/Mail/MailDeliveryService.h"
#include "Poco/OSP/BundleActivator.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/Bundle.h"
#include "Poco/OSP/ServiceRegistry.h"
#include "Poco/OSP/ServiceRef.h"
#include "Poco/OSP/ServiceFinder.h"
#include "Poco/OSP/PreferencesService.h"
#include "Poco/ClassLibrary.h"


using Poco::OSP::BundleContext;
using Poco::OSP::ServiceRegistry;
using Poco::OSP::ServiceRef;
using Poco::OSP::Properties;
using Poco::OSP::ServiceFinder;
using Poco::OSP::PreferencesService;


namespace Poco::OSP::Mail {


class BundleActivator: public Poco::OSP::BundleActivator
{
public:
	BundleActivator() = default;

	~BundleActivator() override = default;

	void start(BundleContext::Ptr pContext) override
	{
		_pPrefs = ServiceFinder::find<PreferencesService>(pContext);

		bool enabled = _pPrefs->configuration()->getBool("osp.mail.enable", false);
		bool secure = _pPrefs->configuration()->getBool("osp.mail.secure", false);
		if (enabled && !secure)
		{
			const MailDeliveryServiceImpl::Configuration config = MailDeliveryServiceImpl::loadConfiguration(*_pPrefs->configuration(), pContext->thisBundle()->properties(), pContext->logger());

			_pMailDeliveryService = new MailDeliveryServiceImpl(config, pContext->logger());
			_pMailDeliveryService->start();
			_pMailDeliveryServiceRef = pContext->registry().registerService(MailDeliveryService::SERVICE_NAME, _pMailDeliveryService, Properties());
		}
		else
		{
			pContext->logger().notice("Mail delivery service not enabled.");
		}
	}

	void stop(BundleContext::Ptr pContext) override
	{
		if (_pMailDeliveryService != nullptr) _pMailDeliveryService->stop();
		if (_pMailDeliveryServiceRef != nullptr) pContext->registry().unregisterService(_pMailDeliveryServiceRef);
		_pMailDeliveryServiceRef = nullptr;
		_pMailDeliveryService = nullptr;
		_pPrefs = nullptr;
	}

private:
	PreferencesService::Ptr _pPrefs;
	MailDeliveryServiceImpl::Ptr _pMailDeliveryService;
	ServiceRef::Ptr _pMailDeliveryServiceRef;
};


} // namespace Poco::OSP::Mail


POCO_BEGIN_MANIFEST(Poco::OSP::BundleActivator)
	POCO_EXPORT_CLASS(Poco::OSP::Mail::BundleActivator)
POCO_END_MANIFEST
