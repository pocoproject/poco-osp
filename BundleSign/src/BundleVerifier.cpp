//
// BundleVerifier.cpp
//
// Library: OSP/BundleSign
// Package: BundleSign
// Module:  BundleVerifier
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties,
// copied or duplicated in any form, in whole or in part.
//


#include "Poco/OSP/BundleSign/BundleVerifier.h"
#include "Poco/OSP/BundleSign/BundleSigner.h"
#include "Poco/OSP/BundleSign/SignUtility.h"
#include "Poco/OSP/BundleSign/SignatureFile.h"
#include "Poco/OSP/BundleStorage.h"
#include "Poco/Crypto/RSADigestEngine.h"
#include "Poco/Crypto/DigestEngine.h"
#include "Poco/Crypto/RSAKey.h"
#include "Poco/Crypto/CryptoException.h"
#include "Poco/Crypto/X509Certificate.h"
#include "Poco/DigestEngine.h"
#include "Poco/SharedPtr.h"
#include "Poco/Path.h"
#include "Poco/DateTime.h"
#include "Poco/Format.h"
#include "Poco/Exception.h"
#include "Poco/Bugcheck.h"
#include <openssl/x509v3.h>
#include <array>
#include <cstddef>
#include <ios>
#include <istream>
#include <memory>
#include <sstream>
#include <utility>


