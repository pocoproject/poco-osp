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
#include "Poco/OSP/BundleSign/SignUtility.h"
#include "Poco/OSP/BundleSign/SignatureFile.h"
#include "Poco/OSP/BundleFile.h"
#include "Poco/OSP/BundleDirectory.h"
#include "Poco/Crypto/RSADigestEngine.h"
#include "Poco/Crypto/DigestEngine.h"
#include "Poco/DigestStream.h"
#include "Poco/File.h"
#include "Poco/Path.h"
#include "Poco/Format.h"


namespace Poco {
namespace OSP {
namespace BundleSign {


const std::string BundleVerifier::META_INF("META-INF");
const std::string BundleVerifier::EXT_SF("sf");
const std::string BundleVerifier::EXT_RSA("rsa");


BundleVerifier::BundleVerifier(const std::string& bundlePath)
{
	Poco::File f(bundlePath);
	Poco::OSP::BundleStorage::Ptr pBundleStorage;
	if (f.isDirectory())
		_pBundleStorage = new Poco::OSP::BundleDirectory(bundlePath);
	else
		_pBundleStorage = new Poco::OSP::BundleFile(bundlePath);
}


BundleVerifier::~BundleVerifier()
{
}


void BundleVerifier::listSigners(std::vector<std::string>& aliases) const
{
	aliases.clear();
	std::vector<std::string> files;
	_pBundleStorage->list(META_INF, files);
	for (std::vector<std::string>::const_iterator it = files.begin(); it != files.end(); ++it)
	{
		Poco::Path path(*it);
		if (path.getExtension() == EXT_SF)
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
	Poco::SharedPtr<std::istream> pStream = _pBundleStorage->getResource(rsaPath);
	if (pStream)
	{
		CertificatePtr pCert = 0;
		try
		{
			pCert = new Poco::Crypto::X509Certificate(*pStream);
		}
		catch (Poco::Exception&)
		{
		}
		return pCert;
	}
	else return 0;
}


BundleVerifier::VerificationResult BundleVerifier::verify(const std::string& alias, const Poco::Crypto::X509Certificate& certificate, std::vector<std::string>& messages)
{
	messages.clear();

	std::string rsaPath(META_INF);
	rsaPath += "/";
	rsaPath += alias;
	rsaPath += ".";
	rsaPath += EXT_RSA;
	Poco::SharedPtr<std::istream> pRSAStream = _pBundleStorage->getResource(rsaPath);
	if (!pRSAStream) return BUNDLE_VERIFICATION_NO_SIGNATURE;
	SignatureFile rsaFile;
	SignatureFile::Signatures rsaSignatures;
	rsaFile.read(rsaSignatures, *pRSAStream);

	std::string signatureType = rsaFile.digestType();
	std::string digestAlgorithm;
	if (signatureType == "RSA-SHA1-Signature")
	{
		digestAlgorithm = "SHA1";
	}
	else if (signatureType == "RSA-SHA224-Signature")
	{
		digestAlgorithm = "SHA224";
	}
	else if (signatureType == "RSA-SHA256-Signature")
	{
		digestAlgorithm = "SHA256";
	}
	else if (signatureType == "RSA-SHA384-Signature")
	{
		digestAlgorithm = "SHA384";
	}
	else if (signatureType == "RSA-SHA512-Signature")
	{
		digestAlgorithm = "SHA512";
	}
	else
	{
		messages.push_back(Poco::format("The signature type \"%s\"is not supported", signatureType));
		return BUNDLE_VERIFICATION_FAILED;
	}

	std::string sfPath(META_INF);
	sfPath += "/";
	sfPath += alias;
	sfPath += ".";
	sfPath += EXT_SF;

	Poco::DigestEngine::Digest rsaSignature;
	SignatureFile::Signatures::const_iterator it = rsaSignatures.find(sfPath);
	if (it != rsaSignatures.end())
		digestFromHex(it->second, rsaSignature);
	else
		return BUNDLE_VERIFICATION_INVALID_SIGNATURE;

	Poco::SharedPtr<std::istream> pSFStream = _pBundleStorage->getResource(sfPath);
	if (!pSFStream) return BUNDLE_VERIFICATION_INVALID_SIGNATURE;

	Poco::Crypto::RSAKey rsaKey(certificate);
	Poco::Crypto::RSADigestEngine rsaEngine(rsaKey, digestAlgorithm);
	Poco::DigestInputStream digStream(rsaEngine, *pSFStream);

	SignatureFile::Signatures signatures;
	SignatureFile sfFile;
	sfFile.read(signatures, digStream);
	if (!pSFStream->eof()) return BUNDLE_VERIFICATION_INVALID_SIGNATURE;
	if (!rsaEngine.verify(rsaSignature))
	{
		messages.push_back("The integrity of the signature file could not be verified");
		return BUNDLE_VERIFICATION_FAILED;
	}

	SignUtility::Digests digests;
	Poco::Crypto::DigestEngine digestEngine(digestAlgorithm);
	SignUtility::computeDigests(_pBundleStorage, digestEngine, digests);
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


void BundleVerifier::digestFromHex(const std::string& hex, Poco::DigestEngine::Digest& digest)
{
	digest.clear();
	std::string::const_iterator it = hex.begin();
	std::string::const_iterator end = hex.end();
	while (it != end)
	{
		int highNibble = nibble(*it);
		if (highNibble == -1) throw Poco::DataFormatException("Invalid signature: ", hex);
		++it;
		if (it == end)
			throw Poco::DataFormatException("Incomplete signature: ", hex);
		int lowNibble = nibble(*it);
		++it;
		if (lowNibble == -1) throw Poco::DataFormatException("Invalid signature: ", hex);
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


} } } // namespace Poco::OSP::BundleSign
