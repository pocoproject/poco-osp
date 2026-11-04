//
// BundleArchive.h
//
// Definition of the BundleArchive class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef BundleArchive_INCLUDED
#define BundleArchive_INCLUDED


#include <string>
#include <utility>
#include <vector>


class BundleArchive
	/// Creates bundle archives (ZIP files) in memory for the tests.
{
public:
	using Entries = std::vector<std::pair<std::string, std::string>>;
		/// The path of each file in the archive, with its content.

	BundleArchive() = delete;

	[[nodiscard]] static std::string create(const Entries& entries);
		/// Returns a ZIP archive of the given files, stored without compression.
		/// A file two or more directories deep gets an entry for each of its
		/// directories (a/ and a/b/ for a/b/c.txt; none for META-INF/manifest.mf).

	[[nodiscard]] static std::string createFromDirectory(const std::string& path);
		/// Returns an archive as create() does, holding the files
		/// below the directory with the given path.

	static void renameEntry(std::string& archive, const std::string& name, const std::string& newName);
		/// Replaces the name of the given entry, in its local header and in the
		/// central directory, with newName, which can be a name that Poco::Zip
		/// refuses to write. Nothing else is changed.
		///
		/// Throws a Poco::InvalidArgumentException if newName differs in length
		/// from name, a Poco::NotFoundException if there is no such entry, and a
		/// Poco::DataFormatException if the archive has an archive comment or its
		/// records are not where the central directory says.

	[[nodiscard]] static std::string manifest(const std::string& symbolicName, const std::string& version);
		/// Returns the content of a minimal bundle manifest.
};


#endif // BundleArchive_INCLUDED
