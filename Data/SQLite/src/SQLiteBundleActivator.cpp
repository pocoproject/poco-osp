//
// SQLiteBundleActivator.cpp
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "Poco/OSP/BundleActivator.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/Bundle.h"
#include "Poco/OSP/ServiceRegistry.h"
#include "Poco/OSP/ServiceRef.h"
#include "Poco/OSP/ServiceFinder.h"
#include "Poco/OSP/PreferencesService.h"
#include "Poco/Data/SQLite/Connector.h"
#include "Poco/ClassLibrary.h"


using Poco::OSP::BundleActivator;
using Poco::OSP::BundleContext;
using Poco::OSP::Bundle;
using Poco::OSP::PreferencesService;
using Poco::OSP::ServiceFinder;


class SQLiteBundleActivator: public BundleActivator
{
public:
	SQLiteBundleActivator() = default;
	
	~SQLiteBundleActivator() override = default;
	
	void start(BundleContext::Ptr pContext) override
	{
		// Read before the connector is registered: the framework does not call stop() when start() fails.
		bool sharedCache = false;
		int heapLimit = 0;
		PreferencesService::Ptr pPrefs = ServiceFinder::find<PreferencesService>(pContext);
		if (pPrefs != nullptr)
		{
			sharedCache = pPrefs->configuration()->getBool("data.sqlite.sharedCache", false);
			heapLimit = pPrefs->configuration()->getInt("data.sqlite.softHeapLimit", 0);
		}

		Poco::Data::SQLite::Connector::registerConnector();
		if (sharedCache)
		{
			Poco::Data::SQLite::Connector::enableSharedCache(sharedCache);
		}
		if (heapLimit > 0)
		{
			Poco::Data::SQLite::Connector::enableSoftHeapLimit(heapLimit);
		}
	}
		
	void stop(BundleContext::Ptr /*pContext*/) override
	{
		Poco::Data::SQLite::Connector::unregisterConnector();
	}
};


POCO_BEGIN_MANIFEST(BundleActivator)
	POCO_EXPORT_CLASS(SQLiteBundleActivator)
POCO_END_MANIFEST
