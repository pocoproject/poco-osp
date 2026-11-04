//
// BundleFile.cpp
//
// Library: OSP
// Package: Bundle
// Module:  BundleFile
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "Poco/OSP/BundleFile.h"
#include "Poco/Zip/ZipArchive.h"
#include "Poco/Zip/ZipStream.h"
#include "Poco/Path.h"
#include "Poco/Exception.h"
#include "Poco/FileStream.h"
#include <set>
#include <iostream>


using Poco::Path;
using Poco::Zip::ZipArchive;
using Poco::Zip::ZipInputStream;
using Poco::Zip::ZipIOS;
using Poco::Zip::ZipLocalFileHeader;


namespace Poco::OSP {


namespace
{
	struct BundleFileStream
		/// Holds the file stream of a BundleFileInputStream. As the first
		/// base class, it is constructed before ZipIOS uses the stream,
		/// and destroyed after it.
	{
		explicit BundleFileStream(const std::string& path):
			fileStream(path)
		{
		}

		Poco::FileInputStream fileStream;
	};


	class BundleFileInputStream: private BundleFileStream, public ZipIOS, public std::istream
	{
	public:
		BundleFileInputStream(const std::string& path, const ZipLocalFileHeader& fileEntry):
			BundleFileStream(path),
			ZipIOS(fileStream, fileEntry, true),
			std::istream(&_buf)
		{
		}
		
		~BundleFileInputStream() override = default;
	};
}


BundleFile::BundleFile(const std::string& path):
	_path(path),
	_pArchive(nullptr)
{
	Poco::FileInputStream istr(path);
	if (istr.good())
		_pArchive = new ZipArchive(istr);
	else
		throw Poco::OpenFileException(path);
}


BundleFile::~BundleFile()
{
	delete _pArchive;
}


std::istream* BundleFile::getResource(const std::string& path) const
{
	poco_assert (_pArchive != nullptr);
	
	ZipArchive::FileHeaders::const_iterator it = _pArchive->findHeader(path);
	if (it != _pArchive->headerEnd() && it->second.isFile())
		return new BundleFileInputStream(_path, it->second);
	else
		return nullptr;
}


void BundleFile::list(const std::string& path, std::vector<std::string>& files) const	
{
	poco_assert (_pArchive != nullptr);
	
	files.clear();
	std::string parent;
	if (!path.empty())
	{
		// ensure parent ends with a slash
		Path parentPath(path, Path::PATH_UNIX);
		parentPath.makeDirectory();
		parent = parentPath.toString(Path::PATH_UNIX);
	}

	// The names are taken from the paths of all entries below parent:
	// an archive need not have entries for its directories.
	std::set<std::string> fileSet;
	for (ZipArchive::FileHeaders::const_iterator it = _pArchive->headerBegin(); it != _pArchive->headerEnd(); ++it)
	{
		if (isSubdirectoryOf(it->first, parent))
		{
			const std::string::size_type endPos = it->first.find('/', parent.size());
			const std::string::size_type length = endPos == std::string::npos ? std::string::npos : endPos - parent.size();
			const std::string name(it->first, parent.size(), length);
			if (!name.empty() && fileSet.insert(name).second)
			{
				files.push_back(name);
			}
		}
	}
}


Poco::Timestamp BundleFile::lastModified(const std::string& path) const
{
	poco_assert (_pArchive != nullptr);

	ZipArchive::FileHeaders::const_iterator it = _pArchive->findHeader(path);
	if (it != _pArchive->headerEnd())
		return it->second.lastModifiedAt().timestamp();
	else
		throw Poco::NotFoundException(path);
}


std::string BundleFile::path() const
{
	return _path;
}


bool BundleFile::isSubdirectoryOf(const std::string& dir, const std::string& parent) const
{
	if (dir.size() > parent.size())
		return dir.compare(0, parent.size(), parent) == 0;
	else
		return false;
}


} // namespace Poco::OSP
