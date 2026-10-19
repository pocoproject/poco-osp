//
// SignatureFileTest.cpp
//
// Copyright (c) 2007, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties, 
// copied or duplicated in any form, in whole or in part.
//


#include "SignatureFileTest.h"
#include "Poco/OSP/BundleSign/SignatureFile.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include <sstream>


using namespace Poco::OSP::BundleSign;
using namespace Poco::OSP;


SignatureFileTest::SignatureFileTest(const std::string& name): CppUnit::TestCase(name)
{
}


SignatureFileTest::~SignatureFileTest()
{
}


void SignatureFileTest::testRead()
{
	std::string fileContent(
		"Signature-Version: 1.0\n"
		"Created-By: OSPBundleSign Testsuite\n"
		"\n"
		"Name: file1.txt\n"
		"SHA1-Digest: 0123456789ABCDEF0123456789ABCDEF\n"
		"\n"
		"Name: file2.txt\n"
		"SHA1-Digest: 1123456789ABCDEF0123456789ABCDEF\n"
	);
	
	std::istringstream istr(fileContent);
	SignatureFile sf;
	SignatureFile::Signatures signatures;
	sf.read(signatures, istr);
	
	assert (signatures.size() == 2);
	assert (signatures["file1.txt"] == "0123456789ABCDEF0123456789ABCDEF");
	assert (signatures["file2.txt"] == "1123456789ABCDEF0123456789ABCDEF");
}


void SignatureFileTest::testWrite()
{
	SignatureFile::Signatures signatures;
	signatures["file1.txt"] = "0123456789ABCDEF0123456789ABCDEF";
	signatures["file2.txt"] = "1123456789ABCDEF0123456789ABCDEF";
	
	SignatureFile sf("1.0", "OSPBundleSign Testsuite", "SHA1-Digest");
	std::ostringstream ostr;
	sf.write(signatures, ostr);
	std::string fileContent(ostr.str());
	assert (fileContent == 
		"Signature-Version: 1.0\n"
		"Created-By: OSPBundleSign Testsuite\n"
		"\n"
		"Name: file1.txt\n"
		"SHA1-Digest: 0123456789ABCDEF0123456789ABCDEF\n"
		"\n"
		"Name: file2.txt\n"
		"SHA1-Digest: 1123456789ABCDEF0123456789ABCDEF\n"
		"\n"
	);
}


void SignatureFileTest::setUp()
{
}


void SignatureFileTest::tearDown()
{
}


CppUnit::Test* SignatureFileTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("SignatureFileTest");

	CppUnit_addTest(pSuite, SignatureFileTest, testRead);
	CppUnit_addTest(pSuite, SignatureFileTest, testWrite);

	return pSuite;
}