namespace Poco::OSP::BundleSign {


const std::string BundleVerifier::META_INF("META-INF");
const std::string BundleVerifier::EXT_SF("sf");
const std::string BundleVerifier::EXT_RSA("rsa");


BundleVerifier::BundleVerifier(const std::string& bundlePath):
	_pBundleStorage(SignUtility::openBundle(bundlePath))
{
}


BundleVerifier::BundleVerifier(Poco::OSP::BundleStorage::Ptr pBundleStorage):
	_pBundleStorage(std::move(pBundleStorage))
{
	poco_check_ptr (_pBundleStorage);
}


BundleVerifier::~BundleVerifier() = default;


void BundleVerifier::setAllowSHA1(bool allow)
{
	_allowSHA1 = allow;
}


void BundleVerifier::listSigners(std::vector<std::string>& aliases) const
{
	aliases.clear();
	std::vector<std::string> files;
	SignUtility::listFiles(_pBundleStorage, files);
	for (const auto& file: files)
	{
		Poco::Path path(file, Poco::Path::PATH_UNIX);
		if (SignUtility::isSignatureFile(file) && path.getExtension() == EXT_SF)
		{
			aliases.push_back(path.getBaseName());
		}
	}
}


std::string BundleVerifier::firstSigner() const
{
	std::vector<std::string> aliases;
	listSigners(aliases);
	if (!aliases.empty())
		return aliases[0];
	else
		return std::string();
}


BundleVerifier::CertificatePtr BundleVerifier::extractCertificate(const std::string& alias) const
{
	std::string rsaPath(META_INF);
	rsaPath += "/";
	rsaPath += alias;
	rsaPath += ".";
	rsaPath += EXT_RSA;
	try
	{
		std::string rsaContent;
		if (!readSignatureFile(rsaPath, SignatureFile::MAX_SIGNATURE_BLOCK_SIZE, rsaContent)) return nullptr;
		std::istringstream rsaStream(rsaContent);
		return Poco::makeShared<Poco::Crypto::X509Certificate>(rsaStream);
	}
	catch (Poco::Exception&)
	{
		// A signature block file without a valid certificate has no certificate to extract.
		return nullptr;
	}
}


BundleVerifier::VerificationResult BundleVerifier::verify(const std::string& alias, const Poco::Crypto::X509Certificate::List& trustedCertificates, std::vector<std::string>& messages)
{
	messages.clear();

	Poco::Crypto::X509Certificate::List signerCertificates;
	std::vector<std::string> signerMessages;
	CertificatePtr pCertificate = extractCertificate(alias);
	if (pCertificate != nullptr)
	{
		if (!isTrusted(*pCertificate, trustedCertificates, signerMessages))
		{
			messages = signerMessages;
			messages.push_back(Poco::format("The certificate of signer %s (%s) is not trusted", alias, pCertificate->subjectName()));
			return BUNDLE_VERIFICATION_FAILED;
		}
		signerCertificates.push_back(*pCertificate);
	}
	else
	{
		signerCertificates = trustedCertificates;
	}
	if (signerCertificates.empty())
	{
		messages.push_back(Poco::format("The signature of %s cannot be verified without a trusted certificate", alias));
		return BUNDLE_VERIFICATION_FAILED;
	}

	// The signature files are read once, when the first usable certificate is found;
	// only the check of the signature depends on the certificate.
	VerificationResult result = BUNDLE_VERIFICATION_FAILED;
	SignatureFiles signatureFiles;
	bool haveSignatureFiles = false;
	for (const auto& signerCertificate: signerCertificates)
	{
		if (isValidSigner(signerCertificate, signerMessages))
		{
			if (!haveSignatureFiles)
			{
				result = readSignatureFiles(alias, signatureFiles, signerMessages);
				if (result != BUNDLE_VERIFICATION_OK) break;
				haveSignatureFiles = true;
			}
			result = verifySignature(alias, signerCertificate, signatureFiles, signerMessages);
			if (result != BUNDLE_VERIFICATION_FAILED) break;
		}
	}
	if (result == BUNDLE_VERIFICATION_OK)
	{
		result = verifyFiles(signatureFiles.digestAlgorithm, signatureFiles.signatures, messages);
	}
	else
	{
		// The reasons why each certificate could not be used.
		messages = signerMessages;
	}
	return result;
}


BundleVerifier::VerificationResult BundleVerifier::verify(const std::string& alias, const Poco::Crypto::X509Certificate& certificate, std::vector<std::string>& messages)
{
	messages.clear();

	SignatureFiles signatureFiles;
	VerificationResult result = readSignatureFiles(alias, signatureFiles, messages);
	if (result == BUNDLE_VERIFICATION_OK)
	{
		result = verifySignature(alias, certificate, signatureFiles, messages);
	}
	if (result == BUNDLE_VERIFICATION_OK)
	{
		result = verifyFiles(signatureFiles.digestAlgorithm, signatureFiles.signatures, messages);
	}
	return result;
}


BundleVerifier::VerificationResult BundleVerifier::readSignatureFiles(const std::string& alias, SignatureFiles& signatureFiles, std::vector<std::string>& messages) const
{
	std::string rsaPath(META_INF);
	rsaPath += "/";
	rsaPath += alias;
	rsaPath += ".";
	rsaPath += EXT_RSA;
	signatureFiles = SignatureFiles();
	try
	{
		std::string rsaContent;
		if (!readSignatureFile(rsaPath, SignatureFile::MAX_SIGNATURE_BLOCK_SIZE, rsaContent)) return BUNDLE_VERIFICATION_NO_SIGNATURE;
		std::istringstream rsaStream(rsaContent);
		SignatureFile rsaFile;
		SignatureFile::Signatures rsaSignatures;
		rsaFile.read(rsaSignatures, rsaStream);

		const std::string& signatureType = rsaFile.digestType();
		for (const auto& algorithm: SignUtility::digestAlgorithms())
		{
			if (signatureType == Poco::format("RSA-%s-Signature", algorithm)) signatureFiles.digestAlgorithm = algorithm;
		}
		if (signatureFiles.digestAlgorithm.empty())
		{
			messages.push_back(Poco::format("The signature type \"%s\" is not supported", signatureType));
			return BUNDLE_VERIFICATION_FAILED;
		}
		if (signatureFiles.digestAlgorithm == BundleSigner::SHA1 && !_allowSHA1)
		{
			messages.push_back(Poco::format("The signature of %s uses the SHA1 digest algorithm, which is not allowed", alias));
			return BUNDLE_VERIFICATION_FAILED;
		}

		std::string sfPath(META_INF);
		sfPath += "/";
		sfPath += alias;
		sfPath += ".";
		sfPath += EXT_SF;

		SignatureFile::Signatures::const_iterator it = rsaSignatures.find(sfPath);
		if (it != rsaSignatures.end())
			digestFromHex(it->second, signatureFiles.signature);
		else
			return BUNDLE_VERIFICATION_INVALID_SIGNATURE;

		// The size of the signature file is limited by the number of files it can list.
		std::vector<std::string> files;
		try
		{
			SignUtility::listFiles(_pBundleStorage, files);
		}
		catch (Poco::DataFormatException& exc)
		{
			messages.push_back(exc.message());
			return BUNDLE_VERIFICATION_FAILED;
		}
		if (!readSignatureFile(sfPath, SignatureFile::maxSize(files.size()), signatureFiles.content)) return BUNDLE_VERIFICATION_INVALID_SIGNATURE;

		std::istringstream sfStream(signatureFiles.content);
		SignatureFile sfFile;
		sfFile.read(signatureFiles.signatures, sfStream);
		if (!sfStream.eof()) return BUNDLE_VERIFICATION_INVALID_SIGNATURE;

		// A digest that is not hexadecimal makes the signature file invalid,
		// not the file it belongs to; verifyFiles() relies on this check.
		Poco::DigestEngine::Digest digest;
		for (const auto& signature: signatureFiles.signatures)
		{
			digestFromHex(signature.second, digest);
		}
	}
	catch (Poco::DataFormatException& exc)
	{
		messages.push_back(Poco::format("The signature of %s is malformed: %s", alias, exc.message()));
		return BUNDLE_VERIFICATION_INVALID_SIGNATURE;
	}
	return BUNDLE_VERIFICATION_OK;
}


BundleVerifier::VerificationResult BundleVerifier::verifySignature(const std::string& alias, const Poco::Crypto::X509Certificate& certificate, const SignatureFiles& signatureFiles, std::vector<std::string>& messages)
{
	std::unique_ptr<Poco::Crypto::RSAKey> pRSAKey;
	try
	{
		pRSAKey = std::make_unique<Poco::Crypto::RSAKey>(certificate);
	}
	catch (Poco::Crypto::OpenSSLException&)
	{
		messages.push_back(Poco::format("The certificate %s has no RSA key and cannot verify the signature of %s", certificate.subjectName(), alias));
		return BUNDLE_VERIFICATION_FAILED;
	}
	Poco::Crypto::RSADigestEngine rsaEngine(*pRSAKey, signatureFiles.digestAlgorithm);
	rsaEngine.update(signatureFiles.content);
	if (!rsaEngine.verify(signatureFiles.signature))
	{
		messages.push_back("The integrity of the signature file could not be verified");
		return BUNDLE_VERIFICATION_FAILED;
	}
	return BUNDLE_VERIFICATION_OK;
}


BundleVerifier::VerificationResult BundleVerifier::verifyFiles(const std::string& digestAlgorithm, SignatureFile::Signatures& signatures, std::vector<std::string>& messages) const
{
	SignUtility::Digests digests;
	Poco::Crypto::DigestEngine digestEngine(digestAlgorithm);
	try
	{
		SignUtility::computeDigests(_pBundleStorage, digestEngine, digests);
	}
	catch (Poco::DataFormatException& exc)
	{
		messages.push_back(exc.message());
		return BUNDLE_VERIFICATION_FAILED;
	}
	for (SignUtility::Digests::const_iterator it = digests.begin(); it != digests.end(); ++it)
	{
		if (!SignUtility::isSignatureFile(it->first))
		{
			SignatureFile::Signatures::iterator itSig = signatures.find(it->first);
			if (itSig == signatures.end())
			{
				std::string message("No signature found for file: ");
				message += it->first;
				messages.push_back(message);
			}
			else
			{
				Poco::DigestEngine::Digest digest;
				digestFromHex(it->second, digest);
				Poco::DigestEngine::Digest signature;
				digestFromHex(itSig->second, signature);
				if (digest != signature)
				{
					std::string message("Signature mismatch for file: ");
					message += it->first;
					messages.push_back(message);
				}
				signatures.erase(itSig);
			}
		}
	}
	for (SignatureFile::Signatures::const_iterator it = signatures.begin(); it != signatures.end(); ++it)
	{
		std::string message("File is missing: ");
		message += it->first;
		messages.push_back(message);
	}
	if (messages.empty())
		return BUNDLE_VERIFICATION_OK;
	else
		return BUNDLE_VERIFICATION_FAILED;
}


bool BundleVerifier::isTrusted(const Poco::Crypto::X509Certificate& certificate, const Poco::Crypto::X509Certificate::List& trustedCertificates, std::vector<std::string>& messages)
{
	for (const auto& trustedCertificate: trustedCertificates)
	{
		if (certificate.equals(trustedCertificate)) return true;
		try
		{
			if (certificate.issuedBy(trustedCertificate)) return true;
		}
		catch (Poco::InvalidArgumentException&)
		{
			messages.push_back(Poco::format("The trusted certificate %s has no public key and is skipped", trustedCertificate.subjectName()));
		}
	}
	return false;
}


bool BundleVerifier::isValidSigner(const Poco::Crypto::X509Certificate& certificate, std::vector<std::string>& messages)
{
	const Poco::DateTime now;
	if (now < certificate.validFrom() || now >= certificate.expiresOn())
	{
		messages.push_back(Poco::format("The signer certificate %s is not valid at the current time", certificate.subjectName()));
		return false;
	}
	// Without an extended key usage extension, OpenSSL reports all usages as allowed.
	X509* pCertificate = const_cast<X509*>(certificate.certificate());
	if ((X509_get_extended_key_usage(pCertificate) & XKU_CODE_SIGN) == 0)
	{
		messages.push_back(Poco::format("The signer certificate %s does not allow code signing", certificate.subjectName()));
		return false;
	}
	return true;
}


bool BundleVerifier::readSignatureFile(const std::string& path, std::size_t maxSize, std::string& content) const
{
	std::unique_ptr<std::istream> pStream(_pBundleStorage->getResource(path));
	if (pStream == nullptr) return false;

	constexpr std::size_t BUFFER_SIZE = 4096;
	content.clear();
	std::array<char, BUFFER_SIZE> buffer{};
	while (pStream->read(buffer.data(), static_cast<std::streamsize>(buffer.size())) || pStream->gcount() > 0)
	{
		const auto n = static_cast<std::size_t>(pStream->gcount());
		if (content.size() + n > maxSize)
			throw Poco::DataFormatException("Signature file is too large", path);
		content.append(buffer.data(), n);
	}
	return true;
}


void BundleVerifier::digestFromHex(const std::string& hex, Poco::DigestEngine::Digest& digest)
{
	digest.clear();
	std::string::const_iterator it = hex.begin();
	std::string::const_iterator end = hex.end();
	while (it != end)
	{
		int highNibble = nibble(*it);
		if (highNibble == -1) throw Poco::DataFormatException("Invalid signature", hex);
		++it;
		if (it == end)
			throw Poco::DataFormatException("Incomplete signature", hex);
		int lowNibble = nibble(*it);
		++it;
		if (lowNibble == -1) throw Poco::DataFormatException("Invalid signature", hex);
		int byte = highNibble*16 + lowNibble;
		digest.push_back(static_cast<char>(byte));
	}
}


int BundleVerifier::nibble(char ch)
{
	if (ch >= '0' && ch <= '9')
		return ch - '0';
	else if (ch >= 'A' && ch <= 'F')
		return 10 + ch - 'A';
	else if (ch >= 'a' && ch <= 'f')
		return 10 + ch - 'a';
	else
		return -1;
}


} // namespace Poco::OSP::BundleSign
