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
#include "ZipEntryAttributes.h"
#include "Poco/OSP/BundleSign/SignUtility.h"
#include "Poco/OSP/BundleSign/SignatureFile.h"
#include "Poco/OSP/BundleManifest.h"
#include "Poco/Crypto/RSADigestEngine.h"
#include "Poco/Crypto/DigestEngine.h"
#include "Poco/Crypto/X509Certificate.h"
#include "Poco/Zip/ZipManipulator.h"
#include "Poco/Zip/ZipArchive.h"
#include "Poco/Zip/ZipStream.h"
#include "Poco/FileStream.h"
#include "Poco/StreamCopier.h"
#include "Poco/TemporaryFile.h"
#include "Poco/File.h"
#include "Poco/Path.h"
#include "Poco/DigestStream.h"
#include "Poco/Ascii.h"
#include "Poco/Exception.h"
#include "Poco/Format.h"
#include <algorithm>
#include <list>
#include <vector>


namespace Poco::OSP::BundleSign {


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
	BundleSigner(signer, rsaKey, SHA256)
{
}


BundleSigner::BundleSigner(const std::string& signer, const Poco::Crypto::RSAKey& rsaKey, const std::string& digestAlgorithm):
	_signer(signer),
	_rsaKey(rsaKey),
	_digestAlgorithm(digestAlgorithm)
{
	// The alias names the signature files, and a signature file drops white space at the start of a name.
	if (!Poco::OSP::BundleManifest::isValidFileName(signer)
		|| Poco::Ascii::isSpace(static_cast<unsigned char>(signer.front())))
		throw Poco::InvalidArgumentException("The signer alias cannot be used as a file name", signer);

	const std::vector<std::string>& algorithms = SignUtility::digestAlgorithms();
	if (std::find(algorithms.begin(), algorithms.end(), digestAlgorithm) == algorithms.end())
		throw Poco::InvalidArgumentException("Unsupported digest algorithm for signing a bundle", digestAlgorithm);
}

	
BundleSigner::~BundleSigner() = default;


void BundleSigner::signBundle(const std::string& bundlePath)
{
	signBundle(bundlePath, nullptr);
}


void BundleSigner::signBundle(const std::string& bundlePath, const Poco::Crypto::X509Certificate& certificate)
{
	signBundle(bundlePath, &certificate);
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
	if (pCert != nullptr)
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
	// ZipManipulator copies a kept entry raw, and Compress::addFileRaw() can write a local header of another length for
	// an entry with an extra field, so that its data no longer follows the header. Such a file is therefore compressed
	// again, and such a directory entry, which a bundle does not need, is removed.
	std::list<Poco::TemporaryFile> copies;
	Poco::FileInputStream bundleStream(bundlePath);
	const Poco::Zip::ZipArchive& archive = manip.originalArchive();
	for (auto it = archive.headerBegin(); it != archive.headerEnd(); ++it)
	{
		const bool isOwnSignatureFile = it->first == sfBundlePath || it->first == rsaBundlePath;
		if (it->second.isDirectory() && it->second.hasExtraField())
		{
			manip.deleteFile(it->first);
		}
		else if (it->second.hasExtraField() && !isOwnSignatureFile)
		{
			checkAttributesCanBeCopied(archive, it->first, bundlePath);
			Poco::Zip::ZipInputStream entryStream(bundleStream, it->second);
			Poco::FileOutputStream copyStream(copies.emplace_back().path());
			Poco::StreamCopier::copyStream(entryStream, copyStream);
			copyStream.close();
			if (copyStream.fail())
				throw Poco::WriteFileException(copies.back().path());
			// The stream reports a CRC mismatch and a decompression error only in its state.
			if (entryStream.bad() || !entryStream.crcValid())
				throw Poco::DataFormatException("Corrupt entry " + it->first + " in bundle file", bundlePath);
			// ZipManipulator::replaceFile() takes the modification time of the new entry from the file.
			Poco::File(copies.back().path()).setLastModified(it->second.lastModifiedAt().timestamp());
			manip.replaceFile(it->first, copies.back().path());
		}
	}
	bundleStream.close();
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


} // namespace Poco::OSP::BundleSign
