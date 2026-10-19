//
// BundleVerifierTest.cpp
//
// Copyright (c) 2007, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties, 
// copied or duplicated in any form, in whole or in part.
//


#include "BundleVerifierTest.h"
#include "Poco/OSP/BundleSign/BundleVerifier.h"
#include "Poco/OSP/BundleSign/BundleSigner.h"
#include "Poco/Crypto/RSAKey.h"
#include "Poco/Zip/Decompress.h"
#include "Poco/File.h"
#include "Poco/Path.h"
#include "Poco/FileStream.h"
#include "Poco/SharedPtr.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include <sstream>
#include <fstream>


using namespace Poco::OSP::BundleSign;
using namespace Poco::OSP;


static const std::string APPINF_PEM(
	"-----BEGIN CERTIFICATE-----\n"
	"MIIESzCCAzOgAwIBAgIBATALBgkqhkiG9w0BAQUwgdMxEzARBgNVBAMMCmFwcGlu\n"
	"Zi5jb20xNjA0BgNVBAoMLUFwcGxpZWQgSW5mb3JtYXRpY3MgU29mdHdhcmUgRW5n\n"
	"aW5lZXJpbmcgR21iSDEUMBIGA1UECwwLRGV2ZWxvcG1lbnQxEjAQBgNVBAgMCUNh\n"
	"cmludGhpYTELMAkGA1UEBhMCQVQxHjAcBgNVBAcMFVN0LiBKYWtvYiBpbSBSb3Nl\n"
	"bnRhbDEtMCsGCSqGSIb3DQEJARYeZ3VlbnRlci5vYmlsdHNjaG5pZ0BhcHBpbmYu\n"
	"Y29tMB4XDTA5MDUwNzE0NTY1NloXDTI5MDUwMjE0NTY1NlowgdMxEzARBgNVBAMM\n"
	"CmFwcGluZi5jb20xNjA0BgNVBAoMLUFwcGxpZWQgSW5mb3JtYXRpY3MgU29mdHdh\n"
	"cmUgRW5naW5lZXJpbmcgR21iSDEUMBIGA1UECwwLRGV2ZWxvcG1lbnQxEjAQBgNV\n"
	"BAgMCUNhcmludGhpYTELMAkGA1UEBhMCQVQxHjAcBgNVBAcMFVN0LiBKYWtvYiBp\n"
	"bSBSb3NlbnRhbDEtMCsGCSqGSIb3DQEJARYeZ3VlbnRlci5vYmlsdHNjaG5pZ0Bh\n"
	"cHBpbmYuY29tMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA89GolWCR\n"
	"KtLQclJ2M2QtpFqzNC54hUQdR6n8+DAeruH9WFwLSdWW2fEi+jrtd/WEWCdt4PxX\n"
	"F2/eBYeURus7Hg2ZtJGDd3je0+Ygsv7+we4cMN/knaBY7rATqhmnZWk+yBpkf5F2\n"
	"IHp9gBxUaJWmt/bq3XrvTtzrDXpCd4zg4zPXZ8IC8ket5o3K2vnkAOsIsgN+Ffqd\n"
	"4GjF4dsblG6u6E3VarGRLwGtgB8BAZOA/33mV4FHSMkc4OXpAChaK3tM8YhrLw+m\n"
	"XtsfqDiv1825S6OWFCKGj/iX8X2QAkrdB63vXCSpb3de/ByIUfp31PpMlMh6dKo1\n"
	"vf7yj0nb2w0utQIDAQABoyowKDAOBgNVHQ8BAf8EBAMCB4AwFgYDVR0lAQH/BAww\n"
	"CgYIKwYBBQUHAwMwDQYJKoZIhvcNAQEFBQADggEBAM0cpfb4BgiU/rkYe121P581\n"
	"ftg5Ck1PYYda1Fy/FgzbgJh2AwVo/6sn6GF79/QkEcWEgtCMNNO3LMTTddUUApuP\n"
	"jnEimyfmUhIThyud/vryzTMNa/eZMwaAqUQWqLf+AwgqjUsBSMenbSHavzJOpsvR\n"
	"LI0PQ1VvqB+3UGz0JUnBJiKvHs83Fdm4ewPAf3M5fGcIa+Fl2nU5Plzwzskj84f6\n"
	"73ZlEEi3aW9JieNy7RWsMM+1E8Sj2CGRZC4BM9V1Fgnsh4+VHX8Eu7eHucvfeIYx\n"
	"3mmLMoK4sCayL/FGhrUDw5AkWb8tKNpRXY+W60Et281yxQSeWLPIbatVzIWI0/M=\n"
	"-----END CERTIFICATE-----\n"
	"-----BEGIN RSA PRIVATE KEY-----\n"
	"MIIEowIBAAKCAQEA89GolWCRKtLQclJ2M2QtpFqzNC54hUQdR6n8+DAeruH9WFwL\n"
	"SdWW2fEi+jrtd/WEWCdt4PxXF2/eBYeURus7Hg2ZtJGDd3je0+Ygsv7+we4cMN/k\n"
	"naBY7rATqhmnZWk+yBpkf5F2IHp9gBxUaJWmt/bq3XrvTtzrDXpCd4zg4zPXZ8IC\n"
	"8ket5o3K2vnkAOsIsgN+Ffqd4GjF4dsblG6u6E3VarGRLwGtgB8BAZOA/33mV4FH\n"
	"SMkc4OXpAChaK3tM8YhrLw+mXtsfqDiv1825S6OWFCKGj/iX8X2QAkrdB63vXCSp\n"
	"b3de/ByIUfp31PpMlMh6dKo1vf7yj0nb2w0utQIDAQABAoIBAC0YFL7z6kVqUiUi\n"
	"vTvKBmsEX2qCQcYcD/JGLURV2gbsQygJdgkE2O6cG6T+KxdpsD2BpV/cK9to1D5f\n"
	"kfv/26I2T7xGwXLiRwwX3upcIRALEumzLxw8INaceDaYog4uhe8KepXheGCPZxC/\n"
	"rJjfPMN/hnbKGuKSF2MZvptP0fQTMQ3vayi804Wswk43/dSAN64cJwsCN9GavKrf\n"
	"1ZehGi1Lw8NBUnbWTbWAnq4eXW+FDBHIP98EIm9gqwnzr3FqLPlAAFvbfGCeFRbi\n"
	"FL7ofUNYvkLHwWoyebHY4hP0q9S9kMvXDWtHWYk8oOqx9/0yh5kdK7ILIx6jhHcP\n"
	"4FxqsYECgYEA/8xOS2NIFD9r+CjcIELohHBHEINyJUP/0Xti+I+j3hCVJmhdrnhG\n"
	"QTXCjdTNrKiOY50qWpAMVIuZnr4n1vlm4NueD0sFai7HXy3RN+KDdb4clLx963qd\n"
	"+GXNaZtTICHM/YPxbZNzhahy3d1GUyb7lGEt/a0tUWl9giWmv+Zwm1UCgYEA9ALu\n"
	"jRUMu0l6fJwPcah3TvaSes+8Aiv0EZ4yAd7+ROQDw+r3fWrJxfi+vYDEfLavZdwl\n"
	"gqZHMigTvfaWRMhmyJAOGlYND256Zy6UFShFCyTP/AJLlE4Fs1QhGnOU9mEAh4mC\n"
	"QsJAyP0GBaAr/Kcu66iH3P/G9SWKpTjyjhrcBeECgYEAq9qZJ3D2NhQ996WL7mjF\n"
	"SmUceW22PG8o6ZZZ8eCfGUxOyFUJy3Omz5zMrvkvjTZZI3etwtZQUmTPuqTKo1U/\n"
	"1wX1dRTT+HihHcLscv1NkMc+8HaKqq/gfcf3b2IZQNc0n4YLp7COxldbhM9qtebU\n"
	"0uVPPsUgUzfqSIv/MaIiOVkCgYAOCWUjpIRY+b8cD/YreZ9zYWfHvM956IFfyhPi\n"
	"W6uQ1vUIfinCgzQcOVTvncqvyeqJF4Lo4xvRXNDV6qt2RXj0LKVDCpQxwQ81hE5s\n"
	"cSaXn4R/YLpsS5i0WjCVTvTQv3/CTDy8DmSfqz5/cFlXbV6bm9RE+Em/rGsvWflK\n"
	"B22EwQKBgDwsj7myJW+FubsKB7v+1+60fxIebZjui2hJ/1kj/oELnMwUDJOMSFM9\n"
	"VpsYtRjTQFRgffd8IPbrE3PqKl20sN9XhmZZdzhQ6gxrunXNOvDYmlC++mvaz9Hz\n"
	"L31sbcb8eK6Z7p0gMBJc7cDEAaI5aApJyOBFSY474jpfK3xWNfxv\n"
	"-----END RSA PRIVATE KEY-----\n"
);


