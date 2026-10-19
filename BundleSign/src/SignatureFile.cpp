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


void SignatureFile::readEntry(std::istream& istr, std::string& name, std::string& value)
{
	static const int eof = std::char_traits<char>::eof();

	name.clear();
	value.clear();
	
	int ch = istr.get();
	while (ch != eof && std::isspace(ch)) ch = istr.get();
	while (ch != eof && ch != ':') 
	{
		name += static_cast<char>(ch);
		ch = istr.get();
	}
	if (ch == ':') ch = istr.get();
	while (ch != eof && std::isspace(ch)) ch = istr.get();
	while (ch != eof && ch != '\n') 
	{
		value += static_cast<char>(ch);
		ch = istr.get();
	}
}


} } } // namespace Poco::OSP::BundleSign
