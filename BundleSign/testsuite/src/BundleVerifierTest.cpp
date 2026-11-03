//
// BundleVerifierTest.cpp
//
// Copyright (c) 2007, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "BundleVerifierTest.h"
#include "TestData.h"
#include "Poco/OSP/BundleSign/BundleVerifier.h"
#include "Poco/OSP/BundleSign/BundleSigner.h"
#include "Poco/OSP/BundleSign/SignatureFile.h"
#include "Poco/OSP/BundleSign/SignUtility.h"
#include "Poco/OSP/BundleFile.h"
#include "Poco/Crypto/RSAKey.h"
#include "Poco/Zip/Decompress.h"
#include "Poco/Zip/ZipManipulator.h"
#include "Poco/File.h"
#include "Poco/Path.h"
#include "Poco/FileStream.h"
#include "Poco/StreamCopier.h"
#include "Poco/Format.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "CppUnit/TestCase.h"
#include <cstddef>
#include <utility>
#include <fstream>


using namespace Poco::OSP::BundleSign;
using namespace Poco::OSP;


BundleVerifierTest::BundleVerifierTest(const std::string& name): CppUnit::TestCase(name)
{
}


BundleVerifierTest::~BundleVerifierTest() = default;


void BundleVerifierTest::testVerifyFile()
{
	BundleVerifier verifier1(_bundleFile);
	std::vector<std::string> aliases;
	verifier1.listSigners(aliases);
	assert (aliases.empty());

	BundleVerifier::CertificatePtr pCert = verifier1.extractCertificate("test.signer");
	assertNull (pCert);

	Poco::Crypto::RSAKey rsaKey("", TestData::path("signer.pem"));
	Poco::Crypto::X509Certificate cert(TestData::path("signer.pem"));
	BundleSigner signer("test.signer", rsaKey);
	signer.signBundle(_bundleFile, cert);
	
	BundleVerifier verifier2(_bundleFile);
	verifier2.listSigners(aliases);
	assert (aliases.size() == 1);
	assert (aliases[0] == "test.signer");
	
	pCert = verifier2.extractCertificate("test.signer");
	assertNotNull (pCert);
	
	std::vector<std::string> messages;
	BundleVerifier::VerificationResult result = verifier2.verify("test.signer", *pCert, messages);
	assert (result == BundleVerifier::BUNDLE_VERIFICATION_OK);
	assert (messages.empty());
}


void BundleVerifierTest::testVerifyFileSHA256()
{
	BundleVerifier verifier1(_bundleFile);
	std::vector<std::string> aliases;
	verifier1.listSigners(aliases);
	assert (aliases.empty());

	BundleVerifier::CertificatePtr pCert = verifier1.extractCertificate("test.signer");
	assertNull (pCert);

	Poco::Crypto::RSAKey rsaKey("", TestData::path("signer.pem"));
	Poco::Crypto::X509Certificate cert(TestData::path("signer.pem"));
	BundleSigner signer("test.signer", rsaKey, "SHA256");
	signer.signBundle(_bundleFile, cert);
	
	BundleVerifier verifier2(_bundleFile);
	verifier2.listSigners(aliases);
	assert (aliases.size() == 1);
	assert (aliases[0] == "test.signer");
	
	pCert = verifier2.extractCertificate("test.signer");
	assertNotNull (pCert);
	
	std::vector<std::string> messages;
	BundleVerifier::VerificationResult result = verifier2.verify("test.signer", *pCert, messages);
	assert (result == BundleVerifier::BUNDLE_VERIFICATION_OK);
	assert (messages.empty());
}


void BundleVerifierTest::testVerifyDirectory()
{
	std::ifstream bundleStream(_bundleFile, std::ios::binary);
	Poco::Zip::Decompress decompress(bundleStream, _bundleDirectory);
	decompress.decompressAllFiles();
	bundleStream.close();

	BundleVerifier verifier1(_bundleDirectory);
	std::vector<std::string> aliases;
	verifier1.listSigners(aliases);
	assert (aliases.empty());

	BundleVerifier::CertificatePtr pCert = verifier1.extractCertificate("test.signer");
	assertNull (pCert);
	
	Poco::Crypto::RSAKey rsaKey("", TestData::path("signer.pem"));
	Poco::Crypto::X509Certificate cert(TestData::path("signer.pem"));
	BundleSigner signer("test.signer", rsaKey);
	signer.signBundle(_bundleDirectory, cert);

	BundleVerifier verifier2(_bundleDirectory);
	verifier2.listSigners(aliases);
	assert (aliases.size() == 1);
	assert (aliases[0] == "test.signer");
	
	pCert = verifier2.extractCertificate("test.signer");
	assertNotNull (pCert);
	
	std::vector<std::string> messages;
	BundleVerifier::VerificationResult result = verifier2.verify("test.signer", *pCert, messages);
	assert (result == BundleVerifier::BUNDLE_VERIFICATION_OK);
	assert (messages.empty());
}