BundleVerifierTest::BundleVerifierTest(const std::string& name): CppUnit::TestCase(name)
{
}


BundleVerifierTest::~BundleVerifierTest()
{
}


void BundleVerifierTest::testVerifyFile()
{
	BundleVerifier verifier1("testBundle.zip");
	std::vector<std::string> aliases;
	verifier1.listSigners(aliases);
	assert (aliases.empty());

	BundleVerifier::CertificatePtr pCert = verifier1.extractCertificate("appinf.com");
	assertNull (pCert);

	std::istringstream istr(APPINF_PEM);
	Poco::Crypto::RSAKey rsaKey(0, &istr, "secret");
	istr.clear();
	istr.seekg(0);
	Poco::Crypto::X509Certificate cert(istr);
	BundleSigner signer("appinf.com", rsaKey);
	signer.signBundle("testBundle.zip", cert);	
	
	BundleVerifier verifier2("testBundle.zip");
	verifier2.listSigners(aliases);
	assert (aliases.size() == 1);
	assert (aliases[0] == "appinf.com");
	
	pCert = verifier2.extractCertificate("appinf.com");
	assertNotNull (pCert);
	
	std::vector<std::string> messages;
	BundleVerifier::VerificationResult result = verifier2.verify("appinf.com", *pCert, messages);
	assert (result == BundleVerifier::BUNDLE_VERIFICATION_OK);
	assert (messages.empty());
}


