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
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties, 
// copied or duplicated in any form, in whole or in part.
//


#ifndef OSP_BundleSign_BundleSigner_INCLUDED
#define OSP_BundleSign_BundleSigner_INCLUDED


#include "Poco/OSP/BundleSign/BundleSign.h"
#include "Poco/Crypto/RSAKey.h"
#include "Poco/Crypto/X509Certificate.h"


namespace Poco {
namespace OSP {
namespace BundleSign {


class OSPBundleSign_API BundleSigner
	/// The BundleSigner class is used sign an existing
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
	BundleSigner(const std::string& signer, const Poco::Crypto::RSAKey& rsaKey);
		/// Creates a BundleSigner for signing bundles using the given
		/// RSA private/public key pair and the SHA256 digest algorithm.

	BundleSigner(const std::string& signer, const Poco::Crypto::RSAKey& rsaKey, const std::string& digestAlgorithm);
		/// Creates a BundleSigner for signing bundles using the given
		/// RSA private/public key pair and digest algorithm,
		/// which should be on of:
		///   - "SHA1" (only for verifiers that cannot use SHA2; a BundleVerifier
		///     accepts it only if allowed with BundleVerifier::setAllowSHA1())
		///   - "SHA256" (SHA2-256, recommended)
		///   - "SHA224" (SHA2-224)
		///   - "SHA384" (SHA2-384)
		///   - "SHA512" (SHA2-512)
		///
		/// Throws a Poco::InvalidArgumentException for any other digest algorithm.
		
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
		/// The given certificate is included in the digital signature file.

	static const std::string SHA1;
	static const std::string SHA224;
	static const std::string SHA256;
	static const std::string SHA384;
	static const std::string SHA512;

protected:
	void signBundle(const std::string& bundlePath, const Poco::Crypto::X509Certificate* pCert);
	void writeDSAFile(std::ostream& ostr, const std::string& rsaSignature, const Poco::Crypto::X509Certificate* pCert);
	void signBundleFile(const std::string& bundlePath, const std::string& sfPath, const std::string& rsaPath);
	void signBundleDirectory(const std::string& bundlePath, const std::string& sfPath, const std::string& rsaPath);

private:
	BundleSigner();
	BundleSigner(const BundleSigner&);
	BundleSigner& operator = (const BundleSigner&);
	
	static const std::string CREATOR;
	static const std::string SIGNATURE_VERSION;
	static const std::string META_INF;
	static const std::string EXT_SF;
	static const std::string EXT_RSA;
	
	std::string _signer;
	Poco::Crypto::RSAKey _rsaKey;
	std::string _digestAlgorithm;
};


} } } // namespace Poco::OSP::BundleSign


#endif // OSP_BundleSign_BundleSigner_INCLUDED
