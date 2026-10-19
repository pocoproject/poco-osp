//
// RedisBundleActivator.cpp
//
// Copyright (c) 2021-2025, Aleph ONE Software Engineering LLC
// All rights reserved.
//
// License: Aleph ONE Software and Source Code License Agreement
//


#include "Poco/OSP/BundleActivator.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/Bundle.h"
#include "Poco/ClassLibrary.h"


using Poco::OSP::BundleActivator;
using Poco::OSP::BundleContext;
using Poco::OSP::Bundle;


class RedisBundleActivator: public BundleActivator
{
public:
	RedisBundleActivator()
	{
	}

	~RedisBundleActivator()
	{
	}

	void start(BundleContext::Ptr pContext)
	{
	}

	void stop(BundleContext::Ptr pContext)
	{
	}
};


POCO_BEGIN_MANIFEST(BundleActivator)
	POCO_EXPORT_CLASS(RedisBundleActivator)
POCO_END_MANIFEST