void BundleVerifierTest::testVerifyFileSHA256()
{
	BundleVerifier verifier1("testBundle.zip");
	std::vector<std::string> aliases;
	verifier1.listSigners(aliases);
	assert (aliases.empty());

	BundleVerifier::CertificatePtr pCert = verifier1.extractCertificate("appinf.com");
	assertNull (pCert);

	std::istringstream istr(APPINF_PEM);
	Poco::Crypto::RSAKey rsaKey(0, &istr, "secret");
	istr.clear();
	istr.seekg(0);
	Poco::Crypto::X509Certificate cert(istr);
	BundleSigner signer("appinf.com", rsaKey, "SHA256");
	signer.signBundle("testBundle.zip", cert);	
	
	BundleVerifier verifier2("testBundle.zip");
	verifier2.listSigners(aliases);
	assert (aliases.size() == 1);
	assert (aliases[0] == "appinf.com");
	
	pCert = verifier2.extractCertificate("appinf.com");
	assertNotNull (pCert);
	
	std::vector<std::string> messages;
	BundleVerifier::VerificationResult result = verifier2.verify("appinf.com", *pCert, messages);
	assert (result == BundleVerifier::BUNDLE_VERIFICATION_OK);
	assert (messages.empty());
}


void BundleVerifierTest::testVerifyDirectory()
{
	Poco::File bundleFile("testBundle");
	if (bundleFile.exists())
	{
		bundleFile.remove(true);
	}

	std::ifstream bundleStream("testBundle.zip", std::ios::binary);
	Poco::Zip::Decompress decompress(bundleStream, "testBundle");
	decompress.decompressAllFiles();
	bundleStream.close();

	BundleVerifier verifier1("testBundle");
	std::vector<std::string> aliases;
	verifier1.listSigners(aliases);
	assert (aliases.empty());

	BundleVerifier::CertificatePtr pCert = verifier1.extractCertificate("appinf.com");
	assertNull (pCert);
	
	std::istringstream istr(APPINF_PEM);
	Poco::Crypto::RSAKey rsaKey(0, &istr, "secret");
	istr.clear();
	istr.seekg(0);
	Poco::Crypto::X509Certificate cert(istr);
	BundleSigner signer("appinf.com", rsaKey);
	signer.signBundle("testBundle", cert);	

	BundleVerifier verifier2("testBundle");
	verifier2.listSigners(aliases);
	assert (aliases.size() == 1);
	assert (aliases[0] == "appinf.com");
	
	pCert = verifier2.extractCertificate("appinf.com");
	assertNotNull (pCert);
	
	std::vector<std::string> messages;
	BundleVerifier::VerificationResult result = verifier2.verify("appinf.com", *pCert, messages);
	assert (result == BundleVerifier::BUNDLE_VERIFICATION_OK);
	assert (messages.empty());

	bundleFile.remove(true);
}


