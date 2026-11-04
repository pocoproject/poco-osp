//
// BundleVerifierTest.h
//
// Definition of the BundleVerifierTest class.
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties, 
// copied or duplicated in any form, in whole or in part.
//


#ifndef BundleVerifierTest_INCLUDED
#define BundleVerifierTest_INCLUDED


#include "Poco/OSP/BundleSign/BundleSign.h"
#include "Poco/OSP/BundleSign/BundleVerifier.h"
#include "Poco/Path.h"
#include "CppUnit/TestCase.h"


class BundleVerifierTest: public CppUnit::TestCase
{
public:
	BundleVerifierTest(const std::string& name);
	~BundleVerifierTest() override;

	void testVerifyFile();
	void testVerifyFileSHA256();
	void testVerifyDirectory();
	void testVerifyFailModifyFile();
	void testVerifyFailRemoveFile();
	void testVerifyFailAddFile();
	void testVerifyFailFileRenamedInLetterCase();
	void testVerifyFailModifyFileInDirectoryNamedTilde();
	void testVerifyFailModifyFileInDirectoryNamedLikeDevice();
	void testVerifyFailBadSignature();
	void testVerifyFailMissingSignature();
	void testVerifyFailUnsupportedSignatureType();
	void testVerifyFailMalformedSignatureBlockFile();
	void testVerifyFailSignatureNotHexadecimal();
	void testVerifyFailOversizedSignatureBlockFile();
	void testVerifyFailSignatureFileWithMoreEntriesThanFiles();
	void testVerifyFailSignatureFileLargerThanSignerWrites();
	void testVerifyBundleWithManyFiles();
	void testVerifyFailSignatureBlockFileWithoutSignatureFileEntry();
	void testVerifyFailMissingSignatureFile();
	void testVerifyFailContentAfterLastSignatureFileEntry();
	void testVerifyFailSignatureFileWithCRLF();
	void testVerifySHA2DigestAlgorithms();
	void testVerifyTwoSigners();
	void testVerifyFailAddedFileWithoutDirectoryEntry();
	void testVerifyFailReplacedFileWithoutDirectoryEntry();
	void testVerifyFailNonCanonicalEntryName();
	void testVerifyTrustedSigner();
	void testVerifyTrustedSignerWithoutCertificateInBundle();
	void testVerifyIssuedSigner();
	void testVerifyFailResignedWithForeignKey();
	void testVerifyFailExpiredSigner();
	void testVerifyFailSignerWithoutCodeSigning();
	void testVerifyFailUnsignedWithTrustedCertificate();
	void testVerifySHA1OnlyIfAllowed();
	void testVerifyBundleStorage();
	void testVerifyTrustedCertificateWithoutRSAKeyIsSkipped();
	void testVerifyFailCertificateWithoutRSAKey();
	void testVerifyWithManyTrustedCertificates();

	void setUp() override;
	void tearDown() override;

	static CppUnit::Test* suite();

private:
	void signBundleDirectory(const std::string& digestAlgorithm);
	void signBundleFile();
	void signBundleFile(const std::string& keyFile, bool includeCertificate);
	[[nodiscard]] Poco::OSP::BundleSign::BundleVerifier::VerificationResult verifyBundleFile(const std::string& trustedCertificateFile, std::vector<std::string>& messages) const;
	[[nodiscard]] std::string writeWorkFile(const std::string& name, const std::string& content) const;
	static void writeFile(const Poco::Path& path, const std::string& content);
	[[nodiscard]] std::string bundleDirectoryFile(const std::string& path) const;

	std::string _workDir;
	std::string _bundleFile;
	std::string _bundleDirectory;
};


#endif // BundleVerifierTest_INCLUDED
