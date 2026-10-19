//
// SignatureFile.h
//
// Library: OSP/BundleSign
// Package: BundleSign
// Module:  SignatureFile
//
// Definition of the SignatureFile class.
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties,
// copied or duplicated in any form, in whole or in part.
//


#ifndef OSP_BundleSign_SignatureFile_INCLUDED
#define OSP_BundleSign_SignatureFile_INCLUDED


#include "Poco/OSP/BundleSign/BundleSign.h"
#include <istream>
#include <ostream>
#include <map>


namespace Poco {
namespace OSP {
namespace BundleSign {


class OSPBundleSign_API SignatureFile
	/// This class is used to compute hash digests
	/// for the files in a bundle.
	///
	/// The signatures file has the following format:
	///     Signature-Version: <version>
	///     Created-By: <creator>
	///
	///     Name: <path1>
	///     <digestType>: <digest1>
	///
	///     Name: <path2>
	///     <digestType>: <digest2>
	///
	///     ...
	///
	///     Name: <pathN>
	///     <digestType>: <digestN>
	///
	/// Optionally, the signature file can also contain
	/// an X509 certificate in PEM format at the end:
	///
	///     X509-Certificate: <common-name>
	///     -----BEGIN CERTIFICATE-----
	///     MIIEFjCCAv6gAwIBAgIBAjALBgkqhkiG9w0BAQUwgdMxEzARBgNVBAMMCmFwcGlu
	///     Zi5jb20xNjA0BgNVBAoMLUFwcGxpZWQgSW5mb3JtYXRpY3MgU29mdHdhcmUgRW5n
	///     ...
	///     -----END CERTIFICATE-----

{
public:
	using Signatures = std::map<std::string, std::string>;

	SignatureFile();
		/// Creates a SignatureFile object for reading an existing
		/// signature file from a stream.

	SignatureFile(const std::string& version, const std::string& creator, const std::string& digestType);
		/// Creates a SignatureFile object for writing a new
		/// signature file to a stream.
		///
		/// The given creator string can be empty -- in this case, no Created-By
		/// entry will be written to the file.

	~SignatureFile();
		/// Destroys the SignatureFile.

	void write(const Signatures& signatures, std::ostream& ostr) const;
		/// Writes a signature file to the given stream.

	void read(Signatures& signatures, std::istream& istr);
		/// Reads signatures from the given stream.

	const std::string& version() const;
		/// Returns the signature file version.

	const std::string& creator() const;
		/// Returns the signature file creator.

	const std::string& digestType() const;
		/// Returns the signature file digest type.

protected:
	static void readEntry(std::istream& istr, std::string& name, std::string& value);

private:
	SignatureFile(const SignatureFile&);
	SignatureFile& operator = (const SignatureFile&);

	static const std::string SIGNATURE_VERSION;
	static const std::string CREATED_BY;
	static const std::string NAME;

	std::string _version;
	std::string _creator;
	std::string _digestType;
};


//
// inlines
//
inline const std::string& SignatureFile::version() const
{
	return _version;
}


inline const std::string& SignatureFile::creator() const
{
	return _creator;
}


inline const std::string& SignatureFile::digestType() const
{
	return _digestType;
}


} } } // namespace Poco::OSP::BundleSign


#endif // OSP_BundleSign_SignatureFile_INCLUDED