void BundleVerifierTest::testVerifyFailModifyFile()
{
	Poco::File bundleFile("testBundle");
	if (bundleFile.exists())
	{
		bundleFile.remove(true);
	}

	std::ifstream bundleStream("testBundle.zip", std::ios::binary);
	Poco::Zip::Decompress decompress(bundleStream, "testBundle");
	decompress.decompressAllFiles();
	bundleStream.close();

	std::istringstream istr(APPINF_PEM);
	Poco::Crypto::RSAKey rsaKey(0, &istr, "secret");
	istr.clear();
	istr.seekg(0);
	Poco::Crypto::X509Certificate cert(istr);
	BundleSigner signer("appinf.com", rsaKey);
	signer.signBundle("testBundle", cert);	

	// tamper with manifest file
	Poco::Path manifPath("testBundle");
	manifPath.makeDirectory();
	manifPath.pushDirectory("META-INF");
	manifPath.setFileName("manifest.mf");
	Poco::FileOutputStream manifStream(manifPath.toString());
	manifStream << 
		"Manifest-Version: 1.0\n"
		"Bundle-Name: Bad Bundle\n";
	manifStream.close();

	BundleVerifier verifier("testBundle");	
	BundleVerifier::CertificatePtr pCert = verifier.extractCertificate("appinf.com");
	assertNotNull (pCert);
	
	std::vector<std::string> messages;
	BundleVerifier::VerificationResult result = verifier.verify("appinf.com", *pCert, messages);
	assert (result == BundleVerifier::BUNDLE_VERIFICATION_FAILED);
	assert (messages.size() == 1);
	
	bundleFile.remove(true);
}


void BundleVerifierTest::testVerifyFailRemoveFile()
{
	Poco::File bundleFile("testBundle");
	if (bundleFile.exists())
	{
		bundleFile.remove(true);
	}

	std::ifstream bundleStream("testBundle.zip", std::ios::binary);
	Poco::Zip::Decompress decompress(bundleStream, "testBundle");
	decompress.decompressAllFiles();
	bundleStream.close();

	std::istringstream istr(APPINF_PEM);
	Poco::Crypto::RSAKey rsaKey(0, &istr, "secret");
	istr.clear();
	istr.seekg(0);
	Poco::Crypto::X509Certificate cert(istr);
	BundleSigner signer("appinf.com", rsaKey);
	signer.signBundle("testBundle", cert);	

	// remove a file
	Poco::Path propsPath("testBundle");
	propsPath.makeDirectory();
	propsPath.setFileName("bundle.properties");
	Poco::File propsFile(propsPath.toString());
	propsFile.remove();

	BundleVerifier verifier("testBundle");	
	BundleVerifier::CertificatePtr pCert = verifier.extractCertificate("appinf.com");
	assertNotNull (pCert);
	
	std::vector<std::string> messages;
	BundleVerifier::VerificationResult result = verifier.verify("appinf.com", *pCert, messages);
	assert (result == BundleVerifier::BUNDLE_VERIFICATION_FAILED);
	assert (messages.size() == 1);
	
	bundleFile.remove(true);
}


