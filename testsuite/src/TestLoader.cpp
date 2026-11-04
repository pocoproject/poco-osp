//
// TestLoader.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "TestLoader.h"
#include "Poco/OSP/BundleContextFactory.h"
#include "Poco/OSP/BundleFactory.h"
#include "Poco/OSP/LanguageTag.h"
#include <string>


using Poco::OSP::BundleContextFactory;
using Poco::OSP::BundleFactory;


TestLoader::TestLoader(const std::string& codeCachePath, const Poco::OSP::LanguageTag& language):
	_codeCache(codeCachePath),
	_loader(_codeCache,
		BundleFactory::Ptr(new BundleFactory(language)),
		BundleContextFactory::Ptr(new BundleContextFactory(_registry, _systemEvents)))
{
}


TestLoader::~TestLoader() = default;


Poco::OSP::BundleLoader& TestLoader::loader()
{
	return _loader;
}


Poco::OSP::CodeCache& TestLoader::codeCache()
{
	return _codeCache;
}


Poco::OSP::ServiceRegistry& TestLoader::registry()
{
	return _registry;
}


Poco::OSP::SystemEvents& TestLoader::systemEvents()
{
	return _systemEvents;
}
