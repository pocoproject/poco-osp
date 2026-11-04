//
// BundleDirectoryTest.cpp
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#include "BundleDirectoryTest.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "Poco/OSP/BundleDirectory.h"
#include "Poco/File.h"
#include "Poco/Path.h"
#include "Poco/TemporaryFile.h"
#include "Poco/Exception.h"
#include <fstream>
#include <memory>
#include <vector>


using Poco::OSP::BundleDirectory;
using Poco::OSP::BundleStorage;
using Poco::File;
using Poco::Path;


BundleDirectoryTest::BundleDirectoryTest(const std::string& name): CppUnit::TestCase(name)
{
}


BundleDirectoryTest::~BundleDirectoryTest()
{
}


void BundleDirectoryTest::testGetResourceOpensFile()
{
	std::string s;
	BundleStorage::Ptr pBD(new BundleDirectory(_bundlePath));

	std::unique_ptr<std::istream> istr1(pBD->getResource("META-INF/manifest.mf"));
	assert (istr1.get() != 0);
	std::getline(*istr1, s);
	assert (s == "Manifest-Version: 1.0");

	std::unique_ptr<std::istream> istr2(pBD->getResource("bundle.properties"));
	assert (istr2.get() != 0);
	std::getline(*istr2, s);
	assert (s == "foo: bar");

	std::unique_ptr<std::istream> istr3(pBD->getResource("nonexistent"));
	assertTrue (istr3 == nullptr);
}


void BundleDirectoryTest::testGetResourceRejectsInvalidPath()
{
	BundleStorage::Ptr pBD(new BundleDirectory(_bundlePath));
	const std::vector<std::string> paths = {"/foo.bar", "foo/../../bar", "foo\\..\\bar"};
	for (const auto& path: paths)
	{
		try
		{
			std::unique_ptr<std::istream> pStream(pBD->getResource(path));
			failmsg("resource path '" + path + "' must be rejected");
		}
		catch (Poco::InvalidArgumentException&)
		{
		}
	}
}


void BundleDirectoryTest::testListReturnsDirectoryEntries()
{
	BundleStorage::Ptr pBD(new BundleDirectory(_bundlePath));
	std::vector<std::string> files;
	pBD->list("", files);
	assert (files.size() == 2);

	pBD->list("META-INF", files);
	assert (files.size() == 1);
	assert (files[0] == "manifest.mf");
}


void BundleDirectoryTest::testParentDirectoryIsRejected()
{
	BundleStorage::Ptr pBD(new BundleDirectory(_bundlePath));
	std::vector<std::string> files;
	const std::vector<std::string> paths = {"..", "./..", "x/../..", "META-INF/../.."};
	for (const auto& path: paths)
	{
		try
		{
			pBD->list(path, files);
			failmsg("list of '" + path + "' must be rejected");
		}
		catch (Poco::InvalidArgumentException&)
		{
		}
	}

	try
	{
		pBD->lastModified("..");
		fail("parent directory - must throw");
	}
	catch (Poco::InvalidArgumentException&)
	{
	}
}


void BundleDirectoryTest::testLastModifiedIsTimeOfFile()
{
	BundleStorage::Ptr pBD(new BundleDirectory(_bundlePath));
	Path manifestPath(Path::forDirectory(_bundlePath));
	manifestPath.pushDirectory("META-INF");
	manifestPath.setFileName("manifest.mf");
	assertTrue (pBD->lastModified("META-INF/manifest.mf") == File(manifestPath).getLastModified());
}


void BundleDirectoryTest::testStorageOpenReturnsBundleDirectory()
{
	BundleStorage::Ptr pStorage(BundleStorage::open(_bundlePath));
	assertTrue (dynamic_cast<BundleDirectory*>(pStorage.get()) != nullptr);
	std::unique_ptr<std::istream> pStream(pStorage->getResource("bundle.properties"));
	assertTrue (pStream != nullptr);
}


void BundleDirectoryTest::setUp()
{
	_bundlePath = Poco::TemporaryFile::tempName();
	Path basePath(_bundlePath);
	basePath.makeDirectory();
	Path metaPath(basePath, std::string("META-INF"));
	metaPath.makeDirectory();
	File metaDir(metaPath);
	metaDir.createDirectories();
	std::string manif(metaPath.toString() + "manifest.mf");
	std::ofstream manifStr(manif.c_str(), std::ios::binary);
	manifStr << "Manifest-Version: 1.0\n";
	manifStr.close();
	std::string props(basePath.toString() + "bundle.properties");
	std::ofstream propsStr(props.c_str(), std::ios::binary);
	propsStr << "foo: bar\n" << std::endl;
	propsStr.close();
}


void BundleDirectoryTest::tearDown()
{
	try
	{
		File(_bundlePath).remove(true);
	}
	catch (Poco::Exception&)
	{
	}
}


CppUnit::Test* BundleDirectoryTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("BundleDirectoryTest");

	CppUnit_addTest(pSuite, BundleDirectoryTest, testGetResourceOpensFile);
	CppUnit_addTest(pSuite, BundleDirectoryTest, testGetResourceRejectsInvalidPath);
	CppUnit_addTest(pSuite, BundleDirectoryTest, testListReturnsDirectoryEntries);
	CppUnit_addTest(pSuite, BundleDirectoryTest, testParentDirectoryIsRejected);
	CppUnit_addTest(pSuite, BundleDirectoryTest, testLastModifiedIsTimeOfFile);
	CppUnit_addTest(pSuite, BundleDirectoryTest, testStorageOpenReturnsBundleDirectory);

	return pSuite;
}
