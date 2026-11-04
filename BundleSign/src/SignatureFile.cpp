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
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties, 
// copied or duplicated in any form, in whole or in part.
//


#include "Poco/OSP/BundleSign/SignatureFile.h"
#include "Poco/Exception.h"
#include <cctype>
#include <cstddef>


namespace Poco {
namespace OSP {
namespace BundleSign {


const std::string SignatureFile::SIGNATURE_VERSION("Signature-Version");
const std::string SignatureFile::CREATED_BY("Created-By");
const std::string SignatureFile::NAME("Name");


SignatureFile::SignatureFile()
{
}

	
SignatureFile::SignatureFile(const std::string& version, const std::string& creator, const std::string& digestType):
	_version(version),
	_creator(creator),
	_digestType(digestType)
{
}


SignatureFile::~SignatureFile()
{
}


void SignatureFile::write(const Signatures& signatures, std::ostream& ostr) const
{
	ostr << SIGNATURE_VERSION << ": " << _version << "\n";
	if (!_creator.empty())
	{
	     ostr << CREATED_BY << ": " << _creator << "\n";
	}
	ostr << "\n";
	for (Signatures::const_iterator it = signatures.begin(); it != signatures.end(); ++it)
	{
		if (it->first.size() > MAX_PATH_LENGTH)
			throw Poco::InvalidArgumentException("Path is too long for a signature file", it->first);
		if (it->second.size() > MAX_DIGEST_LENGTH)
			throw Poco::InvalidArgumentException("Signature is too long for a signature file", it->first);
		ostr << NAME << ": " << it->first << "\n"
		     << _digestType << ": " << it->second << "\n"
		     << "\n";
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


std::size_t SignatureFile::maxSize(std::size_t fileCount)
{
	return (fileCount + HEADER_ENTRIES)*MAX_ENTRY_LENGTH;
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


} } } // namespace Poco::OSP::BundleSign
