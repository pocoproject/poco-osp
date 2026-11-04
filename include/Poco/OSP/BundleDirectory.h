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
	///
	/// getResource() opens a file only under the names stored on
	/// disk (see isExactPath()).
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

	[[nodiscard]] static bool isExactPath(const Poco::Path& root, const Poco::Path& path);
		/// Returns true if every segment of path below the directory
		/// root is, byte for byte, the name of an entry of its parent
		/// directory. Returns false if path does not begin with root.
		///
		/// A file system can open a file under another spelling of
		/// its name (case, Unicode normalization, Windows short names,
		/// streams, trailing dots and spaces); such a spelling returns
		/// false. Passes on the exceptions of Poco::DirectoryIterator.

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
