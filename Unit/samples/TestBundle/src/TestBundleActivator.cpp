//
// TestBundleActivator.cpp
//
// Copyright (c) 2011, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "Poco/OSP/BundleActivator.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/ClassLibrary.h"
#include "Poco/OSP/Unit/TestFactory.h"
#include "BundleTest.h"


using Poco::OSP::BundleActivator;
using Poco::OSP::BundleContext;
using Poco::OSP::Unit::TestFactory;


class TestBundleActivator: public BundleActivator
	/// TestBundle activator.
{
public:
	TestBundleActivator() = default;
	
	~TestBundleActivator() override = default;
	
	void start(BundleContext::Ptr pContext) override
	{
		TestFactory::instance().add(pContext->thisBundle()->symbolicName(), BundleTest::suite());
	}
		
	void stop(BundleContext::Ptr pContext) override
	{
		TestFactory::instance().remove(pContext->thisBundle()->symbolicName());
	}
};


POCO_BEGIN_MANIFEST(BundleActivator)
	POCO_EXPORT_CLASS(TestBundleActivator)
POCO_END_MANIFEST
