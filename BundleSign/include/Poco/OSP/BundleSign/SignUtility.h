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
#include <istream>
#include <map>
#include <vector>


namespace Poco::OSP::BundleSign {


class OSPBundleSign_API SignUtility
	/// This class is used to compute hash digests
	/// for the files in a bundle.
{
public:
	using Digests = std::map<std::string, std::string>;
		/// Maps file names (paths) in a bundle to hash values
		/// of their contents.

	[[nodiscard]] static Poco::OSP::BundleStorage::Ptr openBundle(const std::string& bundlePath);
		/// Returns Poco::OSP::BundleStorage::open(bundlePath): a
		/// Poco::OSP::BundleDirectory if the given path refers to a
		/// directory, or a Poco::OSP::BundleFile if it refers to a file.
		///
		/// Throws the exceptions of Poco::OSP::BundleStorage::open(),
		/// such as a Poco::OSP::BundleLoadException if bundlePath refers
		/// to neither a file nor a directory.

	static void computeDigests(const std::string& bundlePath, Poco::DigestEngine& engine, Digests& digests);
		/// Compute digests, using the given digest engine, for all
		/// files stored in the bundle given by bundlePath, except
		/// the signature files.
		///
		/// The given bundlePath can be either refer to a bundle file (ZIP archive),
		/// or directory.
		///
		/// Throws the exceptions of listFiles().

	static void computeDigests(Poco::OSP::BundleStorage::Ptr pBundleStorage, Poco::DigestEngine& engine, Digests& digests);
		/// Compute digests, using the given digest engine, for all
		/// files stored in the bundle given by pBundleStorage, except
		/// the signature files.
		///
		/// Throws the exceptions of listFiles().

	static void listFiles(Poco::OSP::BundleStorage::Ptr pBundleStorage, std::vector<std::string>& paths);
		/// Stores the paths of all files in the bundle given by pBundleStorage
		/// in paths, including the signature files.
		///
		/// For a bundle file, these are all file entries of the ZIP archive,
		/// whether or not the archive contains entries for their directories.
		/// For a bundle directory, these are all files in the directory and
		/// its subdirectories.
		///
		/// Throws a Poco::DataFormatException if the ZIP archive contains two
		/// entries with the same name, or an entry whose name is not a relative
		/// path in canonical form (a name that begins with '/', contains a
		/// backslash, or has an empty, "." or ".." component).

	[[nodiscard]] static std::string computeDigest(std::istream& istr, Poco::DigestEngine& engine);
		/// Computes a digest for the contents of the given stream,
		/// using the given digest engine.

	[[nodiscard]] static bool isSignatureFile(const std::string& path);
		/// Returns true iff the given path references a signature file (META-INF/*.sf or META-INF/*.rsa).

	[[nodiscard]] static const std::vector<std::string>& digestAlgorithms();
		/// Returns the names of the digest algorithms that can be used
		/// for signing and verifying bundles: SHA1, SHA224, SHA256, SHA384
		/// and SHA512.
};


} // namespace Poco::OSP::BundleSign


#endif // OSP_BundleSign_SignUtility_INCLUDED
