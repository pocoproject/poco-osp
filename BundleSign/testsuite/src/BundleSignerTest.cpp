//
// BundleSignerTest.cpp
//
// Copyright (c) 2007, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties, 
// copied or duplicated in any form, in whole or in part.
//


#include "BundleSignerTest.h"
#include "Poco/OSP/BundleSign/BundleSigner.h"
#include "Poco/OSP/BundleFile.h"
#include "Poco/OSP/BundleDirectory.h"
#include "Poco/Crypto/RSAKey.h"
#include "Poco/Zip/Decompress.h"
#include "Poco/File.h"
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


static const std::string APPINF_RSA_NO_CERT(
	"Signature-Version: 1.0\n"
	"Created-By: OSP BundleSigner\n"
	"\n"
	"Name: META-INF/appinf.com.sf\n"
	"RSA-SHA1-Signature: 6dee5b161141f8313ceed31c5f2f33a37a506bd572b577f091500d9777f67ed0066d8d12045145c54e3a77a99a852b795af8d57039cee6c01bc48c1547c4b0a56012b2a69d6ea03fa420752c27ebcfd173d7239749861f38c68f865271f1aaf54515cd43a70d41bb78a27dfdd46e7ec1be9b4c8073c1a801212712accf44c77e51a7bad937342266a2d8b4e13e135e493da0b01c328f339776fdbdab70bf0862fba581b8021fb58367c3eb660cb65f42735bb3284c970dec6a35e2711a6a8ec47ba74d43db72815c34a0cdc653d45df8a3a0263e4816841daac09fe290848d7c842be7902cc891ac2f26f5e0e91d12e9abd85992a5dab9965efc52d2895140ae\n"
	"\n"
);


static const std::string APPINF_RSA_SHA256_NO_CERT(
	"Signature-Version: 1.0\n"
	"Created-By: OSP BundleSigner\n"
	"\n"
	"Name: META-INF/appinf.com.sf\n"
	"RSA-SHA256-Signature: 3be79ebbd3d614c7bfcf8d76cb714c2734f6ab22c5998e78adb45f6823cb7e3c891ae4dc05082bb5580526e9d61fac753490af3480dde949907ace1f42b8905ae9b13c6000066fe1e41745ea0c6b96d1211575ea6b4832a6a8c394f154c54e574270316ee9f58625f4495afc49f88c1b412ce5562d7bf1d6b72408a88c39accac8c0118648aa278e22a23703d013791473f27cb6ee0007d2caed0714d85143408e9802aa2ebc00d00d58c09271c0eeb3bd765f41d92b1aec99d3fbcdae0ff538e7b8ace220d79c9f88b2a796f8e4dd5da1b09527dd93cd96946a95f73fc87ad72bb20d12fef3a7bef69f1b4cb4accaa872e12c2f8418524496d887e5fc85940f\n"
	"\n"
);


static const std::string APPINF_RSA(
	"Signature-Version: 1.0\n"
	"Created-By: OSP BundleSigner\n"
	"\n"
	"Name: META-INF/appinf.com.sf\n"
	"RSA-SHA1-Signature: 6dee5b161141f8313ceed31c5f2f33a37a506bd572b577f091500d9777f67ed0066d8d12045145c54e3a77a99a852b795af8d57039cee6c01bc48c1547c4b0a56012b2a69d6ea03fa420752c27ebcfd173d7239749861f38c68f865271f1aaf54515cd43a70d41bb78a27dfdd46e7ec1be9b4c8073c1a801212712accf44c77e51a7bad937342266a2d8b4e13e135e493da0b01c328f339776fdbdab70bf0862fba581b8021fb58367c3eb660cb65f42735bb3284c970dec6a35e2711a6a8ec47ba74d43db72815c34a0cdc653d45df8a3a0263e4816841daac09fe290848d7c842be7902cc891ac2f26f5e0e91d12e9abd85992a5dab9965efc52d2895140ae\n"
	"\n"
	"X509-Certificate: appinf.com\n"
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
);


