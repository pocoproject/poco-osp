//
// BundleFile.h
//
// Library: OSP
// Package: Bundle
// Module:  BundleFile
//
// Definition of the BundleFile class.
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement

//


#ifndef OSP_BundleFile_INCLUDED
#define OSP_BundleFile_INCLUDED


#include "Poco/OSP/OSP.h"
#include "Poco/OSP/BundleStorage.h"


namespace Poco::Zip {
class ZipArchive;
} // namespace Poco::Zip


namespace Poco::OSP {


class OSP_API BundleFile: public BundleStorage
	/// BundleFile implements the BundleStorage interface
	/// for bundles stored in Zip files.
{
public:
	BundleFile(const std::string& path);
		/// Creates the BundleFile, using the
		/// given path which must specify a Zip file.

	BundleFile() = delete;
	BundleFile(const BundleFile&) = delete;
	BundleFile& operator = (const BundleFile&) = delete;

	// BundleStorage
	[[nodiscard]] std::istream* getResource(const std::string& path) const override;
	void list(const std::string& path, std::vector<std::string>& files) const override;
	Poco::Timestamp lastModified(const std::string& path) const override;
	[[nodiscard]] std::string path() const override;

protected:
	[[nodiscard]] bool isSubdirectoryOf(const std::string& dir, const std::string& parent) const;
		/// Returns true iff dir is a subdirectory of parent.

	~BundleFile() override;
		/// Destroys the BundleDirectory.

private:
	
	std::string _path;
	Poco::Zip::ZipArchive* _pArchive;
};


} // namespace Poco::OSP


#endif // OSP_BundleFile_INCLUDED
