//
// BundleDirectory.h
//
// Library: OSP
// Package: Bundle
// Module:  BundleDirectory
//
// Definition of the BundleDirectory class.
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#ifndef OSP_BundleDirectory_INCLUDED
#define OSP_BundleDirectory_INCLUDED


#include "Poco/OSP/OSP.h"
#include "Poco/OSP/BundleStorage.h"
#include "Poco/Path.h"


namespace Poco::OSP {


class OSP_API BundleDirectory: public BundleStorage
	/// BundleDirectory implements the BundleStorage interface
	/// for bundles stored in directories.
{
public:
	BundleDirectory(const std::string& path);
		/// Creates the BundleDirectory, using the
		/// given path which must specify the root
		/// directory of the bundle.

	BundleDirectory() = delete;
	BundleDirectory(const BundleDirectory&) = delete;
	BundleDirectory& operator = (const BundleDirectory&) = delete;

	// BundleStorage
	[[nodiscard]] std::istream* getResource(const std::string& path) const override;
	void list(const std::string& path, std::vector<std::string>& files) const override;
	Poco::Timestamp lastModified(const std::string& path) const override;
	[[nodiscard]] std::string path() const override;

protected:
	[[nodiscard]] Poco::Path buildPath(const std::string& path) const;
		/// Builds a path to a file or directory inside the
		/// bundle by concatenating the bundle's path with
		/// the given path, and doing various sanity checks
		/// along the way.

	~BundleDirectory() override;
		/// Destroys the BundleDirectory.

private:
	
	Poco::Path _path;
};


} // namespace Poco::OSP


#endif // OSP_BundleDirectory_INCLUDED
