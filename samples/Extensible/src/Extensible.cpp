//
// Extensible.cpp
//
// Copyright (c) 2007-2016, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "Poco/OSP/BundleActivator.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/Bundle.h"
#include "Poco/OSP/ServiceRegistry.h"
#include "Poco/OSP/ExtensionPoint.h"
#include "Poco/OSP/ExtensionPointService.h"
#include "Poco/AutoPtr.h"
#include "Poco/ClassLibrary.h"


using Poco::OSP::BundleActivator;
using Poco::OSP::BundleContext;
using Poco::OSP::Bundle;
using Poco::OSP::ServiceRef;
using Poco::OSP::ExtensionPoint;
using Poco::OSP::ExtensionPointService;
using Poco::AutoPtr;


namespace
{
	class SampleExtensionPoint: public ExtensionPoint
	{
	public:
		SampleExtensionPoint(BundleContext::Ptr pContext):
			_pContext(pContext)
		{
		}

		void handleExtension(Bundle::ConstPtr pBundle, Poco::XML::Element* pExtensionElem) override
		{
			std::string msg("Handling extension; bundle: ");
			msg += pBundle->symbolicName();
			msg += "; message: ";
			msg += pExtensionElem->getAttribute("message");
			_pContext->logger().information(msg);
		}

	private:
		BundleContext::Ptr _pContext;
	};


	class ExtensibleBundleActivator: public BundleActivator
		/// Registers an extension point with the ExtensionPointService.
	{
	public:
		ExtensibleBundleActivator() = default;

		~ExtensibleBundleActivator() override = default;

		void start(BundleContext::Ptr pContext) override
		{
			ServiceRef::Ptr pXPSRef = pContext->registry().findByName(ExtensionPointService::SERVICE_NAME);
			if (pXPSRef != nullptr)
			{
				AutoPtr<ExtensionPointService> pXPS = pXPSRef->castedInstance<ExtensionPointService>();

				ExtensionPoint::Ptr pXP = new SampleExtensionPoint(pContext);

				pXPS->registerExtensionPoint(pContext->thisBundle(), "com.appinf.sample.xp", pXP);
			}
			else
			{
				pContext->logger().error("The ExtensionPointService is not available.");
			}
		}

		void stop(BundleContext::Ptr /*pContext*/) override
		{
			// The ExtensionPointService unregisters the extension point when the bundle stops.
		}
	};
}


POCO_BEGIN_MANIFEST(BundleActivator)
	POCO_EXPORT_CLASS(ExtensibleBundleActivator)
POCO_END_MANIFEST
