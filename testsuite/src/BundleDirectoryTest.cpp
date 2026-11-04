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
#include "CppUnit/TestCase.h"
#include "Poco/OSP/BundleDirectory.h"
#include "Poco/File.h"
#include "Poco/Path.h"
#include "Poco/TemporaryFile.h"
#include "Poco/Exception.h"
#if defined(POCO_OS_FAMILY_WINDOWS)
#include "Poco/UnicodeConverter.h"
#include "Poco/UnWindows.h"
#endif
#include <fstream>
#include <ios>
#include <memory>
#include <string>
#include <vector>


using Poco::OSP::BundleDirectory;
using Poco::OSP::BundleStorage;
using Poco::File;
using Poco::Path;
using namespace std::string_literals;


BundleDirectoryTest::BundleDirectoryTest(const std::string& name): CppUnit::TestCase(name)
{
}


BundleDirectoryTest::~BundleDirectoryTest() = default;


void BundleDirectoryTest::testGetResourceOpensFile()
{
	std::string s;
	BundleStorage::Ptr pBD(new BundleDirectory(_bundlePath));

	std::unique_ptr<std::istream> istr1(pBD->getResource("META-INF/manifest.mf"));
	assert (istr1 != nullptr);
	std::getline(*istr1, s);
	assert (s == "Manifest-Version: 1.0");

	std::unique_ptr<std::istream> istr2(pBD->getResource("bundle.properties"));
	assert (istr2 != nullptr);
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


void BundleDirectoryTest::testGetResourceIgnoresOtherCaseOfName()
{
	if (!File(Path(Path::forDirectory(_bundlePath), "BUNDLE.PROPERTIES"s)).exists())
	{
		warnmsg("The file system distinguishes case, test skipped.");
		return;
	}
	BundleStorage::Ptr pBD(new BundleDirectory(_bundlePath));
	for (const auto& path: {"BUNDLE.PROPERTIES"s, "meta-inf/manifest.mf"s})
	{
		std::unique_ptr<std::istream> pStream(pBD->getResource(path));
		assertTrue (pStream == nullptr);
	}
	std::unique_ptr<std::istream> pStream(pBD->getResource("bundle.properties"s));
	assertTrue (pStream != nullptr);
}


void BundleDirectoryTest::testGetResourceIgnoresOtherNormalizationOfName()
{
	// U+00FC in normalization form C is stored; the request uses form D.
	const Path basePath(Path::forDirectory(_bundlePath));
	std::ofstream(Path(basePath, "\xC3\xBC.txt"s).toString(), std::ios::binary) << "x";
	if (!File(Path(basePath, "u\xCC\x88.txt"s)).exists())
	{
		warnmsg("The file system distinguishes normalization forms, test skipped.");
		return;
	}
	BundleStorage::Ptr pBD(new BundleDirectory(_bundlePath));
	std::unique_ptr<std::istream> pOther(pBD->getResource("u\xCC\x88.txt"s));
	assertTrue (pOther == nullptr);
	std::unique_ptr<std::istream> pStored(pBD->getResource("\xC3\xBC.txt"s));
	assertTrue (pStored != nullptr);
}


void BundleDirectoryTest::testGetResourceFollowsSymbolicLinks()
{
#if defined(POCO_OS_FAMILY_UNIX)
	const Path basePath(Path::forDirectory(_bundlePath));
	File(Path(basePath, "bundle.properties"s)).linkTo(Path(basePath, "link.properties"s).toString());
	File(Path(basePath, "META-INF"s)).linkTo(Path(basePath, "META-LINK"s).toString());
	BundleStorage::Ptr pBD(new BundleDirectory(_bundlePath));
	for (const auto& path: {"link.properties"s, "META-LINK/manifest.mf"s})
	{
		std::unique_ptr<std::istream> pStream(pBD->getResource(path));
		assertTrue (pStream != nullptr);
	}
#endif
}


void BundleDirectoryTest::testIsExactPathRejectsPathOutsideRoot()
{
	const Path basePath(Path::forDirectory(_bundlePath));
	const Path propsPath(basePath, "bundle.properties"s);
	assertTrue (BundleDirectory::isExactPath(basePath, propsPath));

	Path metaPath(basePath);
	metaPath.pushDirectory("META-INF"s);
	assertFalse (BundleDirectory::isExactPath(metaPath, propsPath));
	assertFalse (BundleDirectory::isExactPath(propsPath, basePath));

	Path deepRoot(basePath);
	deepRoot.pushDirectory("bundle.properties"s);
	deepRoot.pushDirectory("x"s);
	assertFalse (BundleDirectory::isExactPath(deepRoot, propsPath));

	Path relativePath;
	for (int i = 0; i < basePath.depth(); ++i)
	{
		relativePath.pushDirectory(basePath[i]);
	}
	relativePath.setFileName("bundle.properties"s);
	assertFalse (BundleDirectory::isExactPath(basePath, relativePath));

	Path nodePath(propsPath);
	nodePath.setNode("other"s);
	assertFalse (BundleDirectory::isExactPath(basePath, nodePath));

	Path devicePath(propsPath);
	devicePath.setDevice("X"s);
	assertFalse (BundleDirectory::isExactPath(basePath, devicePath));

	Path siblingPath(basePath);
	siblingPath.popDirectory();
	siblingPath.pushDirectory(basePath[basePath.depth() - 1] + "-other"s);
	siblingPath.setFileName("bundle.properties"s);
	assertFalse (BundleDirectory::isExactPath(basePath, siblingPath));
}


#if defined(POCO_OS_FAMILY_WINDOWS)
void BundleDirectoryTest::testGetResourceIgnoresWindowsAliases()
{
	Path longDir(Path::forDirectory(_bundlePath));
	longDir.pushDirectory("long-directory-name"s);
	File(longDir).createDirectories();
	std::ofstream(Path(longDir, "file.txt"s).toString(), std::ios::binary) << "x";
	BundleStorage::Ptr pBD(new BundleDirectory(_bundlePath));
	for (const auto& path: {"bundle.properties."s, "bundle.properties "s, "bundle.properties::$DATA"s})
	{
		std::unique_ptr<std::istream> pStream(pBD->getResource(path));
		assertTrue (pStream == nullptr);
	}

	std::wstring longPath;
	Poco::UnicodeConverter::toUTF16(longDir.toString(), longPath);
	const DWORD size = GetShortPathNameW(longPath.c_str(), nullptr, 0);
	assertTrue (size > 0);
	std::wstring shortPath(size, L'\0');
	const DWORD length = GetShortPathNameW(longPath.c_str(), shortPath.data(), size);
	assertTrue (length > 0 && length < size);
	shortPath.resize(length);
	std::string utf8ShortPath;
	Poco::UnicodeConverter::toUTF8(shortPath, utf8ShortPath);
	const Path shortDir(Path::forDirectory(utf8ShortPath));
	const std::string shortName(shortDir[shortDir.depth() - 1]);
	if (shortName == "long-directory-name"s)
	{
		warnmsg("The volume has no short names, short name case skipped.");
		return;
	}
	std::unique_ptr<std::istream> pShort(pBD->getResource(shortName + "/file.txt"s));
	assertTrue (pShort == nullptr);
	std::unique_ptr<std::istream> pLong(pBD->getResource("long-directory-name/file.txt"s));
	assertTrue (pLong != nullptr);
}
#endif


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
	CppUnit_addTest(pSuite, BundleDirectoryTest, testGetResourceIgnoresOtherCaseOfName);
	CppUnit_addTest(pSuite, BundleDirectoryTest, testGetResourceIgnoresOtherNormalizationOfName);
	CppUnit_addTest(pSuite, BundleDirectoryTest, testGetResourceFollowsSymbolicLinks);
	CppUnit_addTest(pSuite, BundleDirectoryTest, testIsExactPathRejectsPathOutsideRoot);
#if defined(POCO_OS_FAMILY_WINDOWS)
	CppUnit_addTest(pSuite, BundleDirectoryTest, testGetResourceIgnoresWindowsAliases);
#endif

	return pSuite;
}
