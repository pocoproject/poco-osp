//
// BundleVerifier.h
//
// Library: OSP/BundleSign
// Package: BundleSign
// Module:  BundleVerifier
//
// Definition of the BundleVerifier class.
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties,
// copied or duplicated in any form, in whole or in part.
//


#ifndef OSP_BundleSign_BundleVerifier_INCLUDED
#define OSP_BundleSign_BundleVerifier_INCLUDED


#include "Poco/OSP/BundleSign/BundleSign.h"
#include "Poco/OSP/BundleStorage.h"
#include "Poco/Crypto/X509Certificate.h"
#include "Poco/SharedPtr.h"
#include "Poco/DigestEngine.h"
#include <vector>


namespace Poco {
namespace OSP {
namespace BundleSign {


class OSPBundleSign_API BundleVerifier
	/// The BundleVerifier class is used to verify the
	/// signature of a signed bundle.
{
public:
	using CertificatePtr = Poco::SharedPtr<Poco::Crypto::X509Certificate>;

	enum VerificationResult
	{
		BUNDLE_VERIFICATION_OK,
			/// Bundle verification was successful.

		BUNDLE_VERIFICATION_NO_SIGNATURE,
			/// No signature was found for the given signer.

		BUNDLE_VERIFICATION_INVALID_SIGNATURE,
			/// The signature files found in the bundle are not valid.

		BUNDLE_VERIFICATION_FAILED
			/// Bundle verification failed.
	};

	explicit BundleVerifier(const std::string& bundlePath);
		/// Creates a BundleVerifier for verifying the
		/// bundle given by bundlePath. The given path can either
		/// refer to a bundle file (in ZIP file format), or a bundle
		/// directory.

	~BundleVerifier();
		/// Destroys the BundleVerifier.

	void listSigners(std::vector<std::string>& aliases) const;
		/// Fills the given vectors with the alias names
		/// of all signers of the bundle.
		///
		/// The list is built by searching for <alias>.sf files
		/// in the bundle's META-INF directory.

	std::string firstSigner() const;
		/// Returns the alias name of the signer of the bundle.
		///
		/// If the bundle has more than one signature, the alias
		/// name of the first signer found is returned.
		///
		/// If the bundle does not contain any signatures, returns
		/// an empty string.

	CertificatePtr extractCertificate(const std::string& alias) const;
		/// Extracts the X509 certificate for the signer given
		/// by alias from the bundle's signature file.
		///
		/// If the bundle does not contain a certificate for the
		/// given alias, returns 0.

	VerificationResult verify(const std::string& alias, const Poco::Crypto::X509Certificate& certificate, std::vector<std::string>& messages);
		/// Verifies the bundle, using the signature from the signer
		/// given by alias, as well as the signer's public key
		/// extracted from the given certificate.
		///
		/// First, checks whether a signature from the given signer
		/// exists in the bundle.
		/// Second, verifies the integrity of the signature file, using
		/// the public key from the certificate.
		/// Third, computes the SHA1/SHA2 digests for all files in the bundle,
		/// and checks that the signatures match the signatures in the
		/// bundle's signature file.
		///
		/// Return values:
		///   * If verification was successful, returns BUNDLE_VERIFICATION_OK.
		///   * If the bundle does not contain a signature from the
		///     given signer, returns BUNDLE_VERIFICATION_NO_SIGNATURE.
		///   * If the signature found in the bundle is not valid (or in a
		///     wrong format), returns BUNDLE_VERIFICATION_INVALID_SIGNATURE.
		///   * If verification fails, returns BUNDLE_VERIFICATION_FAILED and
		///     stores messages describing the reason for the failure
		///     in messages.
		///
		/// Note that verify() can also throw a Poco::Exception if a low-level
		/// error occurs.

protected:
	static void digestFromHex(const std::string& hex, Poco::DigestEngine::Digest& digest);
	static int nibble(char ch);

private:
	BundleVerifier();
	BundleVerifier(const BundleVerifier&);
	BundleVerifier& operator = (const BundleVerifier&);

	static const std::string META_INF;
	static const std::string EXT_SF;
	static const std::string EXT_RSA;

	Poco::OSP::BundleStorage::Ptr _pBundleStorage;
};


} } } // namespace Poco::OSP::BundleSign


#endif // OSP_BundleSign_BundleVerifier_INCLUDED
