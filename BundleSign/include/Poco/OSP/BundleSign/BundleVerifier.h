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
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef OSP_BundleSign_BundleVerifier_INCLUDED
#define OSP_BundleSign_BundleVerifier_INCLUDED


#include "Poco/OSP/BundleSign/BundleSign.h"
#include "Poco/OSP/BundleSign/SignatureFile.h"
#include "Poco/OSP/BundleStorage.h"
#include "Poco/Crypto/X509Certificate.h"
#include "Poco/SharedPtr.h"
#include "Poco/DigestEngine.h"
#include <cstddef>
#include <vector>


namespace Poco::OSP::BundleSign {


class OSPBundleSign_API BundleVerifier
	/// The BundleVerifier class is used to verify the
	/// signature of a signed bundle.
{
public:
	BundleVerifier() = delete;
	BundleVerifier(const BundleVerifier&) = delete;
	BundleVerifier& operator = (const BundleVerifier&) = delete;

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

	explicit BundleVerifier(Poco::OSP::BundleStorage::Ptr pBundleStorage);
		/// Creates a BundleVerifier for verifying the bundle
		/// given by pBundleStorage, for example the storage of
		/// a bundle that is being resolved (Poco::OSP::Bundle::storage()).

	~BundleVerifier();
		/// Destroys the BundleVerifier.

	void setAllowSHA1(bool allow);
		/// Allows or refuses (the default) signatures that use the SHA1
		/// digest algorithm. SHA1 is not collision resistant, so a signature
		/// file can be prepared to match a second one with different content.
		/// A refused signature results in BUNDLE_VERIFICATION_FAILED.

	void listSigners(std::vector<std::string>& aliases) const;
		/// Fills the given vectors with the alias names
		/// of all signers of the bundle.
		///
		/// The list is built by searching for <alias>.sf files
		/// in the bundle's META-INF directory.
		///
		/// Throws the exceptions of SignUtility::listFiles().

	[[nodiscard]] std::string firstSigner() const;
		/// Returns the alias name of the signer of the bundle.
		///
		/// If the bundle has more than one signature, the alias
		/// name of the first signer found is returned.
		///
		/// If the bundle does not contain any signatures, returns
		/// an empty string.

	[[nodiscard]] CertificatePtr extractCertificate(const std::string& alias) const;
		/// Extracts the X509 certificate for the signer given
		/// by alias from the bundle's signature file.
		///
		/// If the bundle does not contain a certificate for the
		/// given alias, or if the signature block file is larger than
		/// SignatureFile::MAX_SIGNATURE_BLOCK_SIZE, returns 0.

	[[nodiscard]] VerificationResult verify(const std::string& alias,
		const Poco::Crypto::X509Certificate::List& trustedCertificates, std::vector<std::string>& messages);
		/// Verifies the bundle like verify(alias, certificate, messages),
		/// and in addition decides whether the signer is trusted.
		///
		/// The signer certificate is the certificate stored in the
		/// bundle's signature block file for the given alias. It must be
		/// one of the given trusted certificates, or be issued by one of them.
		/// If the bundle does not contain a certificate, the signature must
		/// verify with one of the trusted certificates.
		/// In both cases, the signer certificate must be within its validity
		/// period, allow code signing if it has an extended key usage extension,
		/// and contain an RSA key. A trusted certificate that cannot verify the
		/// signature is skipped and the next one is tried.
		///
		/// Returns the same values as verify(alias, certificate, messages).
		/// A signer that is not trusted results in BUNDLE_VERIFICATION_FAILED.
		///
		/// This is the verification an application uses to accept only bundles
		/// from known signers.

	[[nodiscard]] VerificationResult verify(const std::string& alias, const Poco::Crypto::X509Certificate& certificate,
		std::vector<std::string>& messages);
		/// Verifies the bundle, using the signature from the signer
		/// given by alias, as well as the signer's public key
		/// extracted from the given certificate.
		///
		/// This only verifies that the bundle has not been modified since
		/// it was signed with the key of the given certificate; it takes no
		/// decision whether that certificate is trusted.
		///
		/// First, checks whether a signature from the given signer
		/// exists in the bundle.
		/// Second, verifies the integrity of the signature file, using
		/// the public key from the certificate.
		/// The signature block file must not be larger than
		/// SignatureFile::MAX_SIGNATURE_BLOCK_SIZE, and the signature file not
		/// larger than SignatureFile::maxSize() for the files in the bundle.
		/// Third, computes the SHA1/SHA2 digests for all files in the bundle
		/// (see SignUtility::listFiles()), and checks that the signatures match
		/// the signatures in the bundle's signature file. A bundle file whose
		/// entry names are duplicated or not in canonical form fails verification.
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

private:
	struct SignatureFiles
		/// The signature block file and the signature file of one signer.
	{
		std::string digestAlgorithm;
			/// The digest algorithm of the signature.
		Poco::DigestEngine::Digest signature;
			/// The signature of the signature file, from the signature block file.
		std::string content;
			/// The content of the signature file.
		SignatureFile::Signatures signatures;
			/// The file digests listed in the signature file.
	};

	[[nodiscard]] VerificationResult readSignatureFiles(const std::string& alias, SignatureFiles& signatureFiles,
		std::vector<std::string>& messages) const;
		/// Reads and checks the signature block file and the signature file
		/// of the given signer, apart from the signature itself, and stores
		/// them in signatureFiles.

	[[nodiscard]] static VerificationResult verifySignature(const std::string& alias,
		const Poco::Crypto::X509Certificate& certificate, const SignatureFiles& signatureFiles,
		std::vector<std::string>& messages);
		/// Verifies the signature of the signature file with the key of the
		/// given certificate.

	[[nodiscard]] VerificationResult verifyFiles(const std::string& digestAlgorithm,
		SignatureFile::Signatures& signatures, std::vector<std::string>& messages) const;
		/// Compares the digests of all files in the bundle with the given signatures,
		/// and removes the matched entries from signatures.

	[[nodiscard]] static bool isTrusted(const Poco::Crypto::X509Certificate& certificate,
		const Poco::Crypto::X509Certificate::List& trustedCertificates, std::vector<std::string>& messages);
		/// Returns true iff the given certificate is one of the trusted
		/// certificates, or has been issued by one of them.
		/// A trusted certificate without a public key is skipped, and
		/// a message naming it is added to messages.

	[[nodiscard]] static bool isValidSigner(const Poco::Crypto::X509Certificate& certificate,
		std::vector<std::string>& messages);
		/// Returns true iff the given certificate is within its validity period
		/// and allows code signing. Otherwise, adds the reason to messages.

	[[nodiscard]] bool readSignatureFile(const std::string& path, std::size_t maxSize, std::string& content) const;
		/// Reads the signature file or signature block file with the given path
		/// into content. Returns false if the bundle does not contain the file.
		/// Throws a Poco::DataFormatException if the file is larger than
		/// maxSize, before more than that has been read.

	static void digestFromHex(const std::string& hex, Poco::DigestEngine::Digest& digest);
	[[nodiscard]] static int nibble(char ch);

	static const std::string META_INF;
	static const std::string EXT_SF;
	static const std::string EXT_RSA;

	Poco::OSP::BundleStorage::Ptr _pBundleStorage;
	bool _allowSHA1 = false;
};


} // namespace Poco::OSP::BundleSign


#endif // OSP_BundleSign_BundleVerifier_INCLUDED
