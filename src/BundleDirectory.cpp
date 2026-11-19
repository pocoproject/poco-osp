//
// BundleDirectory.cpp
//
// Library: OSP
// Package: Bundle
// Module:  BundleDirectory
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "Poco/OSP/BundleDirectory.h"
#include "Poco/File.h"
#include "Poco/DirectoryIterator.h"
#include "Poco/Exception.h"
#include "Poco/FileStream.h"


using Poco::Path;
using Poco::File;
using Poco::DirectoryIterator;


namespace Poco::OSP {


BundleDirectory::BundleDirectory(const std::string& path):
	_path(path)
{
	_path.makeDirectory();
}


BundleDirectory::~BundleDirectory() = default;


std::istream* BundleDirectory::getResource(const std::string& path) const
{
	Path p(buildPath(path));
	File f(p);
	if (f.exists() && f.isFile() && isExactPath(_path, p))
		return new Poco::FileInputStream(f.path());
	else
		return nullptr;
}


void BundleDirectory::list(const std::string& path, std::vector<std::string>& files) const	
{
	files.clear();
	Path p(buildPath(path));
	File f(p);
	if (f.exists() && f.isDirectory())
	{
		DirectoryIterator it(p);
		DirectoryIterator end;
		for (; it != end; ++it)
		{
			files.push_back(it.name());
		}
	}
}


Poco::Timestamp BundleDirectory::lastModified(const std::string& path) const
{
	Path p(buildPath(path));
	File f(p);
	return f.getLastModified();
}
 

std::string BundleDirectory::path() const
{
	return _path.toString();
}


bool BundleDirectory::isExactPath(const Poco::Path& root, const Poco::Path& path)
{
	Path dir(root);
	dir.makeDirectory();
	const int end = path.isFile() ? path.depth() + 1 : path.depth();
	if (end < dir.depth() || path.isAbsolute() != root.isAbsolute() || path.getNode() != root.getNode()
		|| path.getDevice() != root.getDevice())
	{
		return false;
	}
	for (int i = 0; i < dir.depth(); ++i)
	{
		if (path[i] != root[i]) return false;
	}
	for (int i = dir.depth(); i < end; ++i)
	{
		const std::string& name = path[i];
		DirectoryIterator it(dir);
		const DirectoryIterator endIt;
		while (it != endIt && it.name() != name) ++it;
		if (it == endIt) return false;
		dir.pushDirectory(name);
	}
	return true;
}


Poco::Path BundleDirectory::buildPath(const std::string& path) const
{
	Path fullPath(_path);
	if (path.empty())
	{
		fullPath.makeFile();
	}
	else
	{
		if (path.find('\\') != std::string::npos)
			throw Poco::InvalidArgumentException("Resource path contains invalid characters", path);
		Path resPath(path, Path::PATH_UNIX);
		if (!resPath.isRelative()) 
			throw Poco::InvalidArgumentException("Resource path must be relative", path);
		resPath.makeFile();
		for (int i = 0; i < resPath.depth(); ++i)
		{
			const std::string& dir(resPath[i]);
			if (dir == "..") 
				throw Poco::InvalidArgumentException("Resource path must not contain references to a parent directory", path);
			if (dir != ".")
				fullPath.pushDirectory(dir);
		}
		if (resPath.getFileName() == "..")
			throw Poco::InvalidArgumentException("Resource path must not contain references to a parent directory", path);
		fullPath.setFileName(resPath.getFileName());
	}
	return fullPath;
}


} // namespace Poco::OSP
