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
		/// Returns a ZIP archive holding the given files, stored without
		/// compression. Every directory leading to a file gets its own
		/// directory entry.

	[[nodiscard]] static std::string manifest(const std::string& symbolicName, const std::string& version);
		/// Returns the content of a minimal bundle manifest.
};


#endif // BundleArchive_INCLUDED
