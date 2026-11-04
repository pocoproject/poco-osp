//
// BundleArchive.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "BundleArchive.h"
#include "Poco/Zip/Compress.h"
#include "Poco/DateTime.h"
#include "Poco/Path.h"
#include <sstream>


std::string BundleArchive::create(const Entries& entries)
{
	std::ostringstream ostr;
	Poco::Zip::Compress compress(ostr, true);
	for (const auto& [path, content]: entries)
	{
		std::istringstream istr(content);
		compress.addFile(istr, Poco::DateTime(), Poco::Path(path, Poco::Path::PATH_UNIX), Poco::Zip::ZipCommon::CM_STORE);
	}
	compress.close();
	return ostr.str();
}


std::string BundleArchive::manifest(const std::string& symbolicName, const std::string& version)
{
	return
		"Manifest-Version: 1.0\n"
		"Bundle-Name: " + symbolicName + "\n"
		"Bundle-SymbolicName: " + symbolicName + "\n"
		"Bundle-Version: " + version + "\n";
}
