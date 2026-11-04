//
// SignatureFileTest.cpp
//
// Copyright (c) 2007, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "SignatureFileTest.h"
#include "Poco/OSP/BundleSign/SignatureFile.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "Poco/Exception.h"
#include <cstddef>
#include <sstream>
#include <string>
#include <vector>


using namespace Poco::OSP::BundleSign;
using namespace Poco::OSP;


SignatureFileTest::SignatureFileTest(const std::string& name): CppUnit::TestCase(name)
{
}


SignatureFileTest::~SignatureFileTest() = default;


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


void SignatureFileTest::testReadRejectsLongEntry()
{
	constexpr std::size_t LINE_LENGTH = 1024*1024;
	const std::string fileContent(
		"Signature-Version: 1.0\n"
		"Created-By: OSPBundleSign Testsuite\n"
		"\n"
		"Name: " + std::string(LINE_LENGTH, 'a') + "\n"
		"SHA1-Digest: 0123456789ABCDEF0123456789ABCDEF\n"
	);

	std::istringstream istr(fileContent);
	SignatureFile sf;
	SignatureFile::Signatures signatures;
	try
	{
		sf.read(signatures, istr);
		failmsg("an entry longer than MAX_ENTRY_LENGTH must be rejected");
	}
	catch (Poco::DataFormatException&)
	{
	}
}


void SignatureFileTest::testWriteRejectsLongPath()
{
	SignatureFile::Signatures signatures;
	signatures[std::string(SignatureFile::MAX_PATH_LENGTH + 1, 'a')] = "0123456789ABCDEF0123456789ABCDEF";

	SignatureFile sf("1.0", "OSPBundleSign Testsuite", "SHA1-Digest");
	std::ostringstream ostr;
	try
	{
		sf.write(signatures, ostr);
		failmsg("a path longer than MAX_PATH_LENGTH must be rejected");
	}
	catch (Poco::InvalidArgumentException&)
	{
	}
}


void SignatureFileTest::testWriteRejectsPathThatReadWouldChange()
{
	static const std::vector<std::string> paths = {"a\nSHA256-Digest: 00\n\nName: b", " a", "", "a\rb"};
	SignatureFile sf("1.0", "OSPBundleSign Testsuite", "SHA256-Digest");
	for (const auto& path: paths)
	{
		SignatureFile::Signatures signatures;
		signatures[path] = "0123456789ABCDEF0123456789ABCDEF";
		std::ostringstream ostr;
		try
		{
			sf.write(signatures, ostr);
			failmsg("the path \"" + path + "\" must be rejected");
		}
		catch (Poco::InvalidArgumentException&)
		{
		}
	}
}


void SignatureFileTest::testPreservesOnlyPathThatReadReturnsUnchanged()
{
	assertTrue (SignatureFile::preservesPath("META-INF/manifest.mf"));
	assertTrue (SignatureFile::preservesPath("dir/a b.txt"));
	static const std::vector<std::string> paths = {"a\nSHA256-Digest: 00\n\nName: b", " a", "", "a\rb", "a\x7f"};
	for (const auto& path: paths)
	{
		assertTrue (!SignatureFile::preservesPath(path));
	}
}


void SignatureFileTest::testMaxSizeIsSizeOfWrittenEntries()
{
	constexpr std::size_t SHA256_LENGTH = 32;
	const std::vector<std::string> paths = {"a.txt", "dir/b.txt"};
	SignatureFile::Signatures signatures;
	for (const auto& path: paths)
	{
		signatures[path] = std::string(2*SHA256_LENGTH, '0');
	}
	SignatureFile sf("1.0", "", "SHA256-Digest");
	std::ostringstream ostr;
	sf.write(signatures, ostr);
	const std::string header("Signature-Version: 1.0\n\n");

	const std::size_t maxSize = SignatureFile::maxSize(paths, "SHA256-Digest", SHA256_LENGTH);
	const std::size_t maxHeaderSize = SignatureFile::HEADER_ENTRIES*SignatureFile::MAX_ENTRY_LENGTH;
	assertEqual (maxHeaderSize + 190, maxSize);
	assertEqual (maxSize - maxHeaderSize, ostr.str().size() - header.size());
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
	CppUnit_addTest(pSuite, SignatureFileTest, testReadRejectsLongEntry);
	CppUnit_addTest(pSuite, SignatureFileTest, testWriteRejectsLongPath);
	CppUnit_addTest(pSuite, SignatureFileTest, testWriteRejectsPathThatReadWouldChange);
	CppUnit_addTest(pSuite, SignatureFileTest, testPreservesOnlyPathThatReadReturnsUnchanged);
	CppUnit_addTest(pSuite, SignatureFileTest, testMaxSizeIsSizeOfWrittenEntries);

	return pSuite;
}
