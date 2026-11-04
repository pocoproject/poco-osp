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
#include "TestData.h"
#include "Poco/OSP/BundleSign/BundleSigner.h"
#include "Poco/OSP/BundleSign/SignatureFile.h"
#include "Poco/OSP/BundleFile.h"
#include "Poco/OSP/BundleDirectory.h"
#include "Poco/Crypto/RSAKey.h"
#include "Poco/Zip/Decompress.h"
#include "Poco/File.h"
#include "Poco/Path.h"
#include "Poco/SharedPtr.h"
#include "Poco/Exception.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include <sstream>
#include <fstream>


using namespace Poco::OSP::BundleSign;
using namespace Poco::OSP;


static const std::string SIGNER_RSA_SHA1(
	"Signature-Version: 1.0\n"
	"Created-By: OSP BundleSigner\n"
	"\n"
	"Name: META-INF/test.signer.sf\n"
	"RSA-SHA1-Signature: 444f47c8ae351b9d6a6865136779dfccd36ba570f4a5fc3d5ab81b83c308ae84ef0ad2d7464607ab60969347aa860eaf5e923c6c37b77ab72d2e8621d18b7c1f4bbc2eed1255f89ab590510724726b0a9747ec3c63f66ba41e57df8d3286a7cd9ef9848158efd358bb7d888a09ca478aa167d55f4b606d0ab8c481cc1959602a0005c04068c1f5dc7e83777f336ff5eacb98db45e424abec086306b16cbe3bfbe6776574cb9df8354c58eb890ca6598561348e8c1300041afd61947f8b82b61a2615ffcb34dd21edc3c32ac59534ac81328dfa54809abe626308b7c4c795d407d8c250264ffab8648fef22770cbc022cb3276311349ac990c4d8c3efde9e3a82\n"
	"\n"
);


static const std::string SIGNER_RSA_SHA256(
	"Signature-Version: 1.0\n"
	"Created-By: OSP BundleSigner\n"
	"\n"
	"Name: META-INF/test.signer.sf\n"
	"RSA-SHA256-Signature: 17c493bf5944f5d9941c791a1de9abc6188682b8ce72fdae6ae618490e113ca6d18ff1f740f214454f4119fbeddf2d30f601e17a08bea09905099ce0b584460aaaf5e782b2fbc8c3133176dcfe34145a6861f121a94eeca54bf4099e1ae52d7d10d7a485dfbec6bee3086947288e8a2fdebf33d38204e877011983267377e8838c4e85cb494c67bb0e906f9a0630bc02fb7538284ad39dd650bd6eb31337c2dd91abbcfa4d73832b6865fbb1027ea3dd39445f7d7f17f286d257fad6ae7676505bd1b0f94b5e623ac7f2a13e53529270774f2570a4c076e371dc16dc27c9a40e4e74883cd7d898c512952d7e8d034485426b1d19e5cb06174b93a628f3f91519\n"
	"\n"
);



BundleSignerTest::BundleSignerTest(const std::string& name): CppUnit::TestCase(name)
{
}


BundleSignerTest::~BundleSignerTest()
{
}


void BundleSignerTest::testSignFile()
{
	Poco::Crypto::RSAKey rsaKey("", TestData::path("signer.pem"));
	BundleSigner signer("test.signer", rsaKey);
	signer.signBundle(_bundleFile);
	
	Poco::OSP::BundleFile::Ptr pBundleFile = new Poco::OSP::BundleFile(_bundleFile);
	std::string rsa;
	Poco::SharedPtr<std::istream> rsaStream(pBundleFile->getResource("META-INF/test.signer.rsa"));
	assert (!rsaStream.isNull());
	int ch = rsaStream->get();
	while (ch != -1) 
	{
		rsa += static_cast<char>(ch);
		ch = rsaStream->get();
	}

	assert (rsa == SIGNER_RSA_SHA256);
}


void BundleSignerTest::testSignFileSHA1()
{
	Poco::Crypto::RSAKey rsaKey("", TestData::path("signer.pem"));
	BundleSigner signer("test.signer", rsaKey, "SHA1");
	signer.signBundle(_bundleFile);
	
	Poco::OSP::BundleFile::Ptr pBundleFile = new Poco::OSP::BundleFile(_bundleFile);
	std::string rsa;
	Poco::SharedPtr<std::istream> rsaStream(pBundleFile->getResource("META-INF/test.signer.rsa"));
	assert (!rsaStream.isNull());
	int ch = rsaStream->get();
	while (ch != -1) 
	{
		rsa += static_cast<char>(ch);
		ch = rsaStream->get();
	}

	assert (rsa == SIGNER_RSA_SHA1);
}