void BundleVerifierTest::testVerifyFailModifyFile()
{
	std::ifstream bundleStream(_bundleFile, std::ios::binary);
	Poco::Zip::Decompress decompress(bundleStream, _bundleDirectory);
	decompress.decompressAllFiles();
	bundleStream.close();

	Poco::Crypto::RSAKey rsaKey("", TestData::path("signer.pem"));
	Poco::Crypto::X509Certificate cert(TestData::path("signer.pem"));
	BundleSigner signer("test.signer", rsaKey);
	signer.signBundle(_bundleDirectory, cert);

	// tamper with manifest file
	Poco::Path manifPath(_bundleDirectory);
	manifPath.makeDirectory();
	manifPath.pushDirectory("META-INF");
	manifPath.setFileName("manifest.mf");
	Poco::FileOutputStream manifStream(manifPath.toString());
	manifStream << 
		"Manifest-Version: 1.0\n"
		"Bundle-Name: Bad Bundle\n";
	manifStream.close();

	BundleVerifier verifier(_bundleDirectory);
	BundleVerifier::CertificatePtr pCert = verifier.extractCertificate("test.signer");
	assertNotNull (pCert);
	
	std::vector<std::string> messages;
	BundleVerifier::VerificationResult result = verifier.verify("test.signer", *pCert, messages);
	assert (result == BundleVerifier::BUNDLE_VERIFICATION_FAILED);
	assert (messages.size() == 1);
}


void BundleVerifierTest::testVerifyFailRemoveFile()
{
	std::ifstream bundleStream(_bundleFile, std::ios::binary);
	Poco::Zip::Decompress decompress(bundleStream, _bundleDirectory);
	decompress.decompressAllFiles();
	bundleStream.close();

	Poco::Crypto::RSAKey rsaKey("", TestData::path("signer.pem"));
	Poco::Crypto::X509Certificate cert(TestData::path("signer.pem"));
	BundleSigner signer("test.signer", rsaKey);
	signer.signBundle(_bundleDirectory, cert);

	// remove a file
	Poco::Path propsPath(_bundleDirectory);
	propsPath.makeDirectory();
	propsPath.setFileName("bundle.properties");
	Poco::File propsFile(propsPath.toString());
	propsFile.remove();

	BundleVerifier verifier(_bundleDirectory);
	BundleVerifier::CertificatePtr pCert = verifier.extractCertificate("test.signer");
	assertNotNull (pCert);
	
	std::vector<std::string> messages;
	BundleVerifier::VerificationResult result = verifier.verify("test.signer", *pCert, messages);
	assert (result == BundleVerifier::BUNDLE_VERIFICATION_FAILED);
	assert (messages.size() == 1);
}


void BundleVerifierTest::testVerifyFailAddFile()
{
	std::ifstream bundleStream(_bundleFile, std::ios::binary);
	Poco::Zip::Decompress decompress(bundleStream, _bundleDirectory);
	decompress.decompressAllFiles();
	bundleStream.close();

	Poco::Crypto::RSAKey rsaKey("", TestData::path("signer.pem"));
	Poco::Crypto::X509Certificate cert(TestData::path("signer.pem"));
	BundleSigner signer("test.signer", rsaKey);
	signer.signBundle(_bundleDirectory, cert);

	// add a file
	Poco::Path filePath(_bundleDirectory);
	filePath.makeDirectory();
	filePath.setFileName("someAddedFile");
	Poco::FileOutputStream fileStream(filePath.toString());
	fileStream << "This is a file that does not belong into that bundle.";
	fileStream.close();

	BundleVerifier verifier(_bundleDirectory);
	BundleVerifier::CertificatePtr pCert = verifier.extractCertificate("test.signer");
	assertNotNull (pCert);
	
	std::vector<std::string> messages;
	BundleVerifier::VerificationResult result = verifier.verify("test.signer", *pCert, messages);
	assert (result == BundleVerifier::BUNDLE_VERIFICATION_FAILED);
	assert (messages.size() == 1);
}


void BundleVerifierTest::testVerifyFailFileRenamedInLetterCase()
{
	signBundleDirectory(BundleSigner::SHA256);
	Poco::File(bundleDirectoryFile("bundle.properties")).renameTo(bundleDirectoryFile("Bundle.properties"));

	BundleVerifier verifier(_bundleDirectory);
	Poco::Crypto::X509Certificate cert(TestData::path("signer.pem"));
	std::vector<std::string> messages;
	assertTrue (verifier.verify("test.signer", cert, messages) == BundleVerifier::BUNDLE_VERIFICATION_FAILED);
	const std::vector<std::string> expected{
		"No signature found for file: Bundle.properties",
		"File is missing: bundle.properties"};
	assertTrue (messages == expected);
}


void BundleVerifierTest::testVerifyFailModifyFileInDirectoryNamedTilde()
{
	// Poco::Path parses a leading "~" as the home directory, which has no file of this unique name.
	const std::string name = Poco::Path(_workDir).getFileName();
	Poco::Path file = Poco::Path::forDirectory(_bundleDirectory);
	file.pushDirectory("~");
	file.setFileName(name);
	writeFile(file, "signed");
	signBundleDirectory(BundleSigner::SHA256);
	writeFile(file, "modified");

	BundleVerifier verifier(_bundleDirectory);
	Poco::Crypto::X509Certificate cert(TestData::path("signer.pem"));
	std::vector<std::string> messages;
	assertTrue (verifier.verify("test.signer", cert, messages) == BundleVerifier::BUNDLE_VERIFICATION_FAILED);
	assertTrue (messages == std::vector<std::string>{"Signature mismatch for file: ~/" + name});
}


