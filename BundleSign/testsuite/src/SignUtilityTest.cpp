//
// SignUtilityTest.cpp
//
// Copyright (c) 2007, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties, 
// copied or duplicated in any form, in whole or in part.
//


#include "SignUtilityTest.h"
#include "TestData.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "Poco/OSP/BundleSign/SignUtility.h"
#include "Poco/Path.h"
#include "Poco/File.h"
#include "Poco/MD5Engine.h"
#include "Poco/Exception.h"
#include <utility>
#include <vector>


using namespace Poco::OSP::BundleSign;


SignUtilityTest::SignUtilityTest(const std::string& name): CppUnit::TestCase(name)
{
}


SignUtilityTest::~SignUtilityTest()
{
}


void SignUtilityTest::testSignUtility()
{
	SignUtility::Digests digests;
	Poco::MD5Engine md5;
	SignUtility::computeDigests(_bundleFile, md5, digests);
	assert (digests.size() == 2);
	assert (digests["bundle.properties"] == "f5abb3e0801e36fc885f95cd7bacea15");
	assert (digests["META-INF/manifest.mf"] == "785608dce245c09477b6670f7d4012c6");
}


void SignUtilityTest::testFilesWithoutDirectoryEntriesAreIncluded()
{
	TestData::writeBundle(_bundleFile, {
		{"META-INF/manifest.mf", "Manifest-Version: 1.0\n"},
		{"bundle.properties", "foo: bar\n\n"},
		{"en/bundle.properties", "foo: baz\n"}
	});

	SignUtility::Digests digests;
	Poco::MD5Engine md5;
	SignUtility::computeDigests(_bundleFile, md5, digests);
	assertTrue (digests.size() == 3);
	assertEqual ("785608dce245c09477b6670f7d4012c6", digests["META-INF/manifest.mf"]);
	assertEqual ("f5abb3e0801e36fc885f95cd7bacea15", digests["bundle.properties"]);
	assertTrue (digests.find("en/bundle.properties") != digests.end());
}


void SignUtilityTest::testDuplicateEntryIsRejected()
{
	TestData::writeBundle(_bundleFile, {
		{"META-INF/manifest.mf", "Manifest-Version: 1.0\n"},
		{"a.txt", "a"},
		{"b.txt", "b"}
	});
	TestData::renameEntry(_bundleFile, "b.txt", "a.txt");

	SignUtility::Digests digests;
	Poco::MD5Engine md5;
	try
	{
		SignUtility::computeDigests(_bundleFile, md5, digests);
		failmsg("a bundle with two entries of the same name must be rejected");
	}
	catch (Poco::DataFormatException&)
	{
	}
}


void SignUtilityTest::testNonCanonicalEntryNameIsRejected()
{
	static const std::vector<std::pair<std::string, std::string>> names = {
		{"aa/x.txt", "../x.txt"},
		{"a/x.txt", "./x.txt"},
		{"ab/x.txt", "/b/x.txt"},
		{"ab/x.txt", "a//x.txt"},
		{"a_x.txt", "a\\x.txt"}
	};
	for (const auto& name: names)
	{
		TestData::writeBundle(_bundleFile, {
			{"META-INF/manifest.mf", "Manifest-Version: 1.0\n"},
			{name.first, "x"}
		});
		TestData::renameEntry(_bundleFile, name.first, name.second);

		SignUtility::Digests digests;
		Poco::MD5Engine md5;
		try
		{
			SignUtility::computeDigests(_bundleFile, md5, digests);
			failmsg("a bundle with the entry name " + name.second + " must be rejected");
		}
		catch (Poco::DataFormatException&)
		{
		}
	}
}


void SignUtilityTest::setUp()
{
	_workDir = TestData::createWorkDirectory();
	Poco::Path workPath(_workDir);
	workPath.makeDirectory();
	_bundleFile = Poco::Path(workPath, "testBundle.zip").toString();
	TestData::writeTestBundle(_bundleFile);
}


void SignUtilityTest::tearDown()
{
	Poco::File(_workDir).remove(true);
}


CppUnit::Test* SignUtilityTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("SignUtilityTest");

	CppUnit_addTest(pSuite, SignUtilityTest, testSignUtility);
	CppUnit_addTest(pSuite, SignUtilityTest, testFilesWithoutDirectoryEntriesAreIncluded);
	CppUnit_addTest(pSuite, SignUtilityTest, testDuplicateEntryIsRejected);
	CppUnit_addTest(pSuite, SignUtilityTest, testNonCanonicalEntryNameIsRejected);

	return pSuite;
}
