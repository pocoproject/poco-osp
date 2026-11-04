//
// SignatureFile.cpp
//
// Library: OSP/BundleSign
// Package: BundleSign
// Module:  SignatureFile
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "Poco/OSP/BundleSign/SignatureFile.h"
#include "Poco/Ascii.h"
#include "Poco/Exception.h"
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>


namespace Poco::OSP::BundleSign {


namespace {


constexpr std::string_view VALUE_SEPARATOR = ": ";
constexpr std::string_view LINE_END = "\n";


} // anonymous namespace


const std::string SignatureFile::SIGNATURE_VERSION("Signature-Version");
const std::string SignatureFile::CREATED_BY("Created-By");
const std::string SignatureFile::NAME("Name");


SignatureFile::SignatureFile() = default;

	
SignatureFile::SignatureFile(const std::string& version, const std::string& creator, const std::string& digestType):
	_version(version),
	_creator(creator),
	_digestType(digestType)
{
}


SignatureFile::~SignatureFile() = default;


void SignatureFile::write(const Signatures& signatures, std::ostream& ostr) const
{
	ostr << SIGNATURE_VERSION << VALUE_SEPARATOR << _version << LINE_END;
	if (!_creator.empty())
	{
		ostr << CREATED_BY << VALUE_SEPARATOR << _creator << LINE_END;
	}
	ostr << LINE_END;
	for (Signatures::const_iterator it = signatures.begin(); it != signatures.end(); ++it)
	{
		if (it->first.size() > MAX_PATH_LENGTH)
			throw Poco::InvalidArgumentException("Path is too long for a signature file", it->first);
		if (!preservesPath(it->first))
			throw Poco::InvalidArgumentException("Path cannot be written to a signature file", it->first);
		if (it->second.size() > MAX_DIGEST_LENGTH)
			throw Poco::InvalidArgumentException("Signature is too long for a signature file", it->first);
		ostr << NAME << VALUE_SEPARATOR << it->first << LINE_END
		     << _digestType << VALUE_SEPARATOR << it->second << LINE_END
		     << LINE_END;
	}
}

	
void SignatureFile::read(Signatures& signatures, std::istream& istr)
{
	std::string name;
	std::string value;
	
	readEntry(istr, name, value);
	if (name != SIGNATURE_VERSION)
		throw Poco::DataFormatException("No Signature-Version entry in signature file");
	_version = value;
	readEntry(istr, name, value);
	if (name == CREATED_BY)
	{
		_creator = value;
		readEntry(istr, name, value);
	}
	
	while (name == NAME)
	{
		std::string path(value);
		std::string signature;
		readEntry(istr, _digestType, signature);
		signatures[path] = signature;
		readEntry(istr, name, value);
	}
}


std::size_t SignatureFile::maxSize(const std::vector<std::string>& paths, const std::string& digestType,
	std::size_t digestLength)
{
	// write() writes "Name: <path>\n<digestType>: <hexadecimal digest>\n\n" for each path.
	const std::size_t entriesLength = NAME.size() + VALUE_SEPARATOR.size() + LINE_END.size()
		+ digestType.size() + VALUE_SEPARATOR.size() + 2*digestLength + 2*LINE_END.size();
	std::size_t size = HEADER_ENTRIES*MAX_ENTRY_LENGTH;
	for (const auto& path: paths)
	{
		size += entriesLength + path.size();
	}
	return size;
}


bool SignatureFile::preservesPath(const std::string& path)
{
	// read() ends a value at a line feed and drops the white space at its start.
	const auto isControl = [](char c)
	{
		return Poco::Ascii::hasProperties(static_cast<unsigned char>(c), Poco::Ascii::ACP_CONTROL);
	};
	return !path.empty() && !Poco::Ascii::isSpace(static_cast<unsigned char>(path.front()))
		&& std::none_of(path.begin(), path.end(), isControl);
}


void SignatureFile::readEntry(std::istream& istr, std::string& name, std::string& value)
{
	static const int eof = std::char_traits<char>::eof();

	name.clear();
	value.clear();

	// The signature files come from the bundle; the length of an entry
	// is limited, so that reading never allocates without limit.
	std::size_t length = 0;
	const auto next = [&istr, &length]()
	{
		if (++length > MAX_ENTRY_LENGTH) throw Poco::DataFormatException("Signature file entry is too long");
		return istr.get();
	};

	int ch = next();
	while (ch != eof && std::isspace(ch)) ch = next();
	while (ch != eof && ch != ':')
	{
		name += static_cast<char>(ch);
		ch = next();
	}
	if (ch == ':') ch = next();
	while (ch != eof && std::isspace(ch)) ch = next();
	while (ch != eof && ch != '\n')
	{
		value += static_cast<char>(ch);
		ch = next();
	}
}


} // namespace Poco::OSP::BundleSign
