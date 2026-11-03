//
// BundleStorage.cpp
//
// Library: OSP
// Package: Bundle
// Module:  BundleStorage
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#include "Poco/OSP/BundleStorage.h"
#include "Poco/OSP/BundleDirectory.h"
#include "Poco/OSP/BundleFile.h"
#include "Poco/OSP/OSPException.h"
#include "Poco/File.h"
#include <string>


namespace Poco::OSP {


BundleStorage::~BundleStorage() = default;


BundleStorage::Ptr BundleStorage::open(const std::string& path)
{
	File f(path);
	if (f.isFile())
		return new BundleFile(path);
	else if (f.isDirectory())
		return new BundleDirectory(path);
	else
		throw BundleLoadException("Attempted to load a bundle from something that is neither a file nor a directory", path);
}


} // namespace Poco::OSP
