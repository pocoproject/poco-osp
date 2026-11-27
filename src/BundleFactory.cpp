//
// BundleFactory.cpp
//
// Library: OSP
// Package: Bundle
// Module:  BundleFactory
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "Poco/OSP/BundleFactory.h"
#include "Poco/OSP/Bundle.h"
#include "Poco/OSP/BundleLoader.h"
#include "Poco/OSP/BundleStorage.h"


namespace Poco::OSP {


BundleFactory::BundleFactory(const LanguageTag& language):
	_language(language)
{
}


BundleFactory::~BundleFactory() = default;


Bundle* BundleFactory::createBundle(BundleLoader& loader, const std::string& path)
{
	return new Bundle(loader.nextBundleId(), loader, BundleStorage::open(path), _language);
}


} // namespace Poco::OSP
