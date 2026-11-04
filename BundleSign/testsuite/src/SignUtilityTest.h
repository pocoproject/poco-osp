//
// SignUtilityTest.h
//
// Definition of the SignUtilityTest class.
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties, 
// copied or duplicated in any form, in whole or in part.
//


#ifndef SignUtilityTest_INCLUDED
#define SignUtilityTest_INCLUDED


#include "Poco/OSP/BundleSign/BundleSign.h"
#include "CppUnit/TestCase.h"


class SignUtilityTest: public CppUnit::TestCase
{
public:
	SignUtilityTest(const std::string& name);
	~SignUtilityTest();

	void testSignUtility();
	void testFilesWithoutDirectoryEntriesAreIncluded();
	void testDuplicateEntryIsRejected();
	void testNonCanonicalEntryNameIsRejected();

	void setUp();
	void tearDown();

	static CppUnit::Test* suite();

private:
	std::string _workDir;
	std::string _bundleFile;
};


#endif // SignUtilityTest_INCLUDED
