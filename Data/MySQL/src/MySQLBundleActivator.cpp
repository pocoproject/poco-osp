//
// MySQLBundleActivator.cpp
//
// Copyright (c) 2008, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "Poco/OSP/BundleActivator.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/Bundle.h"
#include "Poco/Data/MySQL/Connector.h"
#include "Poco/ClassLibrary.h"


using Poco::OSP::BundleActivator;
using Poco::OSP::BundleContext;
using Poco::OSP::Bundle;


class MySQLBundleActivator: public BundleActivator
{
public:
	MySQLBundleActivator() = default;
	
	~MySQLBundleActivator() override = default;
	
	void start(BundleContext::Ptr /*pContext*/) override
	{
		Poco::Data::MySQL::Connector::registerConnector();
	}
		
	void stop(BundleContext::Ptr /*pContext*/) override
	{
		Poco::Data::MySQL::Connector::unregisterConnector();
	}
};


POCO_BEGIN_MANIFEST(BundleActivator)
	POCO_EXPORT_CLASS(MySQLBundleActivator)
POCO_END_MANIFEST
