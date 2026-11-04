//
// BundleArchive.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "BundleArchive.h"
#include "Poco/Zip/Compress.h"
#include "Poco/Zip/ZipUtil.h"
#include "Poco/DateTime.h"
#include "Poco/Exception.h"
#include "Poco/Path.h"
#include "Poco/File.h"
#include "Poco/FileStream.h"
#include "Poco/RecursiveDirectoryIterator.h"
#include "Poco/StreamCopier.h"
#include "Poco/Types.h"
#include <sstream>


using Poco::Zip::ZipUtil;


namespace
{
	// Offsets of the ZIP record fields that renameEntry() reads or changes,
	// from the ZIP file format specification (APPNOTE.TXT).
	constexpr Poco::UInt32 END_RECORD_SIZE = 22;
	constexpr Poco::UInt32 END_ENTRY_COUNT_POS = 10;
	constexpr Poco::UInt32 END_DIRECTORY_OFFSET_POS = 16;
	constexpr Poco::UInt32 DIRECTORY_NAME_LENGTH_POS = 28;
	constexpr Poco::UInt32 DIRECTORY_EXTRA_LENGTH_POS = 30;
	constexpr Poco::UInt32 DIRECTORY_COMMENT_LENGTH_POS = 32;
	constexpr Poco::UInt32 DIRECTORY_LOCAL_HEADER_POS = 42;
	constexpr Poco::UInt32 DIRECTORY_NAME_POS = 46;
	constexpr Poco::UInt32 LOCAL_NAME_POS = 30;
}


std::string BundleArchive::create(const Entries& entries)
{
	std::ostringstream ostr;
	Poco::Zip::Compress compress(ostr, true);
	for (const auto& [path, content]: entries)
	{
		std::istringstream istr(content);
		compress.addFile(istr, Poco::DateTime(), Poco::Path(path, Poco::Path::PATH_UNIX), Poco::Zip::ZipCommon::CM_STORE);
	}
	compress.close();
	return ostr.str();
}


std::string BundleArchive::createFromDirectory(const std::string& path)
{
	const Poco::Path directory(Poco::Path::forDirectory(path));
	const std::string directoryPath(directory.toString(Poco::Path::PATH_UNIX));
	Entries entries;
	for (Poco::SimpleRecursiveDirectoryIterator it(directory); it != Poco::SimpleRecursiveDirectoryIterator(); ++it)
	{
		if (!it->isFile()) continue;
		Poco::FileInputStream istr(it->path());
		std::string content;
		Poco::StreamCopier::copyToString(istr, content);
		entries.emplace_back(it.path().toString(Poco::Path::PATH_UNIX).substr(directoryPath.size()), content);
	}
	return create(entries);
}


void BundleArchive::renameEntry(std::string& archive, const std::string& name,
	const std::string& newName)
{
	if (newName.size() != name.size())
	{
		throw Poco::InvalidArgumentException("New entry name differs in length", newName);
	}

	// create() writes no archive comment, so the end of central directory
	// record is the last END_RECORD_SIZE bytes of the archive.
	if (archive.size() < END_RECORD_SIZE
		|| archive.compare(archive.size() - END_RECORD_SIZE, 4, "PK\5\6") != 0)
	{
		throw Poco::DataFormatException("No end of central directory record at the end of the archive");
	}

	const Poco::UInt32 endRecord = static_cast<Poco::UInt32>(archive.size() - END_RECORD_SIZE);
	const int entryCount = ZipUtil::get16BitValue(archive.data(), endRecord + END_ENTRY_COUNT_POS);
	Poco::UInt32 entry = ZipUtil::get32BitValue(archive.data(), endRecord + END_DIRECTORY_OFFSET_POS);
	bool found = false;
	for (int i = 0; i < entryCount; ++i)
	{
		if (entry > endRecord || endRecord - entry < DIRECTORY_NAME_POS || archive.compare(entry, 4, "PK\1\2") != 0)
		{
			throw Poco::DataFormatException("Invalid central directory entry");
		}

		const Poco::UInt32 nameLength = ZipUtil::get16BitValue(archive.data(), entry + DIRECTORY_NAME_LENGTH_POS);
		if (archive.compare(entry + DIRECTORY_NAME_POS, nameLength, name) == 0)
		{
			const Poco::UInt32 localHeader =
				ZipUtil::get32BitValue(archive.data(), entry + DIRECTORY_LOCAL_HEADER_POS);
			// A local file header and its name lie before the central directory entry.
			if (localHeader > entry || entry - localHeader < LOCAL_NAME_POS + nameLength
				|| archive.compare(localHeader, 4, "PK\3\4") != 0
				|| archive.compare(localHeader + LOCAL_NAME_POS, nameLength, name) != 0)
			{
				throw Poco::DataFormatException("Invalid local file header", name);
			}

			archive.replace(localHeader + LOCAL_NAME_POS, nameLength, newName);
			archive.replace(entry + DIRECTORY_NAME_POS, nameLength, newName);
			found = true;
		}
		entry += DIRECTORY_NAME_POS + nameLength
			+ ZipUtil::get16BitValue(archive.data(), entry + DIRECTORY_EXTRA_LENGTH_POS)
			+ ZipUtil::get16BitValue(archive.data(), entry + DIRECTORY_COMMENT_LENGTH_POS);
	}
	if (!found) throw Poco::NotFoundException("Archive entry", name);
}


std::string BundleArchive::manifest(const std::string& symbolicName, const std::string& version)
{
	return
		"Manifest-Version: 1.0\n"
		"Bundle-Name: " + symbolicName + "\n"
		"Bundle-SymbolicName: " + symbolicName + "\n"
		"Bundle-Version: " + version + "\n";
}
