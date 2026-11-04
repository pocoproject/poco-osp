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
#include <cstddef>
#include <istream>
#include <ostream>
#include <map>
#include <string>
#include <vector>


namespace Poco::OSP::BundleSign {


class OSPBundleSign_API SignatureFile
	/// This class reads and writes the signature file of a bundle,
	/// which lists the hash digests of the files in the bundle, and
	/// the signature block file, which lists the signature of the
	/// signature file.
	///
	/// Both files have the following format:
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
	/// Optionally, the signature block file also contains
	/// an X509 certificate in PEM format at the end, which
	/// read() does not read:
	///
	///     X509-Certificate: <common-name>
	///     -----BEGIN CERTIFICATE-----
	///     <certificate, Base64-encoded>
	///     -----END CERTIFICATE-----

{
public:
	SignatureFile(const SignatureFile&) = delete;
	SignatureFile& operator = (const SignatureFile&) = delete;

	using Signatures = std::map<std::string, std::string>;

	static constexpr std::size_t MAX_PATH_LENGTH = 4096;
		/// The maximum length of a file path in a signature file.

	static constexpr std::size_t MAX_LABEL_LENGTH = 64;
		/// The maximum length of an entry name and its separators.

	static constexpr std::size_t MAX_DIGEST_LENGTH = 4096;
		/// The maximum length of a hexadecimal digest or signature
		/// (the RSA signature of a 16384-bit key).

	static constexpr std::size_t MAX_ENTRY_LENGTH = MAX_PATH_LENGTH + MAX_LABEL_LENGTH + MAX_DIGEST_LENGTH;
		/// The maximum number of characters that read() takes for one
		/// entry (one line, including the empty lines before it).
		/// The two entries for one file together do not exceed it either.

	static constexpr std::size_t HEADER_ENTRIES = 2;
		/// The number of entries that write() puts before the entries
		/// of the files: Signature-Version and Created-By.

	static constexpr std::size_t MAX_SIGNATURE_BLOCK_SIZE = 64*1024;
		/// The maximum size of a signature block file (.rsa),
		/// including the certificate.

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
		///
		/// Throws a Poco::InvalidArgumentException if a path is longer
		/// than MAX_PATH_LENGTH or a signature is longer than MAX_DIGEST_LENGTH,
		/// since read() would reject the file, and if a path is empty, begins
		/// with white space or contains a control character, since read()
		/// would return another path.

	void read(Signatures& signatures, std::istream& istr);
		/// Reads signatures from the given stream.
		///
		/// Throws a Poco::DataFormatException if the stream is not
		/// a signature file, or if an entry is longer than MAX_ENTRY_LENGTH.

	[[nodiscard]] static std::size_t maxSize(const std::vector<std::string>& paths, const std::string& digestType,
		std::size_t digestLength);
		/// Returns the maximum size of a signature file that write() writes for the given paths, with digests of the
		/// given type (for example "SHA256-Digest") and length in bytes. Each header entry counts as MAX_ENTRY_LENGTH.

	[[nodiscard]] static bool preservesPath(const std::string& path);
		/// Returns true if read() returns the given path unchanged from a
		/// signature file that write() wrote with it: the path is not empty,
		/// does not begin with white space and contains no control character.
		/// The length of the path is not checked (see MAX_PATH_LENGTH).

	[[nodiscard]] const std::string& version() const;
		/// Returns the signature file version.

	[[nodiscard]] const std::string& creator() const;
		/// Returns the signature file creator.

	[[nodiscard]] const std::string& digestType() const;
		/// Returns the signature file digest type.

protected:
	static void readEntry(std::istream& istr, std::string& name, std::string& value);

private:
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


} // namespace Poco::OSP::BundleSign


#endif // OSP_BundleSign_SignatureFile_INCLUDED
