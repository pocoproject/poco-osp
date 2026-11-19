//
// ZipEntryAttributes.h
//
// Library: OSP/BundleSign
// Package: BundleSign
// Module:  ZipEntryAttributes
//
// Definition of the checkAttributesCanBeCopied() function.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef OSP_BundleSign_ZipEntryAttributes_INCLUDED
#define OSP_BundleSign_ZipEntryAttributes_INCLUDED


#include "Poco/Zip/ZipArchive.h"
#include "Poco/Zip/ZipFileInfo.h"
#include "Poco/Zip/ZipUtil.h"
#include "Poco/Exception.h"
#include "Poco/Types.h"
#include <string>


namespace Poco::OSP::BundleSign {


void checkAttributesCanBeCopied(const Poco::Zip::ZipArchive& archive, const std::string& entry,
	const std::string& bundlePath);
	/// Throws a Poco::DataFormatException if the given entry of the bundle file has no central directory entry, or if
	/// the Unix mode in the external file attributes of any of its central directory entries is that of a symbolic
	/// link or has an execute bit set. Poco::Zip writes no Unix file attributes for an entry that it compresses again.


//
// inlines
//
inline void checkAttributesCanBeCopied(const Poco::Zip::ZipArchive& archive, const std::string& entry,
	const std::string& bundlePath)
{
	// getExternalFileAttributes() is private: read them from the central directory header (ZIP specification, 4.3.12).
	constexpr Poco::UInt32 EXTERNAL_ATTRIBUTES_POS = 38;
	constexpr Poco::UInt32 FILE_TYPE_MASK = 0170000;
	constexpr Poco::UInt32 SYMBOLIC_LINK = 0120000;
	constexpr Poco::UInt32 EXECUTE_BITS = 0111;

	bool found = false;
	for (auto it = archive.fileInfoBegin(); it != archive.fileInfoEnd(); ++it)
	{
		// ZipArchive keys a second central directory entry of the same name as <name>.duplicate-<n>.
		if (it->second.getFileName() != entry)
			continue;
		found = true;
		const std::string header = it->second.createHeader();
		const Poco::UInt32 mode = Poco::Zip::ZipUtil::get32BitValue(header.data(), EXTERNAL_ATTRIBUTES_POS) >> 16;
		if ((mode & FILE_TYPE_MASK) == SYMBOLIC_LINK)
			throw Poco::DataFormatException("Entry " + entry + " of bundle file " + bundlePath + " is a symbolic link "
				"with an extra field, which cannot be copied; rebuild the bundle with bundle --symlinks");
		if ((mode & EXECUTE_BITS) != 0)
			throw Poco::DataFormatException("Entry " + entry + " of bundle file " + bundlePath + " is an executable "
				"file with an extra field, whose mode cannot be copied; rebuild the bundle with bundle --symlinks");
	}
	if (!found)
		throw Poco::DataFormatException("Entry " + entry + " of bundle file " + bundlePath + " has no central "
			"directory entry");
}


} // namespace Poco::OSP::BundleSign


#endif // OSP_BundleSign_ZipEntryAttributes_INCLUDED
