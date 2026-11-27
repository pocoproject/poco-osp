//
// GreetingService.cpp
//
// Copyright (c) 2007-2016, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "GreetingService.h"
#include "Poco/OSP/BundleActivator.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/Bundle.h"
#include "Poco/OSP/ServiceRegistry.h"
#include "Poco/ClassLibrary.h"


using Poco::OSP::BundleActivator;
using Poco::OSP::BundleContext;
using Poco::OSP::Properties;
using Poco::OSP::ServiceRef;


namespace
{
	class GreetingServiceImpl: public GreetingService
	{
	public:
		GreetingServiceImpl(BundleContext::Ptr pContext):
			_pContext(pContext)
		{
		}

		~GreetingServiceImpl() override = default;

		[[nodiscard]] std::string greeting() override
		{
			// A localized bundle property: in the language of the locale if it exists, otherwise in English.
			return _pContext->thisBundle()->properties().getString("greeting");
		}

		[[nodiscard]] const std::type_info& type() const override
		{
			return typeid(GreetingService);
		}

		[[nodiscard]] bool isA(const std::type_info& otherType) const override
		{
			std::string name(typeid(GreetingService).name());
			return name == otherType.name() || Service::isA(otherType);
		}

	private:
		BundleContext::Ptr _pContext;
	};


	class GreetingServiceBundleActivator: public BundleActivator
		/// Registers the GreetingService with the Service Registry.
	{
	public:
		GreetingServiceBundleActivator() = default;

		~GreetingServiceBundleActivator() override = default;

		void start(BundleContext::Ptr pContext) override
		{
			GreetingService::Ptr pService = new GreetingServiceImpl(pContext);
			_pService = pContext->registry().registerService("com.appinf.osp.samples.GreetingService",
				pService, Properties());
		}

		void stop(BundleContext::Ptr pContext) override
		{
			pContext->registry().unregisterService(_pService);
		}

	private:
		ServiceRef::Ptr _pService;
	};
}


POCO_BEGIN_MANIFEST(BundleActivator)
	POCO_EXPORT_CLASS(GreetingServiceBundleActivator)
POCO_END_MANIFEST