void BundleVerifierTest::testVerifyFailModifyFileInDirectoryNamedLikeDevice()
{
#if defined(POCO_OS_FAMILY_UNIX)
	// Poco::Path parses a first component "a:" as a device.
	Poco::Path file = Poco::Path::forDirectory(_bundleDirectory);
	file.pushDirectory("a:");
	file.setFileName("file.txt");
	writeFile(file, "signed");
	signBundleDirectory(BundleSigner::SHA256);
	writeFile(file, "modified");

	BundleVerifier verifier(_bundleDirectory);
	Poco::Crypto::X509Certificate cert(TestData::path("signer.pem"));
	std::vector<std::string> messages;
	assertTrue (verifier.verify("test.signer", cert, messages) == BundleVerifier::BUNDLE_VERIFICATION_FAILED);
	assertTrue (messages == std::vector<std::string>{"Signature mismatch for file: a:/file.txt"});
#endif
}


void BundleVerifierTest::testVerifyFailBadSignature()
{
	std::ifstream bundleStream(_bundleFile, std::ios::binary);
	Poco::Zip::Decompress decompress(bundleStream, _bundleDirectory);
	decompress.decompressAllFiles();
	bundleStream.close();

	Poco::Crypto::RSAKey rsaKey("", TestData::path("signer.pem"));
	Poco::Crypto::X509Certificate cert(TestData::path("signer.pem"));
	BundleSigner signer("test.signer", rsaKey);
	signer.signBundle(_bundleDirectory, cert);

	// tamper with signature file
	Poco::Path sfPath(_bundleDirectory);
	sfPath.makeDirectory();
	sfPath.pushDirectory("META-INF");
	sfPath.setFileName("test.signer.sf");
	Poco::FileOutputStream sfStream(sfPath.toString());
	sfStream <<
		"Signature-Version: 1.0\n"
		"Created-By: OSP BundleSigner\n"
		"\n"
		"Name: META-INF/manifest.mf\n"
		"SHA1-Digest: 88fe99ebcba5051ca2bab7981cbaf5c7f14f61ac\n"
		"\n"
		"Name: bundle.properties\n"
		"SHA1-Digest: c86a59ead6ab0165836abcaed82fedebe9bf36f7\n"
		"\n";
	sfStream.close();

	BundleVerifier verifier(_bundleDirectory);
	BundleVerifier::CertificatePtr pCert = verifier.extractCertificate("test.signer");
	assertNotNull (pCert);
	
	std::vector<std::string> messages;
	BundleVerifier::VerificationResult result = verifier.verify("test.signer", *pCert, messages);
	assert (result == BundleVerifier::BUNDLE_VERIFICATION_FAILED);
	assert (messages.size() == 1);
}


void BundleVerifierTest::testVerifyFailMissingSignature()
{
	std::ifstream bundleStream(_bundleFile, std::ios::binary);
	Poco::Zip::Decompress decompress(bundleStream, _bundleDirectory);
	decompress.decompressAllFiles();
	bundleStream.close();

	Poco::Crypto::RSAKey rsaKey("", TestData::path("signer.pem"));
	Poco::Crypto::X509Certificate cert(TestData::path("signer.pem"));
	BundleSigner signer("test.signer", rsaKey);
	signer.signBundle(_bundleDirectory, cert);

	// remove rsa signature file
	Poco::Path rsaPath(_bundleDirectory);
	rsaPath.makeDirectory();
	rsaPath.pushDirectory("META-INF");
	rsaPath.setFileName("test.signer.rsa");
	Poco::File rsaFile(rsaPath.toString());
	rsaFile.remove();

	BundleVerifier verifier(_bundleDirectory);
	
	std::vector<std::string> messages;
	BundleVerifier::VerificationResult result = verifier.verify("test.signer", cert, messages);
	assert (result == BundleVerifier::BUNDLE_VERIFICATION_NO_SIGNATURE);
	assert (messages.size() == 0);
}


void BundleVerifierTest::testVerifyFailUnsupportedSignatureType()
{
	signBundleDirectory(BundleSigner::SHA256);
	TestData::replaceInFile(bundleDirectoryFile("META-INF/test.signer.rsa"), "RSA-SHA256-Signature", "RSA-MD5-Signature");

	BundleVerifier verifier(_bundleDirectory);
	Poco::Crypto::X509Certificate cert(TestData::path("signer.pem"));
	std::vector<std::string> messages;
	assertTrue (verifier.verify("test.signer", cert, messages) == BundleVerifier::BUNDLE_VERIFICATION_FAILED);
	assertTrue (messages.size() == 1);
	assertEqual ("The signature type \"RSA-MD5-Signature\" is not supported", messages[0]);
}


void BundleVerifierTest::testVerifyFailMalformedSignatureBlockFile()
{
	signBundleDirectory(BundleSigner::SHA256);
	Poco::FileOutputStream rsaStream(bundleDirectoryFile("META-INF/test.signer.rsa"));
	rsaStream << "garbage";
	rsaStream.close();

	BundleVerifier verifier(_bundleDirectory);
	Poco::Crypto::X509Certificate cert(TestData::path("signer.pem"));
	std::vector<std::string> messages;
	assertTrue (verifier.verify("test.signer", cert, messages) == BundleVerifier::BUNDLE_VERIFICATION_INVALID_SIGNATURE);
	assertTrue (messages.size() == 1);
}


void BundleVerifierTest::testVerifyFailSignatureNotHexadecimal()
{
	signBundleDirectory(BundleSigner::SHA256);
	Poco::FileOutputStream rsaStream(bundleDirectoryFile("META-INF/test.signer.rsa"));
	rsaStream <<
		"Signature-Version: 1.0\n"
		"Name: META-INF/test.signer.sf\n"
		"RSA-SHA256-Signature: zz\n";
	rsaStream.close();

	BundleVerifier verifier(_bundleDirectory);
	Poco::Crypto::X509Certificate cert(TestData::path("signer.pem"));
	std::vector<std::string> messages;
	assertTrue (verifier.verify("test.signer", cert, messages) == BundleVerifier::BUNDLE_VERIFICATION_INVALID_SIGNATURE);
	assertTrue (messages.size() == 1);
}


