//
// BundleSigner.h
//
// Library: OSP/BundleSign
// Package: BundleSign
// Module:  BundleSigner
//
// Definition of the BundleSigner class.
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef OSP_BundleSign_BundleSigner_INCLUDED
#define OSP_BundleSign_BundleSigner_INCLUDED


#include "Poco/OSP/BundleSign/BundleSign.h"
#include "Poco/Crypto/RSAKey.h"
#include "Poco/Crypto/X509Certificate.h"


namespace Poco::OSP::BundleSign {


class OSPBundleSign_API BundleSigner
	/// The BundleSigner class is used to sign an existing
	/// bundle, using a digital signature algorithm
	/// based on the RSA and SHA1/SHA2 message digest algorithms.
	///
	/// A signed bundle contains two additional files:
	///   * <*META-INF/<signer>.sf*> contains message digests (SHA1 or SHA2) 
	///     for every file stored in the bundle. See the
	///     SignatureFile class for a description of the file format.
	///   * <*META-INF/<signer>.rsa*> contains a cryptographic signature,
	///     based on the RSA algorithm and the SHA1/SHA2 digest algorithm,
	///     of the signature file. Optionally, this file also contains
	///     the X509 certificate of the signer in PEM format. The public
	///     key contained in this certificate can be used to verify
	///     the integrity of the signed bundle.
{
public:	
	BundleSigner() = delete;
	BundleSigner(const BundleSigner&) = delete;
	BundleSigner& operator = (const BundleSigner&) = delete;

	BundleSigner(const std::string& signer, const Poco::Crypto::RSAKey& rsaKey);
		/// Creates a BundleSigner for signing bundles using the given
		/// RSA private/public key pair and the SHA256 digest algorithm.
		///
		/// The signature files are named after the signer alias. Throws a Poco::InvalidArgumentException if the alias
		/// cannot be used as a file name (see Poco::OSP::BundleManifest::isValidFileName()) or begins with white space.

	BundleSigner(const std::string& signer, const Poco::Crypto::RSAKey& rsaKey, const std::string& digestAlgorithm);
		/// Creates a BundleSigner for signing bundles using the given
		/// RSA private/public key pair and digest algorithm,
		/// which should be one of:
		///   - "SHA1" (only for verifiers that cannot use SHA2; a BundleVerifier
		///     accepts it only if allowed with BundleVerifier::setAllowSHA1())
		///   - "SHA256" (SHA2-256, recommended)
		///   - "SHA224" (SHA2-224)
		///   - "SHA384" (SHA2-384)
		///   - "SHA512" (SHA2-512)
		///
		/// Throws a Poco::InvalidArgumentException for any other digest algorithm and for a signer alias that the
		/// other constructor rejects.
		
	~BundleSigner();
		/// Destroys the BundleSigner.
		
	void signBundle(const std::string& bundlePath);
		/// Signs the bundle given by path. The given path can either
		/// refer to a bundle file (in ZIP file format), or a bundle
		/// directory.
		///
		/// This method first computes the cryptographic hashes, using
		/// the digest algorithm given to the constructor,
		/// for all files in the bundle and creates the signature file
		/// (named "META-INF/<signer>.sf").
		/// Then, a digital signature is created for the signature file,
		/// and stored in the bundle as well ("META-INF/<signer>.rsa").
		///
		/// In a bundle file, each entry with a ZIP extra field is compressed again with its modification time,
		/// except the signature files of this signer, which are replaced; a directory entry with an extra field is
		/// removed. Poco::Zip adds a directory entry without an extra field for each missing parent directory of a
		/// file that is compressed again two or more levels deep.
		///
		/// Throws a Poco::DataFormatException, leaving the bundle unchanged, if SignUtility::listFiles() rejects a
		/// name in the bundle, or if an entry that is compressed again is corrupt or has the Unix mode of a symbolic
		/// link or one with an execute bit set, since Poco::Zip writes no Unix file attributes for it. Throws a
		/// Poco::InvalidArgumentException, leaving the bundle unchanged, if the name of a file in the bundle other
		/// than a signature file, or "META-INF/<signer>.sf", is longer than SignatureFile::MAX_PATH_LENGTH.

	void signBundle(const std::string& bundlePath, const Poco::Crypto::X509Certificate& certificate);
		/// Signs the bundle given by path. The given path can either
		/// refer to a bundle file (in ZIP file format), or a bundle
		/// directory.
		///
		/// This method first computes the cryptographic hashes, using
		/// the digest algorithm given to the constructor,
		/// for all files in the bundle and creates the signature file
		/// (named "META-INF/<signer>.sf").
		/// Then, a digital signature is created for the signature file,
		/// and stored in the bundle as well ("META-INF/<signer>.rsa").
		///
		/// Changes a bundle file and throws as signBundle(const std::string&) does.
		///
		/// The given certificate is included in the digital signature file.

	static const std::string SHA1;
	static const std::string SHA224;
	static const std::string SHA256;
	static const std::string SHA384;
	static const std::string SHA512;

private:
	void signBundle(const std::string& bundlePath, const Poco::Crypto::X509Certificate* pCert);
	void writeDSAFile(std::ostream& ostr, const std::string& rsaSignature, const Poco::Crypto::X509Certificate* pCert);
	void signBundleFile(const std::string& bundlePath, const std::string& sfPath, const std::string& rsaPath);
	void signBundleDirectory(const std::string& bundlePath, const std::string& sfPath, const std::string& rsaPath);

	static const std::string CREATOR;
	static const std::string SIGNATURE_VERSION;
	static const std::string META_INF;
	static const std::string EXT_SF;
	static const std::string EXT_RSA;
	
	std::string _signer;
	Poco::Crypto::RSAKey _rsaKey;
	std::string _digestAlgorithm;
};


} // namespace Poco::OSP::BundleSign


#endif // OSP_BundleSign_BundleSigner_INCLUDED