void BundleSignerTest::testSignDirectory()
{
	std::ifstream bundleStream(_bundleFile, std::ios::binary);
	Poco::Zip::Decompress decompress(bundleStream, _bundleDirectory);
	decompress.decompressAllFiles();
	bundleStream.close();
	
	Poco::Crypto::RSAKey rsaKey("", TestData::path("signer.pem"));
	BundleSigner signer("test.signer", rsaKey);
	signer.signBundle(_bundleDirectory);

	Poco::OSP::BundleDirectory::Ptr pBundleDir = new Poco::OSP::BundleDirectory(_bundleDirectory);
	std::string rsa;
	Poco::SharedPtr<std::istream> rsaStream(pBundleDir->getResource("META-INF/test.signer.rsa"));
	assert (!rsaStream.isNull());
	int ch = rsaStream->get();
	while (ch != -1) 
	{
		rsa += static_cast<char>(ch);
		ch = rsaStream->get();
	}
	rsaStream = 0;
	
	assert (rsa == SIGNER_RSA_SHA256);
}


void BundleSignerTest::testSignDirectoryCert()
{
	std::ifstream bundleStream(_bundleFile, std::ios::binary);
	Poco::Zip::Decompress decompress(bundleStream, _bundleDirectory);
	decompress.decompressAllFiles();
	bundleStream.close();
	
	Poco::Crypto::RSAKey rsaKey("", TestData::path("signer.pem"));
	Poco::Crypto::X509Certificate cert(TestData::path("signer.pem"));
	BundleSigner signer("test.signer", rsaKey);
	signer.signBundle(_bundleDirectory, cert);

	Poco::OSP::BundleDirectory::Ptr pBundleDir = new Poco::OSP::BundleDirectory(_bundleDirectory);
	std::string rsa;
	Poco::SharedPtr<std::istream> rsaStream(pBundleDir->getResource("META-INF/test.signer.rsa"));
	assert (!rsaStream.isNull());
	int ch = rsaStream->get();
	while (ch != -1) 
	{
		rsa += static_cast<char>(ch);
		ch = rsaStream->get();
	}
	rsaStream = 0;

	std::ostringstream expected;
	expected << SIGNER_RSA_SHA256 << "X509-Certificate: OSP Test Signer\n";
	cert.save(expected);
	assert (rsa == expected.str());
}


void BundleSignerTest::testUnsupportedDigestAlgorithmIsRejected()
{
	Poco::Crypto::RSAKey rsaKey("", TestData::path("signer.pem"));
	try
	{
		BundleSigner signer("test.signer", rsaKey, "MD5");
		failmsg("the signer must reject a digest algorithm that the verifier does not support");
	}
	catch (Poco::InvalidArgumentException&)
	{
	}
}


void BundleSignerTest::testLongPathIsRejected()
{
	TestData::writeBundle(_bundleFile, {
		{"META-INF/manifest.mf", "Manifest-Version: 1.0\n"},
		{std::string(SignatureFile::MAX_PATH_LENGTH + 1, 'a'), "x"}
	});
	Poco::Crypto::RSAKey rsaKey("", TestData::path("signer.pem"));
	BundleSigner signer("test.signer", rsaKey);
	try
	{
		signer.signBundle(_bundleFile);
		failmsg("the signer must reject a path that the verifier would reject");
	}
	catch (Poco::InvalidArgumentException&)
	{
	}
}


void BundleSignerTest::setUp()
{
	_workDir = TestData::createWorkDirectory();
	Poco::Path workPath(_workDir);
	workPath.makeDirectory();
	_bundleFile = Poco::Path(workPath, "testBundle.zip").toString();
	_bundleDirectory = Poco::Path(workPath, "testBundle").toString();
	TestData::writeTestBundle(_bundleFile);
}


void BundleSignerTest::tearDown()
{
	Poco::File(_workDir).remove(true);
}


CppUnit::Test* BundleSignerTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("BundleSignerTest");

	CppUnit_addTest(pSuite, BundleSignerTest, testSignFile);
	CppUnit_addTest(pSuite, BundleSignerTest, testSignFileSHA1);
	CppUnit_addTest(pSuite, BundleSignerTest, testSignDirectory);
	CppUnit_addTest(pSuite, BundleSignerTest, testSignDirectoryCert);
	CppUnit_addTest(pSuite, BundleSignerTest, testUnsupportedDigestAlgorithmIsRejected);
	CppUnit_addTest(pSuite, BundleSignerTest, testLongPathIsRejected);

	return pSuite;
}
