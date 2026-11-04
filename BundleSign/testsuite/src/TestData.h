//
// TestData.h
//
// Definition of the TestData class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef TestData_INCLUDED
#define TestData_INCLUDED


#include "Poco/OSP/BundleSign/BundleSign.h"
#include <map>
#include <string>


class TestData
	/// Fixtures shared by the BundleSign tests.
{
public:
	TestData() = delete;

	[[nodiscard]] static std::string createWorkDirectory();
		/// Creates a directory with a unique name in the system's
		/// temporary directory and returns its path.
		/// The caller removes the directory.

	[[nodiscard]] static std::string path(const std::string& fileName);
		/// Returns the path of the given file in the testsuite's
		/// data directory, based on the POCO_BASE environment variable.
		///
		/// The data directory contains certificates and private keys
		/// without personal data, generated for the tests only:
		///   - signer.pem: self-signed, code signing
		///   - foreign.pem: self-signed, code signing, another key with
		///     the subject of signer.pem
		///   - expired.pem: self-signed, code signing, expired
		///   - server.pem: self-signed, TLS server authentication only
		///   - ca.pem: certificate authority (certificate only)
		///   - issued.pem: issued by ca.pem, code signing
		///   - ec.pem: self-signed, code signing, EC key (certificate only)

	static void writeTestBundle(const std::string& path);
		/// Writes a bundle file (ZIP archive) containing bundle.properties,
		/// META-INF/manifest.mf and a directory entry for META-INF/.

	static void writeBundle(const std::string& path, const std::map<std::string, std::string>& files);
		/// Writes a bundle file (ZIP archive) containing the given files
		/// (name and content). For a name with one directory component,
		/// the archive does not contain an entry for the directory.

	static void renameEntry(const std::string& path, const std::string& name, const std::string& newName);
		/// Replaces the name of an entry in the given bundle file with newName,
		/// which must have the same length, without changing anything else.
		/// This creates entry names that Poco::Zip::Compress does not write.

	static void replaceInFile(const std::string& path, const std::string& text, const std::string& replacement);
		/// Replaces every occurrence of text in the given file with replacement.
};


#endif // TestData_INCLUDED
