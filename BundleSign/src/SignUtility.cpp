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
#include "Poco/OSP/BundleDirectory.h"
#include "Poco/OSP/BundleFile.h"
#include "Poco/DigestStream.h"
#include "Poco/StreamCopier.h"
#include "Poco/File.h"


namespace Poco {
namespace OSP {
namespace BundleSign {


void SignUtility::computeDigests(const std::string& bundlePath, Poco::DigestEngine& engine, Digests& digests)
{
	Poco::File f(bundlePath);
	Poco::OSP::BundleStorage::Ptr pBundleStorage;
	if (f.isDirectory())
		pBundleStorage = new Poco::OSP::BundleDirectory(bundlePath);
	else
		pBundleStorage = new Poco::OSP::BundleFile(bundlePath);

	computeDigests(pBundleStorage, engine, digests);
}


void SignUtility::computeDigests(Poco::OSP::BundleStorage::Ptr pBundleStorage, Poco::DigestEngine& engine, Digests& digests)
{
	computeDigest(pBundleStorage, "", engine, digests);
}


std::string SignUtility::computeDigest(std::istream& istr, Poco::DigestEngine& engine)
{
	engine.reset();
	Poco::DigestOutputStream ostr(engine);
	Poco::StreamCopier::copyStream(istr, ostr);
	ostr.close();
	return Poco::DigestEngine::digestToHex(engine.digest());
}


void SignUtility::computeDigest(Poco::OSP::BundleStorage::Ptr pBundleStorage, const std::string& path, Poco::DigestEngine& engine, Digests& digests)
{
	Poco::SharedPtr<std::istream> pStream = pBundleStorage->getResource(path);
	if (pStream)
	{
		if (!isSignatureFile(path))
		{
			computeDigest(pStream, path, engine, digests);
		}
	}
	else
	{
		std::vector<std::string> files;
		pBundleStorage->list(path, files);
		for (std::vector<std::string>::const_iterator it = files.begin(); it != files.end(); ++it)
		{
			std::string resource(path);
			if (!resource.empty()) resource += '/';
			resource += *it;
			computeDigest(pBundleStorage, resource, engine, digests);
		}
	}
}


void SignUtility::computeDigest(Poco::SharedPtr<std::istream> istr, const std::string& path, Poco::DigestEngine& engine, Digests& digests)
{
	digests[path] = computeDigest(*istr, engine);
}


bool SignUtility::isSignatureFile(const std::string& path)
{
	Poco::Path p(path, Poco::Path::PATH_UNIX);
	std::string ext = p.getExtension();
	return p.depth() == 1 && p[0] == "META-INF" && (ext == "sf" || ext == "rsa");
}


} } } // namespace Poco::OSP::BundleSign
