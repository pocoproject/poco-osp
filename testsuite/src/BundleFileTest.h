//
// BundleFileTest.h
//
// Definition of the BundleFileTest class.
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#ifndef BundleFileTest_INCLUDED
#define BundleFileTest_INCLUDED


#include "Poco/OSP/OSP.h"
#include "CppUnit/TestCase.h"


class BundleFileTest: public CppUnit::TestCase
{
public:
	BundleFileTest(const std::string& name);
	~BundleFileTest() override;

	void testGetResourceOpensEntry();
	void testListReturnsDirectoryEntries();
	void testListWithoutDirectoryEntries();
	void testLastModifiedIsTimeOfEntry();
	void testLastModifiedOfMissingEntryThrows();
	void testListWithTrailingSlash();
	void testStorageOpenReturnsBundleFile();
	void testStorageOpenRejectsPathThatIsNoBundle();
	void testRenamedArchiveEntryIsRead();

	void setUp() override;
	void tearDown() override;

	static CppUnit::Test* suite();

private:
	std::string _scratchPath;
		/// A unique temporary directory that holds the files of the test.
	std::string _bundlePath;
		/// The bundle file that setUp() creates.
};


#endif // BundleFileTest_INCLUDED