void BundleVerifierTest::testVerifyFailOversizedSignatureBlockFile()
{
	signBundleDirectory(BundleSigner::SHA256);
	Poco::FileOutputStream rsaStream(bundleDirectoryFile("META-INF/test.signer.rsa"), std::ios::app);
	rsaStream << std::string(SignatureFile::MAX_SIGNATURE_BLOCK_SIZE, 'x') << "\n";
	rsaStream.close();

	BundleVerifier verifier(_bundleDirectory);
	assertNull (verifier.extractCertificate("test.signer"));
	Poco::Crypto::X509Certificate cert(TestData::path("signer.pem"));
	std::vector<std::string> messages;
	assertTrue (verifier.verify("test.signer", cert, messages) == BundleVerifier::BUNDLE_VERIFICATION_INVALID_SIGNATURE);
	assertTrue (messages.size() == 1);
}


void BundleVerifierTest::testVerifyFailSignatureFileWithMoreEntriesThanFiles()
{
	signBundleDirectory(BundleSigner::SHA256);
	Poco::FileOutputStream sfStream(bundleDirectoryFile("META-INF/test.signer.sf"), std::ios::app);
	// The bundle has four files; 20 entries of long paths exceed what a signature file for them can hold.
	constexpr int ADDED_ENTRIES = 20;
	constexpr std::size_t SHA256_HEX_LENGTH = 64;
	const std::string longPath(SignatureFile::MAX_PATH_LENGTH - 10, 'p');
	for (int i = 0; i < ADDED_ENTRIES; ++i)
	{
		sfStream << "Name: " << longPath << i << "\n"
			<< "SHA256-Digest: " << std::string(SHA256_HEX_LENGTH, '0') << "\n"
			<< "\n";
	}
	sfStream.close();

	BundleVerifier verifier(_bundleDirectory);
	Poco::Crypto::X509Certificate cert(TestData::path("signer.pem"));
	std::vector<std::string> messages;
	assertTrue (verifier.verify("test.signer", cert, messages) == BundleVerifier::BUNDLE_VERIFICATION_INVALID_SIGNATURE);
	assertTrue (messages.size() == 1);
}


void BundleVerifierTest::testVerifyFailSignatureFileLargerThanSignerWrites()
{
	signBundleDirectory(BundleSigner::SHA256);
	std::vector<std::string> files;
	SignUtility::listFiles(SignUtility::openBundle(_bundleDirectory), files);
	constexpr std::size_t SHA256_LENGTH = 32;
	const std::size_t maxSize = SignatureFile::maxSize(files, "SHA256-Digest", SHA256_LENGTH);
	const std::string sfPath(bundleDirectoryFile("META-INF/test.signer.sf"));
	Poco::FileInputStream sfInput(sfPath);
	std::string content;
	Poco::StreamCopier::copyToString(sfInput, content);
	sfInput.close();

	// Entries and white space after the last entry are read, but every entry has a limited length.
	const std::string entry(
		"Name: " + std::string(1000, 'p') + "\nSHA256-Digest: " + std::string(2*SHA256_LENGTH, '0') + "\n\n");
	const std::vector<std::pair<std::size_t, BundleVerifier::VerificationResult>> cases = {
		{maxSize, BundleVerifier::BUNDLE_VERIFICATION_FAILED},
		{maxSize + 1, BundleVerifier::BUNDLE_VERIFICATION_INVALID_SIGNATURE}
	};
	for (const auto& testCase: cases)
	{
		std::string padded(content);
		while (padded.size() + 2*entry.size() < testCase.first) padded += entry;
		padded.append(testCase.first - padded.size(), '\n');
		Poco::FileOutputStream sfOutput(sfPath);
		sfOutput << padded;
		sfOutput.close();

		BundleVerifier verifier(_bundleDirectory);
		Poco::Crypto::X509Certificate cert(TestData::path("signer.pem"));
		std::vector<std::string> messages;
		assertTrue (verifier.verify("test.signer", cert, messages) == testCase.second);
	}
}


void BundleVerifierTest::testVerifyBundleWithManyFiles()
{
	Poco::File(bundleDirectoryFile("META-INF")).createDirectories();
	Poco::FileOutputStream manifestStream(bundleDirectoryFile("META-INF/manifest.mf"));
	manifestStream << "Manifest-Version: 1.0\n";
	manifestStream.close();
	Poco::File(bundleDirectoryFile("resources")).createDirectories();
	// The signature file for this many files is larger than 1 MB.
	constexpr int FILE_COUNT = 9000;
	for (int i = 0; i < FILE_COUNT; ++i)
	{
		Poco::FileOutputStream fileStream(bundleDirectoryFile(Poco::format("resources/file-with-a-longer-name-%05d.txt", i)));
		fileStream << i;
	}
	Poco::Crypto::RSAKey rsaKey("", TestData::path("signer.pem"));
	BundleSigner signer("test.signer", rsaKey, BundleSigner::SHA256);
	signer.signBundle(_bundleDirectory);

	BundleVerifier verifier(_bundleDirectory);
	Poco::Crypto::X509Certificate cert(TestData::path("signer.pem"));
	std::vector<std::string> messages;
	assertTrue (verifier.verify("test.signer", cert, messages) == BundleVerifier::BUNDLE_VERIFICATION_OK);
	assertTrue (messages.empty());
}