void BundleVerifierTest::testVerifyFailAddFile()
{
	Poco::File bundleFile("testBundle");
	if (bundleFile.exists())
	{
		bundleFile.remove(true);
	}

	std::ifstream bundleStream("testBundle.zip", std::ios::binary);
	Poco::Zip::Decompress decompress(bundleStream, "testBundle");
	decompress.decompressAllFiles();
	bundleStream.close();

	std::istringstream istr(APPINF_PEM);
	Poco::Crypto::RSAKey rsaKey(0, &istr, "secret");
	istr.clear();
	istr.seekg(0);
	Poco::Crypto::X509Certificate cert(istr);
	BundleSigner signer("appinf.com", rsaKey);
	signer.signBundle("testBundle", cert);	

	// add a file
	Poco::Path filePath("testBundle");
	filePath.makeDirectory();
	filePath.setFileName("someAddedFile");
	Poco::FileOutputStream fileStream(filePath.toString());
	fileStream << "This is a file that does not belong into that bundle.";
	fileStream.close();

	BundleVerifier verifier("testBundle");	
	BundleVerifier::CertificatePtr pCert = verifier.extractCertificate("appinf.com");
	assertNotNull (pCert);
	
	std::vector<std::string> messages;
	BundleVerifier::VerificationResult result = verifier.verify("appinf.com", *pCert, messages);
	assert (result == BundleVerifier::BUNDLE_VERIFICATION_FAILED);
	assert (messages.size() == 1);
	
	bundleFile.remove(true);
}


void BundleVerifierTest::testVerifyFailBadSignature()
{
	Poco::File bundleFile("testBundle");
	if (bundleFile.exists())
	{
		bundleFile.remove(true);
	}

	std::ifstream bundleStream("testBundle.zip", std::ios::binary);
	Poco::Zip::Decompress decompress(bundleStream, "testBundle");
	decompress.decompressAllFiles();
	bundleStream.close();

	std::istringstream istr(APPINF_PEM);
	Poco::Crypto::RSAKey rsaKey(0, &istr, "secret");
	istr.clear();
	istr.seekg(0);
	Poco::Crypto::X509Certificate cert(istr);
	BundleSigner signer("appinf.com", rsaKey);
	signer.signBundle("testBundle", cert);	

	// tamper with signature file
	Poco::Path sfPath("testBundle");
	sfPath.makeDirectory();
	sfPath.pushDirectory("META-INF");
	sfPath.setFileName("appinf.com.sf");
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

	BundleVerifier verifier("testBundle");	
	BundleVerifier::CertificatePtr pCert = verifier.extractCertificate("appinf.com");
	assertNotNull (pCert);
	
	std::vector<std::string> messages;
	BundleVerifier::VerificationResult result = verifier.verify("appinf.com", *pCert, messages);
	assert (result == BundleVerifier::BUNDLE_VERIFICATION_FAILED);
	assert (messages.size() == 1);
	
	bundleFile.remove(true);
}


void BundleVerifierTest::testVerifyFailMissingSignature()
{
	Poco::File bundleFile("testBundle");
	if (bundleFile.exists())
	{
		bundleFile.remove(true);
	}

	std::ifstream bundleStream("testBundle.zip", std::ios::binary);
	Poco::Zip::Decompress decompress(bundleStream, "testBundle");
	decompress.decompressAllFiles();
	bundleStream.close();

	std::istringstream istr(APPINF_PEM);
	Poco::Crypto::RSAKey rsaKey(0, &istr, "secret");
	istr.clear();
	istr.seekg(0);
	Poco::Crypto::X509Certificate cert(istr);
	BundleSigner signer("appinf.com", rsaKey);
	signer.signBundle("testBundle", cert);	

	// remove rsa signature file
	Poco::Path rsaPath("testBundle");
	rsaPath.makeDirectory();
	rsaPath.pushDirectory("META-INF");
	rsaPath.setFileName("appinf.com.rsa");
	Poco::File rsaFile(rsaPath.toString());
	rsaFile.remove();

	BundleVerifier verifier("testBundle");	
	
	std::vector<std::string> messages;
	BundleVerifier::VerificationResult result = verifier.verify("appinf.com", cert, messages);
	assert (result == BundleVerifier::BUNDLE_VERIFICATION_NO_SIGNATURE);
	assert (messages.size() == 0);
	
	bundleFile.remove(true);
}


