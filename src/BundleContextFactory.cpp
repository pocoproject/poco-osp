//
// BundleContextFactory.cpp
//
// Library: OSP
// Package: Bundle
// Module:  BundleContextFactory
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "Poco/OSP/BundleContextFactory.h"
#include "Poco/OSP/BundleContext.h"


namespace Poco::OSP {


BundleContextFactory::BundleContextFactory(ServiceRegistry& registry, SystemEvents& systemEvents):
	_registry(registry),
	_systemEvents(systemEvents),
	_persistencyDir(Poco::Path::current() + "data")
{
}


BundleContextFactory::BundleContextFactory(ServiceRegistry& registry, SystemEvents& systemEvents, const Poco::Path& persistencyDir):
	_registry(registry),
	_systemEvents(systemEvents),
	_persistencyDir(persistencyDir)
{
}


BundleContextFactory::~BundleContextFactory() = default;


BundleContext* BundleContextFactory::createBundleContext(BundleLoader& loader, Bundle::ConstPtr pBundle, BundleEvents& events)
{
	return new BundleContext(loader, pBundle, events, _systemEvents, _registry, _persistencyDir);
}


} // namespace Poco::OSP
