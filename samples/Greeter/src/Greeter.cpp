//
// Greeter.cpp
//
// Copyright (c) 2007-2016, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "Poco/OSP/BundleActivator.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/ServiceRegistry.h"
#include "Poco/ClassLibrary.h"
#include "GreetingService.h"


using Poco::OSP::BundleActivator;
using Poco::OSP::BundleContext;
using Poco::OSP::ServiceRef;


namespace
{
	class GreeterBundleActivator: public BundleActivator
		/// Logs the greeting of the GreetingService.
	{
	public:
		GreeterBundleActivator() = default;

		~GreeterBundleActivator() override = default;

		void start(BundleContext::Ptr pContext) override
		{
			ServiceRef::Ptr pServiceRef = pContext->registry().findByName("com.appinf.osp.samples.GreetingService");
			if (pServiceRef != nullptr)
			{
				GreetingService::Ptr pService = pServiceRef->castedInstance<GreetingService>();
				std::string greeting = pService->greeting();
				std::string msg("****** ");
				msg += greeting;
				msg += " ******";
				pContext->logger().information(msg);
			}
			else
			{
				pContext->logger().error("The GreetingService is not available.");
			}
		}

		void stop(BundleContext::Ptr /*pContext*/) override
		{
		}
	};
}


POCO_BEGIN_MANIFEST(BundleActivator)
	POCO_EXPORT_CLASS(GreeterBundleActivator)
POCO_END_MANIFEST