void BundleVerifierTest::testVerifyFailSignatureBlockFileWithoutSignatureFileEntry()
{
	signBundleDirectory(BundleSigner::SHA256);
	TestData::replaceInFile(bundleDirectoryFile("META-INF/test.signer.rsa"),
		"Name: META-INF/test.signer.sf", "Name: META-INF/other.signer.sf");

	BundleVerifier verifier(_bundleDirectory);
	Poco::Crypto::X509Certificate cert(TestData::path("signer.pem"));
	std::vector<std::string> messages;
	const BundleVerifier::VerificationResult result = verifier.verify("test.signer", cert, messages);
	assertTrue (result == BundleVerifier::BUNDLE_VERIFICATION_INVALID_SIGNATURE);
}


void BundleVerifierTest::testVerifyFailMissingSignatureFile()
{
	signBundleDirectory(BundleSigner::SHA256);
	Poco::File(bundleDirectoryFile("META-INF/test.signer.sf")).remove();

	BundleVerifier verifier(_bundleDirectory);
	Poco::Crypto::X509Certificate cert(TestData::path("signer.pem"));
	std::vector<std::string> messages;
	const BundleVerifier::VerificationResult result = verifier.verify("test.signer", cert, messages);
	assertTrue (result == BundleVerifier::BUNDLE_VERIFICATION_INVALID_SIGNATURE);
}


void BundleVerifierTest::testVerifyFailContentAfterLastSignatureFileEntry()
{
	signBundleDirectory(BundleSigner::SHA256);
	Poco::FileOutputStream sfStream(bundleDirectoryFile("META-INF/test.signer.sf"), std::ios::app);
	sfStream << "Comment: not an entry of a file\n";
	sfStream.close();

	BundleVerifier verifier(_bundleDirectory);
	Poco::Crypto::X509Certificate cert(TestData::path("signer.pem"));
	std::vector<std::string> messages;
	const BundleVerifier::VerificationResult result = verifier.verify("test.signer", cert, messages);
	assertTrue (result == BundleVerifier::BUNDLE_VERIFICATION_INVALID_SIGNATURE);
}


void BundleVerifierTest::testVerifyFailSignatureFileWithCRLF()
{
	signBundleDirectory(BundleSigner::SHA256);
	TestData::replaceInFile(bundleDirectoryFile("META-INF/test.signer.sf"), "\n", "\r\n");

	BundleVerifier verifier(_bundleDirectory);
	Poco::Crypto::X509Certificate cert(TestData::path("signer.pem"));
	std::vector<std::string> messages;
	const BundleVerifier::VerificationResult result = verifier.verify("test.signer", cert, messages);
	assertTrue (result == BundleVerifier::BUNDLE_VERIFICATION_INVALID_SIGNATURE);
}


void BundleVerifierTest::testVerifySHA2DigestAlgorithms()
{
	for (const auto& algorithm: {BundleSigner::SHA224, BundleSigner::SHA384, BundleSigner::SHA512})
	{
		signBundleDirectory(algorithm);

		BundleVerifier verifier(_bundleDirectory);
		Poco::Crypto::X509Certificate cert(TestData::path("signer.pem"));
		std::vector<std::string> messages;
		assertTrue (verifier.verify("test.signer", cert, messages) == BundleVerifier::BUNDLE_VERIFICATION_OK);
		assertTrue (messages.empty());
	}
}


void BundleVerifierTest::testVerifyTwoSigners()
{
	Poco::Crypto::RSAKey signerKey("", TestData::path("signer.pem"));
	Poco::Crypto::X509Certificate signerCert(TestData::path("signer.pem"));
	BundleSigner signer("first.signer", signerKey);
	signer.signBundle(_bundleFile, signerCert);
	Poco::Crypto::RSAKey issuedKey("", TestData::path("issued.pem"));
	Poco::Crypto::X509Certificate issuedCert(TestData::path("issued.pem"));
	BundleSigner issuedSigner("second.signer", issuedKey);
	issuedSigner.signBundle(_bundleFile, issuedCert);

	BundleVerifier verifier(_bundleFile);
	std::vector<std::string> aliases;
	verifier.listSigners(aliases);
	assertTrue (aliases == std::vector<std::string>({"first.signer", "second.signer"}));
	std::vector<std::string> messages;
	assertTrue (verifier.verify("first.signer", signerCert, messages) == BundleVerifier::BUNDLE_VERIFICATION_OK);
	assertTrue (verifier.verify("second.signer", issuedCert, messages) == BundleVerifier::BUNDLE_VERIFICATION_OK);
	assertTrue (verifier.verify("second.signer", signerCert, messages) == BundleVerifier::BUNDLE_VERIFICATION_FAILED);
}


void BundleVerifierTest::testVerifyFailAddedFileWithoutDirectoryEntry()
{
	signBundleFile();
	Poco::Zip::ZipManipulator manip(_bundleFile, false);
	manip.addFile("en/bundle.properties", writeWorkFile("bundle.properties", "foo: baz\n"));
	manip.commit();

	BundleVerifier verifier(_bundleFile);
	Poco::Crypto::X509Certificate cert(TestData::path("signer.pem"));
	std::vector<std::string> messages;
	assertTrue (verifier.verify("test.signer", cert, messages) == BundleVerifier::BUNDLE_VERIFICATION_FAILED);
	assertTrue (messages.size() == 1);
	assertEqual ("No signature found for file: en/bundle.properties", messages[0]);
}


