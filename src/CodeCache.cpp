//
// CodeCache.cpp
//
// Library: OSP
// Package: Util
// Module:  CodeCache
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "Poco/OSP/CodeCache.h"
#include "Poco/OSP/BundleManifest.h"
#include "Poco/File.h"
#include "Poco/FileStream.h"
#include "Poco/SharedLibrary.h"
#include "Poco/StreamCopier.h"
#include "Poco/SHA1Engine.h"
#include "Poco/NumberFormatter.h"
#include "Poco/Process.h"
#include "Poco/Exception.h"


using Poco::File;
using Poco::SharedLibrary;
using Poco::StreamCopier;
using Poco::Timestamp;


namespace Poco::OSP {


CodeCache::CodeCache(const std::string& path, bool shared):
	_path(path)
{
	if (shared) _pMutex = new Poco::NamedMutex(mutexName(path));
	_path.makeDirectory();
	File dir(path);
	dir.createDirectories();
}


CodeCache::~CodeCache() = default;


bool CodeCache::hasLibrary(const std::string& name)
{
	Path p(_path, name);
	File f(p);
	return f.exists();
}


Poco::Timestamp CodeCache::libraryTimestamp(const std::string& name)
{
	Path p(_path, name);
	File f(p);
	return f.getLastModified();
}


void CodeCache::installLibrary(const std::string& name, std::istream& istr)
{
	if (!BundleManifest::isValidFileName(name))
		throw Poco::InvalidArgumentException("Library name cannot be used as a file name in the code cache", name);

	// The library is written to a temporary file that replaces the installed
	// one only when complete: a process that has loaded the previous library
	// keeps its file, and a damaged or truncated copy is never installed.
	Path p(_path, name);
	File f(Path(_path, name + ".tmp." + NumberFormatter::format(Process::id())));
	try
	{
		Poco::FileOutputStream ostr(f.path());
		if (!ostr.good()) throw CreateFileException(f.path());
		StreamCopier::copyStream(istr, ostr);
		ostr.close();
		if (istr.bad() || !istr.eof()) throw ReadFileException("Cannot read library", name);
		if (!ostr.good()) throw WriteFileException(f.path());
		f.setExecutable();
		f.renameTo(p.toString());
	}
	catch (...)
	{
		try
		{
			if (f.exists()) f.remove();
		}
		catch (...)
		{
			// the exception being handled is reported, not the failed cleanup
		}
		throw;
	}
}


void CodeCache::uninstallLibrary(const std::string& name)
{
	if (!BundleManifest::isValidFileName(name))
		throw Poco::InvalidArgumentException("Library name cannot be used as a file name in the code cache", name);

	Path p(_path, name);
	File f(p);
	f.remove();
}


std::string CodeCache::pathFor(const std::string& name, bool appendSuffix)
{
	Path p(_path);
	if (!name.empty())
	{
		if (appendSuffix)
			p.setFileName(name + SharedLibrary::suffix());
		else
			p.setFileName(name);
	}
	return p.toString();
}


void CodeCache::clear()
{
	File dir(_path);
	dir.remove(true);
	dir.createDirectories();
}


void CodeCache::lock()
{
	if (_pMutex != nullptr) _pMutex->lock();
}


void CodeCache::unlock()
{
	if (_pMutex != nullptr) _pMutex->unlock();
}


std::string CodeCache::mutexName(const std::string& path)
{
	std::string name("ospcc");
	Poco::Path p(path);
	p.makeAbsolute();
	p.makeDirectory();
	Poco::SHA1Engine sha1;
	sha1.update(p.toString());
	std::string hash = Poco::DigestEngine::digestToHex(sha1.digest());
	hash.resize(16);
	name += hash;
	return name;
}


} // namespace Poco::OSP
