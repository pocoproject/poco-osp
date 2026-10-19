//
// SignUtility.h
//
// Library: OSP/BundleSign
// Package: BundleSign
// Module:  SignUtility
//
// Definition of the SignUtility class.
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties,
// copied or duplicated in any form, in whole or in part.
//


#ifndef OSP_BundleSign_SignUtility_INCLUDED
#define OSP_BundleSign_SignUtility_INCLUDED


#include "Poco/OSP/BundleSign/BundleSign.h"
#include "Poco/OSP/BundleStorage.h"
#include "Poco/DigestEngine.h"
#include "Poco/SharedPtr.h"
#include <istream>
#include <map>


namespace Poco {
namespace OSP {
namespace BundleSign {


class OSPBundleSign_API SignUtility
	/// This class is used to compute hash digests
	/// for the files in a bundle.
{
public:
	using Digests = std::map<std::string, std::string>;
		/// Maps file names (paths) in a bundle to hash values
		/// of their contents.

	static void computeDigests(const std::string& bundlePath, Poco::DigestEngine& engine, Digests& digests);
		/// Compute digests, using the given digest engine, for all
		/// files stored in the bundle given by bundlePath.
		///
		/// The given bundlePath can be either refer to a bundle file (ZIP archive),
		/// or directory.

	static void computeDigests(Poco::OSP::BundleStorage::Ptr pBundleStorage, Poco::DigestEngine& engine, Digests& digests);
		/// Compute digests, using the given digest engine, for all
		/// files stored in the bundle given by pBundleStorage.

	static std::string computeDigest(std::istream& istr, Poco::DigestEngine& engine);
		/// Computes a digest for the contents of the given stream,
		/// using the given digest engine.

	static bool isSignatureFile(const std::string& path);
		/// Returns true iff the given path references a signature file (META-INF/*.sf or META-INF/*.rsa).

private:
	static void computeDigest(Poco::OSP::BundleStorage::Ptr pBundleStorage, const std::string& path, Poco::DigestEngine& engine, Digests& digests);
	static void computeDigest(Poco::SharedPtr<std::istream> pStream, const std::string& path, Poco::DigestEngine& engine, Digests& digests);
};


} } } // namespace Poco::OSP::BundleSign


#endif // OSP_BundleSign_SignUtility_INCLUDED