void BundleVerifierTest::testVerifyFailReplacedFileWithoutDirectoryEntry()
{
	TestData::writeBundle(_bundleFile, {
		{"META-INF/manifest.mf", "Manifest-Version: 1.0\n"},
		{"bundle.properties", "foo: bar\n\n"},
		{"en/bundle.properties", "foo: baz\n"}
	});
	signBundleFile();
	Poco::Zip::ZipManipulator manip(_bundleFile, false);
	manip.replaceFile("en/bundle.properties", writeWorkFile("bundle.properties", "foo: changed\n"));
	manip.commit();

	BundleVerifier verifier(_bundleFile);
	std::vector<std::string> aliases;
	verifier.listSigners(aliases);
	assertTrue (aliases.size() == 1);
	assertEqual ("test.signer", aliases[0]);
	Poco::Crypto::X509Certificate cert(TestData::path("signer.pem"));
	std::vector<std::string> messages;
	assertTrue (verifier.verify("test.signer", cert, messages) == BundleVerifier::BUNDLE_VERIFICATION_FAILED);
	assertTrue (messages.size() == 1);
	assertEqual ("Signature mismatch for file: en/bundle.properties", messages[0]);
}


void BundleVerifierTest::testVerifyFailNonCanonicalEntryName()
{
	TestData::writeBundle(_bundleFile, {
		{"META-INF/manifest.mf", "Manifest-Version: 1.0\n"},
		{"aa/x.txt", "x"}
	});
	signBundleFile();
	TestData::renameEntry(_bundleFile, "aa/x.txt", "../x.txt");

	BundleVerifier verifier(_bundleFile);
	Poco::Crypto::X509Certificate cert(TestData::path("signer.pem"));
	std::vector<std::string> messages;
	assertTrue (verifier.verify("test.signer", cert, messages) == BundleVerifier::BUNDLE_VERIFICATION_FAILED);
	assertTrue (messages.size() == 1);
	assertMessageEqual ("Invalid entry name in bundle: ../x.txt", messages[0]);
}


void BundleVerifierTest::testVerifyTrustedSigner()
{
	signBundleFile("signer.pem", true);
	std::vector<std::string> messages;
	assertTrue (verifyBundleFile("signer.pem", messages) == BundleVerifier::BUNDLE_VERIFICATION_OK);
	assertTrue (messages.empty());
}


void BundleVerifierTest::testVerifyTrustedSignerWithoutCertificateInBundle()
{
	signBundleFile("signer.pem", false);
	std::vector<std::string> messages;
	assertTrue (verifyBundleFile("signer.pem", messages) == BundleVerifier::BUNDLE_VERIFICATION_OK);
	assertTrue (messages.empty());
	assertTrue (verifyBundleFile("foreign.pem", messages) == BundleVerifier::BUNDLE_VERIFICATION_FAILED);
	assertTrue (messages.size() == 1);
}


void BundleVerifierTest::testVerifyIssuedSigner()
{
	signBundleFile("issued.pem", true);
	std::vector<std::string> messages;
	assertTrue (verifyBundleFile("ca.pem", messages) == BundleVerifier::BUNDLE_VERIFICATION_OK);
	assertTrue (messages.empty());
}


void BundleVerifierTest::testVerifyFailResignedWithForeignKey()
{
	signBundleFile("signer.pem", true);
	signBundleFile("foreign.pem", true);

	BundleVerifier verifier(_bundleFile);
	BundleVerifier::CertificatePtr pCert = verifier.extractCertificate("test.signer");
	assertNotNull (pCert);
	Poco::Crypto::X509Certificate signerCert(TestData::path("signer.pem"));
	assertEqual (signerCert.subjectName(), pCert->subjectName());

	std::vector<std::string> messages;
	assertTrue (verifyBundleFile("signer.pem", messages) == BundleVerifier::BUNDLE_VERIFICATION_FAILED);
	assertTrue (messages.size() == 1);
}


void BundleVerifierTest::testVerifyFailExpiredSigner()
{
	signBundleFile("expired.pem", true);
	std::vector<std::string> messages;
	assertTrue (verifyBundleFile("expired.pem", messages) == BundleVerifier::BUNDLE_VERIFICATION_FAILED);
	assertTrue (messages.size() == 1);
}


void BundleVerifierTest::testVerifyFailSignerWithoutCodeSigning()
{
	signBundleFile("server.pem", true);
	std::vector<std::string> messages;
	assertTrue (verifyBundleFile("server.pem", messages) == BundleVerifier::BUNDLE_VERIFICATION_FAILED);
	assertTrue (messages.size() == 1);
}


void BundleVerifierTest::testVerifyFailUnsignedWithTrustedCertificate()
{
	std::vector<std::string> messages;
	assertTrue (verifyBundleFile("signer.pem", messages) == BundleVerifier::BUNDLE_VERIFICATION_NO_SIGNATURE);
}


