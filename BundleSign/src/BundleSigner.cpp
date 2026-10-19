//
// BundleSigner.cpp
//
// Library: OSP/BundleSign
// Package: BundleSign
// Module:  BundleSigner
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties, 
// copied or duplicated in any form, in whole or in part.
//


#include "Poco/OSP/BundleSign/BundleSigner.h"
#include "Poco/OSP/BundleSign/SignUtility.h"
#include "Poco/OSP/BundleSign/SignatureFile.h"
#include "Poco/Crypto/RSADigestEngine.h"
#include "Poco/Crypto/DigestEngine.h"
#include "Poco/Zip/ZipManipulator.h"
#include "Poco/FileStream.h"
#include "Poco/TemporaryFile.h"
#include "Poco/File.h"
#include "Poco/Path.h"
#include "Poco/DigestStream.h"
#include "Poco/Format.h"


namespace Poco {
namespace OSP {
namespace BundleSign {


const std::string BundleSigner::SHA1("SHA1");
const std::string BundleSigner::SHA224("SHA224");
const std::string BundleSigner::SHA256("SHA256");
const std::string BundleSigner::SHA384("SHA384");
const std::string BundleSigner::SHA512("SHA512");
const std::string BundleSigner::CREATOR("OSP BundleSigner");
const std::string BundleSigner::SIGNATURE_VERSION("1.0");
const std::string BundleSigner::META_INF("META-INF");
const std::string BundleSigner::EXT_SF(".sf");
const std::string BundleSigner::EXT_RSA(".rsa");


BundleSigner::BundleSigner(const std::string& signer, const Poco::Crypto::RSAKey& rsaKey):
	_signer(signer),
	_rsaKey(rsaKey),
	_digestAlgorithm(SHA1)
{
	poco_assert (!signer.empty());
}


BundleSigner::BundleSigner(const std::string& signer, const Poco::Crypto::RSAKey& rsaKey, const std::string& digestAlgorithm):
	_signer(signer),
	_rsaKey(rsaKey),
	_digestAlgorithm(digestAlgorithm)
{
	poco_assert (!signer.empty());
}

	
BundleSigner::~BundleSigner()
{
}


void BundleSigner::signBundle(const std::string& bundlePath)
{
	signBundle(bundlePath, 0);
}


void BundleSigner::signBundle(const std::string& bundlePath, const Poco::Crypto::X509Certificate& cert)
{
	signBundle(bundlePath, &cert);
}

	
void BundleSigner::signBundle(const std::string& bundlePath, const Poco::Crypto::X509Certificate* pCert)
{
	SignUtility::Digests digests;
	Poco::Crypto::DigestEngine digestEngine(_digestAlgorithm);
	SignUtility::computeDigests(bundlePath, digestEngine, digests);
	
	Poco::TemporaryFile signatureFile;
	Poco::FileOutputStream signatureStream(signatureFile.path());
	Poco::Crypto::RSADigestEngine rsaEngine(_rsaKey, _digestAlgorithm);
	Poco::DigestOutputStream engineStream(rsaEngine, signatureStream);
	
	SignatureFile sf(SIGNATURE_VERSION, CREATOR, Poco::format("%s-Digest", _digestAlgorithm));
	sf.write(digests, engineStream);
	engineStream.close();
	signatureStream.close();
	std::string rsaSignature(Poco::DigestEngine::digestToHex(rsaEngine.signature()));
	
	Poco::TemporaryFile rsaFile;
	Poco::FileOutputStream rsaStream(rsaFile.path());
	writeDSAFile(rsaStream, rsaSignature, pCert);
	rsaStream.close();
	
	Poco::File bundleFile(bundlePath);
	if (bundleFile.isDirectory())
	{
		signBundleDirectory(bundlePath, signatureFile.path(), rsaFile.path());
	}
	else
	{
		signBundleFile(bundlePath, signatureFile.path(), rsaFile.path());
	}
}


void BundleSigner::writeDSAFile(std::ostream& ostr, const std::string& rsaSignature, const Poco::Crypto::X509Certificate* pCert)
{
	SignatureFile::Signatures signatures;
	std::string sfPath(META_INF);
	sfPath += "/";
	sfPath += _signer;
	sfPath += EXT_SF;
	signatures[sfPath] = rsaSignature;	
	SignatureFile rsa(SIGNATURE_VERSION, CREATOR, Poco::format("RSA-%s-Signature", _digestAlgorithm));
	rsa.write(signatures, ostr);
	if (pCert)
	{
		ostr << "X509-Certificate: " << pCert->commonName() << "\n";
		pCert->save(ostr);
	}	
}


void BundleSigner::signBundleFile(const std::string& bundlePath, const std::string& sfPath, const std::string& rsaPath)
{
	std::string sfBundlePath(META_INF);
	sfBundlePath += "/";
	sfBundlePath += _signer;
	sfBundlePath += EXT_SF;

	std::string rsaBundlePath(META_INF);
	rsaBundlePath += "/";
	rsaBundlePath += _signer;
	rsaBundlePath += EXT_RSA;

	Poco::Zip::ZipManipulator manip(bundlePath, false);
	manip.addFile(sfBundlePath, sfPath);
	manip.addFile(rsaBundlePath, rsaPath);	
	manip.commit();
}


void BundleSigner::signBundleDirectory(const std::string& bundlePath, const std::string& sfPath, const std::string& rsaPath)
{
	Poco::Path sfBundlePath(bundlePath);
	sfBundlePath.makeDirectory();
	sfBundlePath.pushDirectory(META_INF);
	sfBundlePath.setFileName(_signer + EXT_SF);
	Poco::File sfFile(sfPath);
	sfFile.copyTo(sfBundlePath.toString());
	
	Poco::Path rsaBundlePath(bundlePath);
	rsaBundlePath.makeDirectory();
	rsaBundlePath.pushDirectory(META_INF);
	rsaBundlePath.setFileName(_signer + EXT_RSA);
	Poco::File rsaFile(rsaPath);
	rsaFile.copyTo(rsaBundlePath.toString());
}


} } } // namespace Poco::OSP::BundleSign
