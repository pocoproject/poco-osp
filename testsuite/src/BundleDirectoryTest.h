//
// BundleDirectoryTest.h
//
// Definition of the BundleDirectoryTest class.
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef BundleDirectoryTest_INCLUDED
#define BundleDirectoryTest_INCLUDED


#include "Poco/OSP/OSP.h"
#include "CppUnit/TestCase.h"


class BundleDirectoryTest: public CppUnit::TestCase
{
public:
	BundleDirectoryTest(const std::string& name);
	~BundleDirectoryTest() override;

	void testGetResourceOpensFile();
	void testGetResourceRejectsInvalidPath();
	void testListReturnsDirectoryEntries();
	void testParentDirectoryIsRejected();
	void testLastModifiedIsTimeOfFile();
	void testStorageOpenReturnsBundleDirectory();
	void testGetResourceIgnoresOtherCaseOfName();
	void testGetResourceIgnoresOtherNormalizationOfName();
	void testGetResourceFollowsSymbolicLinks();
	void testIsExactPathRejectsPathOutsideRoot();
#if defined(POCO_OS_FAMILY_WINDOWS)
	void testGetResourceIgnoresWindowsAliases();
#endif

	void setUp() override;
	void tearDown() override;

	static CppUnit::Test* suite();

private:
	std::string _bundlePath;
		/// A unique temporary directory that holds the bundle.
};


#endif // BundleDirectoryTest_INCLUDED