void BundleVerifierTest::testVerifySHA1OnlyIfAllowed()
{
	Poco::Crypto::RSAKey rsaKey("", TestData::path("signer.pem"));
	if (!TestData::signsWithSHA1(rsaKey))
	{
		warnmsg("OpenSSL refuses SHA1 signatures, SHA1 case skipped.");
		return;
	}
	BundleSigner signer("test.signer", rsaKey, BundleSigner::SHA1);
	signer.signBundle(_bundleFile);

	BundleVerifier verifier(_bundleFile);
	Poco::Crypto::X509Certificate cert(TestData::path("signer.pem"));
	std::vector<std::string> messages;
	assertTrue (verifier.verify("test.signer", cert, messages) == BundleVerifier::BUNDLE_VERIFICATION_FAILED);
	assertTrue (messages.size() == 1);

	verifier.setAllowSHA1(true);
	assertTrue (verifier.verify("test.signer", cert, messages) == BundleVerifier::BUNDLE_VERIFICATION_OK);
	assertTrue (messages.empty());
}


void BundleVerifierTest::testVerifyBundleStorage()
{
	signBundleFile();

	BundleVerifier verifier(new Poco::OSP::BundleFile(_bundleFile));
	const Poco::Crypto::X509Certificate::List trustedCertificates{Poco::Crypto::X509Certificate(TestData::path("signer.pem"))};
	std::vector<std::string> messages;
	assertTrue (verifier.verify("test.signer", trustedCertificates, messages) == BundleVerifier::BUNDLE_VERIFICATION_OK);
	assertTrue (messages.empty());
}


void BundleVerifierTest::testVerifyTrustedCertificateWithoutRSAKeyIsSkipped()
{
	signBundleFile("signer.pem", false);

	BundleVerifier verifier(_bundleFile);
	const Poco::Crypto::X509Certificate::List trustedCertificates{
		Poco::Crypto::X509Certificate(TestData::path("ec.pem")),
		Poco::Crypto::X509Certificate(TestData::path("signer.pem"))
	};
	std::vector<std::string> messages;
	assertTrue (verifier.verify("test.signer", trustedCertificates, messages) == BundleVerifier::BUNDLE_VERIFICATION_OK);
	assertTrue (messages.empty());
}


void BundleVerifierTest::testVerifyFailCertificateWithoutRSAKey()
{
	Poco::Crypto::RSAKey rsaKey("", TestData::path("signer.pem"));
	Poco::Crypto::X509Certificate ecCert(TestData::path("ec.pem"));
	BundleSigner signer("test.signer", rsaKey, BundleSigner::SHA256);
	signer.signBundle(_bundleFile, ecCert);

	std::vector<std::string> messages;
	assertTrue (verifyBundleFile("ec.pem", messages) == BundleVerifier::BUNDLE_VERIFICATION_FAILED);
	assertTrue (messages.size() == 1);
}


void BundleVerifierTest::testVerifyWithManyTrustedCertificates()
{
	signBundleFile("signer.pem", false);

	constexpr std::size_t OTHER_CERTIFICATES = 50;
	const std::vector<std::string> otherFiles{"ec.pem", "foreign.pem", "expired.pem", "server.pem", "ca.pem", "issued.pem"};
	Poco::Crypto::X509Certificate::List trustedCertificates;
	while (trustedCertificates.size() < OTHER_CERTIFICATES)
	{
		trustedCertificates.emplace_back(TestData::path(otherFiles[trustedCertificates.size() % otherFiles.size()]));
	}
	trustedCertificates.emplace_back(TestData::path("signer.pem"));

	BundleVerifier verifier(_bundleFile);
	std::vector<std::string> messages;
	assertTrue (verifier.verify("test.signer", trustedCertificates, messages) == BundleVerifier::BUNDLE_VERIFICATION_OK);
	assertTrue (messages.empty());
}


void BundleVerifierTest::signBundleFile()
{
	signBundleFile("signer.pem", true);
}


void BundleVerifierTest::signBundleFile(const std::string& keyFile, bool includeCertificate)
{
	Poco::Crypto::RSAKey rsaKey("", TestData::path(keyFile));
	BundleSigner signer("test.signer", rsaKey, BundleSigner::SHA256);
	if (includeCertificate)
	{
		Poco::Crypto::X509Certificate cert(TestData::path(keyFile));
		signer.signBundle(_bundleFile, cert);
	}
	else
	{
		signer.signBundle(_bundleFile);
	}
}


BundleVerifier::VerificationResult BundleVerifierTest::verifyBundleFile(const std::string& trustedCertificateFile, std::vector<std::string>& messages) const
{
	BundleVerifier verifier(_bundleFile);
	const Poco::Crypto::X509Certificate::List trustedCertificates{Poco::Crypto::X509Certificate(TestData::path(trustedCertificateFile))};
	return verifier.verify("test.signer", trustedCertificates, messages);
}


std::string BundleVerifierTest::writeWorkFile(const std::string& name, const std::string& content) const
{
	Poco::Path p(_workDir);
	p.makeDirectory();
	p.setFileName(name);
	Poco::FileOutputStream ostr(p.toString());
	ostr << content;
	return p.toString();
}


void BundleVerifierTest::writeFile(const Poco::Path& path, const std::string& content)
{
	Poco::File(path.parent()).createDirectories();
	Poco::FileOutputStream ostr(path.toString());
	ostr << content;
}


void BundleVerifierTest::signBundleDirectory(const std::string& digestAlgorithm)
{
	std::ifstream bundleStream(_bundleFile, std::ios::binary);
	Poco::Zip::Decompress decompress(bundleStream, _bundleDirectory);
	decompress.decompressAllFiles();
	bundleStream.close();

	Poco::Crypto::RSAKey rsaKey("", TestData::path("signer.pem"));
	Poco::Crypto::X509Certificate cert(TestData::path("signer.pem"));
	BundleSigner signer("test.signer", rsaKey, digestAlgorithm);
	signer.signBundle(_bundleDirectory, cert);
}