void BundleVerifierTest::setUp()
{
	// The following is a ZIP file containing the same
	// directory hierarchy as is used in BundleDirectoryTest.
	static const unsigned char TEST_BUNDLE_ZIP[] = 
	{
		0x50, 0x4b, 0x03, 0x04, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x76, 0x83, 0x36, 0xe8, 0x16,
		0xfa, 0x93, 0x0a, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x62, 0x75,
		0x6e, 0x64, 0x6c, 0x65, 0x2e, 0x70, 0x72, 0x6f, 0x70, 0x65, 0x72, 0x74, 0x69, 0x65, 0x73, 0x66,
		0x6f, 0x6f, 0x3a, 0x20, 0x62, 0x61, 0x72, 0x0a, 0x0a, 0x50, 0x4b, 0x03, 0x04, 0x0a, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x11, 0x76, 0x83, 0x36, 0x7a, 0xdf, 0x5a, 0x20, 0x16, 0x00, 0x00, 0x00, 0x16,
		0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x4d, 0x45, 0x54, 0x41, 0x2d, 0x49, 0x4e, 0x46, 0x2f,
		0x6d, 0x61, 0x6e, 0x69, 0x66, 0x65, 0x73, 0x74, 0x2e, 0x6d, 0x66, 0x4d, 0x61, 0x6e, 0x69, 0x66,
		0x65, 0x73, 0x74, 0x2d, 0x56, 0x65, 0x72, 0x73, 0x69, 0x6f, 0x6e, 0x3a, 0x20, 0x31, 0x2e, 0x30,
		0x0a, 0x50, 0x4b, 0x03, 0x04, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x76, 0x83, 0x36, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x4d,
		0x45, 0x54, 0x41, 0x2d, 0x49, 0x4e, 0x46, 0x2f, 0x50, 0x4b, 0x01, 0x02, 0x14, 0x00, 0x0a, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x11, 0x76, 0x83, 0x36, 0xe8, 0x16, 0xfa, 0x93, 0x0a, 0x00, 0x00, 0x00,
		0x0a, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x20, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x62, 0x75, 0x6e, 0x64, 0x6c, 0x65, 0x2e, 0x70, 0x72, 0x6f,
		0x70, 0x65, 0x72, 0x74, 0x69, 0x65, 0x73, 0x50, 0x4b, 0x01, 0x02, 0x14, 0x00, 0x0a, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x11, 0x76, 0x83, 0x36, 0x7a, 0xdf, 0x5a, 0x20, 0x16, 0x00, 0x00, 0x00, 0x16,
		0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x20, 0x00, 0x00,
		0x00, 0x39, 0x00, 0x00, 0x00, 0x4d, 0x45, 0x54, 0x41, 0x2d, 0x49, 0x4e, 0x46, 0x2f, 0x6d, 0x61,
		0x6e, 0x69, 0x66, 0x65, 0x73, 0x74, 0x2e, 0x6d, 0x66, 0x50, 0x4b, 0x01, 0x02, 0x14, 0x00, 0x0a,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x76, 0x83, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10,
		0x00, 0x00, 0x00, 0x81, 0x00, 0x00, 0x00, 0x4d, 0x45, 0x54, 0x41, 0x2d, 0x49, 0x4e, 0x46, 0x2f,
		0x50, 0x4b, 0x05, 0x06, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x03, 0x00, 0xb8, 0x00, 0x00, 0x00,
		0xa8, 0x00, 0x00, 0x00, 0x00, 0x00
	};

	std::ofstream ostr("testBundle.zip", std::ios::binary);
	ostr.write(reinterpret_cast<const char*>(&TEST_BUNDLE_ZIP[0]), sizeof(TEST_BUNDLE_ZIP));
}


void BundleVerifierTest::tearDown()
{
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
	CppUnit_addTest(pSuite, BundleVerifierTest, testVerifyFailBadSignature);
	CppUnit_addTest(pSuite, BundleVerifierTest, testVerifyFailMissingSignature);

	return pSuite;
}
