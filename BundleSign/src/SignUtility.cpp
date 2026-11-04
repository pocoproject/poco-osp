//
// SignUtility.cpp
//
// Library: OSP/BundleSign
// Package: BundleSign
// Module:  SignUtility
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties,
// copied or duplicated in any form, in whole or in part.
//


#include "Poco/OSP/BundleSign/SignUtility.h"
#include "Poco/OSP/BundleSign/BundleSigner.h"
#include "Poco/OSP/BundleSign/SignatureFile.h"
#include "Poco/OSP/BundleStorage.h"
#include "Poco/Zip/ZipArchive.h"
#include "Poco/Ascii.h"
#include "Poco/DigestEngine.h"
#include "Poco/DigestStream.h"
#include "Poco/DirectoryIterator.h"
#include "Poco/StreamCopier.h"
#include "Poco/StringTokenizer.h"
#include "Poco/FileStream.h"
#include "Poco/Exception.h"
#include "Poco/File.h"
#include "Poco/Path.h"
#include <istream>
#include <memory>
#include <string>
#include <vector>


namespace Poco::OSP::BundleSign {


namespace
{
	void listDirectory(const Poco::Path& directory, const std::string& prefix, std::vector<std::string>& paths)
	{
		Poco::DirectoryIterator end;
		for (Poco::DirectoryIterator it(directory); it != end; ++it)
		{
			// A dangling symbolic link cannot be read by the bundle loader either.
			if (!it->exists()) continue;
			std::string path(prefix);
			path += it.name();
			if (it->isDirectory())
			{
				Poco::Path subdirectory(it.path());
				subdirectory.makeDirectory();
				listDirectory(subdirectory, path + '/', paths);
			}
			else
			{
				paths.push_back(path);
			}
		}
	}


	[[nodiscard]] bool isCanonicalPath(const std::string& path)
	{
		if (path.find('\\') != std::string::npos) return false;
		// A name that the signature file does not preserve could be signed as another name.
		if (!SignatureFile::preservesPath(path)) return false;
		std::string::size_type start = 0;
		while (true)
		{
			const std::string::size_type end = path.find('/', start);
			const std::string component(path, start, end == std::string::npos ? std::string::npos : end - start);
			if (component.empty() || component == "." || component == "..") return false;
			if (Poco::Ascii::isSpace(static_cast<unsigned char>(component.front()))) return false;
			if (end == std::string::npos) return true;
			start = end + 1;
		}
	}
}


Poco::OSP::BundleStorage::Ptr SignUtility::openBundle(const std::string& bundlePath)
{
	return Poco::OSP::BundleStorage::open(bundlePath);
}


void SignUtility::computeDigests(const std::string& bundlePath, Poco::DigestEngine& engine, Digests& digests)
{
	computeDigests(openBundle(bundlePath), engine, digests);
}


void SignUtility::computeDigests(Poco::OSP::BundleStorage::Ptr pBundleStorage, Poco::DigestEngine& engine, Digests& digests)
{
	std::vector<std::string> paths;
	listFiles(pBundleStorage, paths);
	// The names of a directory bundle come from the walk and are exact; BundleDirectory::getResource() would list
	// the parent directory of each file again.
	const Poco::Path directory = Poco::Path::forDirectory(pBundleStorage->path());
	const bool isDirectory = Poco::File(directory).isDirectory();
	for (const auto& path: paths)
	{
		if (!isSignatureFile(path))
		{
			std::unique_ptr<std::istream> pStream;
			if (isDirectory)
			{
				// Not parsed: Poco::Path reads a leading "~" as the home directory and "a:" as a device.
				const Poco::StringTokenizer components(path, "/");
				Poco::Path filePath(directory);
				for (const auto& component: components) filePath.pushDirectory(component);
				filePath.makeFile();
				const Poco::File file(filePath);
				// Like getResource(), regular files only: reading a FIFO would block.
				if (file.exists() && file.isFile()) pStream = std::make_unique<Poco::FileInputStream>(file.path());
			}
			else
			{
				pStream.reset(pBundleStorage->getResource(path));
			}
			if (pStream == nullptr) throw Poco::FileNotFoundException("Cannot read file in bundle", path);
			digests[path] = computeDigest(*pStream, engine);
		}
	}
}


std::string SignUtility::computeDigest(std::istream& istr, Poco::DigestEngine& engine)
{
	engine.reset();
	Poco::DigestOutputStream ostr(engine);
	Poco::StreamCopier::copyStream(istr, ostr);
	ostr.close();
	return Poco::DigestEngine::digestToHex(engine.digest());
}


void SignUtility::listFiles(Poco::OSP::BundleStorage::Ptr pBundleStorage, std::vector<std::string>& paths)
{
	paths.clear();
	// BundleStorage::list() lists one directory and does not tell files from directories.
	Poco::File bundle(pBundleStorage->path());
	if (bundle.isDirectory())
	{
		listDirectory(Poco::Path::forDirectory(pBundleStorage->path()), "", paths);
		for (const auto& path: paths)
		{
			if (!isCanonicalPath(path)) throw Poco::DataFormatException("Invalid file name in bundle", path);
		}
	}
	else
	{
		Poco::FileInputStream istr(pBundleStorage->path());
		Poco::Zip::ZipArchive archive(istr);
		for (auto it = archive.headerBegin(); it != archive.headerEnd(); ++it)
		{
			const std::string& name = it->second.getFileName();
			// ZipArchive keys a second entry with the same name as <name>.duplicate-<n>.
			if (it->first != name) throw Poco::DataFormatException("Duplicate entry in bundle", name);
			if (it->second.isFile())
			{
				if (!isCanonicalPath(name)) throw Poco::DataFormatException("Invalid entry name in bundle", name);
				paths.push_back(name);
			}
			else if (!isCanonicalPath(name.substr(0, name.size() - 1)))
			{
				throw Poco::DataFormatException("Invalid entry name in bundle", name);
			}
		}
	}
}


bool SignUtility::isSignatureFile(const std::string& path)
{
	Poco::Path p(path, Poco::Path::PATH_UNIX);
	std::string ext = p.getExtension();
	return p.depth() == 1 && p[0] == "META-INF" && (ext == "sf" || ext == "rsa");
}


const std::vector<std::string>& SignUtility::digestAlgorithms()
{
	static const std::vector<std::string> algorithms{BundleSigner::SHA1, BundleSigner::SHA224, BundleSigner::SHA256, BundleSigner::SHA384, BundleSigner::SHA512};
	return algorithms;
}


} // namespace Poco::OSP::BundleSign