std::string BundleVerifierTest::bundleDirectoryFile(const std::string& path) const
{
	Poco::Path p(_bundleDirectory);
	p.makeDirectory();
	p.append(Poco::Path(path, Poco::Path::PATH_UNIX));
	return p.toString();
}


void BundleVerifierTest::setUp()
{
	_workDir = TestData::createWorkDirectory();
	Poco::Path workPath(_workDir);
	workPath.makeDirectory();
	_bundleFile = Poco::Path(workPath, "testBundle.zip").toString();
	_bundleDirectory = Poco::Path(workPath, "testBundle").toString();
	TestData::writeTestBundle(_bundleFile);
}


void BundleVerifierTest::tearDown()
{
	Poco::File(_workDir).remove(true);
}


CppUnit::Test* BundleVerifierTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("BundleVerifierTest");

	CppUnit_addTest(pSuite, BundleVerifierTest, testVerifyFile);
	CppUnit_addTest(pSuite, BundleVerifierTest, testVerifyFileSHA256);
	CppUnit_addTest(pSuite, BundleVerifierTest, testVerifyDirectory);
	CppUnit_addTest(pSuite, BundleVerifierTest, testVerifyFailModifyFile);
	CppUnit_addTest(pSuite, BundleVerifierTest, testVerifyFailRemoveFile);
	CppUnit_addTest(pSuite, BundleVerifierTest, testVerifyFailAddFile);
	CppUnit_addTest(pSuite, BundleVerifierTest, testVerifyFailFileRenamedInLetterCase);
	CppUnit_addTest(pSuite, BundleVerifierTest, testVerifyFailModifyFileInDirectoryNamedTilde);
	CppUnit_addTest(pSuite, BundleVerifierTest, testVerifyFailModifyFileInDirectoryNamedLikeDevice);
	CppUnit_addTest(pSuite, BundleVerifierTest, testVerifyFailBadSignature);
	CppUnit_addTest(pSuite, BundleVerifierTest, testVerifyFailMissingSignature);
	CppUnit_addTest(pSuite, BundleVerifierTest, testVerifyFailUnsupportedSignatureType);
	CppUnit_addTest(pSuite, BundleVerifierTest, testVerifyFailMalformedSignatureBlockFile);
	CppUnit_addTest(pSuite, BundleVerifierTest, testVerifyFailSignatureNotHexadecimal);
	CppUnit_addTest(pSuite, BundleVerifierTest, testVerifyFailOversizedSignatureBlockFile);
	CppUnit_addTest(pSuite, BundleVerifierTest, testVerifyFailSignatureFileWithMoreEntriesThanFiles);
	CppUnit_addTest(pSuite, BundleVerifierTest, testVerifyFailSignatureFileLargerThanSignerWrites);
	CppUnit_addTest(pSuite, BundleVerifierTest, testVerifyBundleWithManyFiles);
	CppUnit_addTest(pSuite, BundleVerifierTest, testVerifyFailSignatureBlockFileWithoutSignatureFileEntry);
	CppUnit_addTest(pSuite, BundleVerifierTest, testVerifyFailMissingSignatureFile);
	CppUnit_addTest(pSuite, BundleVerifierTest, testVerifyFailContentAfterLastSignatureFileEntry);
	CppUnit_addTest(pSuite, BundleVerifierTest, testVerifyFailSignatureFileWithCRLF);
	CppUnit_addTest(pSuite, BundleVerifierTest, testVerifySHA2DigestAlgorithms);
	CppUnit_addTest(pSuite, BundleVerifierTest, testVerifyTwoSigners);
	CppUnit_addTest(pSuite, BundleVerifierTest, testVerifyFailAddedFileWithoutDirectoryEntry);
	CppUnit_addTest(pSuite, BundleVerifierTest, testVerifyFailReplacedFileWithoutDirectoryEntry);
	CppUnit_addTest(pSuite, BundleVerifierTest, testVerifyFailNonCanonicalEntryName);
	CppUnit_addTest(pSuite, BundleVerifierTest, testVerifyTrustedSigner);
	CppUnit_addTest(pSuite, BundleVerifierTest, testVerifyTrustedSignerWithoutCertificateInBundle);
	CppUnit_addTest(pSuite, BundleVerifierTest, testVerifyIssuedSigner);
	CppUnit_addTest(pSuite, BundleVerifierTest, testVerifyFailResignedWithForeignKey);
	CppUnit_addTest(pSuite, BundleVerifierTest, testVerifyFailExpiredSigner);
	CppUnit_addTest(pSuite, BundleVerifierTest, testVerifyFailSignerWithoutCodeSigning);
	CppUnit_addTest(pSuite, BundleVerifierTest, testVerifyFailUnsignedWithTrustedCertificate);
	CppUnit_addTest(pSuite, BundleVerifierTest, testVerifySHA1OnlyIfAllowed);
	CppUnit_addTest(pSuite, BundleVerifierTest, testVerifyBundleStorage);
	CppUnit_addTest(pSuite, BundleVerifierTest, testVerifyTrustedCertificateWithoutRSAKeyIsSkipped);
	CppUnit_addTest(pSuite, BundleVerifierTest, testVerifyFailCertificateWithoutRSAKey);
	CppUnit_addTest(pSuite, BundleVerifierTest, testVerifyWithManyTrustedCertificates);

	return pSuite;
}