BundleSignerTest::BundleSignerTest(const std::string& name): CppUnit::TestCase(name)
{
}


BundleSignerTest::~BundleSignerTest()
{
}


void BundleSignerTest::testSignFile()
{
	std::istringstream istr(APPINF_PEM);
	Poco::Crypto::RSAKey rsaKey(0, &istr, "secret");
	BundleSigner signer("appinf.com", rsaKey);
	signer.signBundle("testBundle.zip");
	
	Poco::OSP::BundleFile::Ptr pBundleFile = new Poco::OSP::BundleFile("testBundle.zip");
	std::string rsa;
	Poco::SharedPtr<std::istream> rsaStream(pBundleFile->getResource("META-INF/appinf.com.rsa"));
	assert (!rsaStream.isNull());
	int ch = rsaStream->get();
	while (ch != -1) 
	{
		rsa += static_cast<char>(ch);
		ch = rsaStream->get();
	}

	assert (rsa == APPINF_RSA_NO_CERT);
}


void BundleSignerTest::testSignFileSHA256()
{
	std::istringstream istr(APPINF_PEM);
	Poco::Crypto::RSAKey rsaKey(0, &istr, "secret");
	BundleSigner signer("appinf.com", rsaKey, "SHA256");
	signer.signBundle("testBundle.zip");
	
	Poco::OSP::BundleFile::Ptr pBundleFile = new Poco::OSP::BundleFile("testBundle.zip");
	std::string rsa;
	Poco::SharedPtr<std::istream> rsaStream(pBundleFile->getResource("META-INF/appinf.com.rsa"));
	assert (!rsaStream.isNull());
	int ch = rsaStream->get();
	while (ch != -1) 
	{
		rsa += static_cast<char>(ch);
		ch = rsaStream->get();
	}

	assert (rsa == APPINF_RSA_SHA256_NO_CERT);
}


void BundleSignerTest::testSignDirectory()
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
	BundleSigner signer("appinf.com", rsaKey);
	signer.signBundle("testBundle");	

	Poco::OSP::BundleDirectory::Ptr pBundleDir = new Poco::OSP::BundleDirectory("testBundle");
	std::string rsa;
	Poco::SharedPtr<std::istream> rsaStream(pBundleDir->getResource("META-INF/appinf.com.rsa"));
	assert (!rsaStream.isNull());
	int ch = rsaStream->get();
	while (ch != -1) 
	{
		rsa += static_cast<char>(ch);
		ch = rsaStream->get();
	}
	rsaStream = 0;
	
	assert (rsa == APPINF_RSA_NO_CERT);

	bundleFile.remove(true);
}


void BundleSignerTest::testSignDirectoryCert()
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

	Poco::OSP::BundleDirectory::Ptr pBundleDir = new Poco::OSP::BundleDirectory("testBundle");
	std::string rsa;
	Poco::SharedPtr<std::istream> rsaStream(pBundleDir->getResource("META-INF/appinf.com.rsa"));
	assert (!rsaStream.isNull());
	int ch = rsaStream->get();
	while (ch != -1) 
	{
		rsa += static_cast<char>(ch);
		ch = rsaStream->get();
	}
	rsaStream = 0;
	
	assert (rsa == APPINF_RSA);

	bundleFile.remove(true);
}


void BundleSignerTest::setUp()
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


void BundleSignerTest::tearDown()
{
}


CppUnit::Test* BundleSignerTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("BundleSignerTest");

	CppUnit_addTest(pSuite, BundleSignerTest, testSignFile);
	CppUnit_addTest(pSuite, BundleSignerTest, testSignFileSHA256);
	CppUnit_addTest(pSuite, BundleSignerTest, testSignDirectory);
	CppUnit_addTest(pSuite, BundleSignerTest, testSignDirectoryCert);

	return pSuite;
}
