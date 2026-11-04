//
// SignUtilityTest.h
//
// Definition of the SignUtilityTest class.
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef SignUtilityTest_INCLUDED
#define SignUtilityTest_INCLUDED


#include "Poco/OSP/BundleSign/BundleSign.h"
#include "CppUnit/TestCase.h"


class SignUtilityTest: public CppUnit::TestCase
{
public:
	SignUtilityTest(const std::string& name);
	~SignUtilityTest() override;

	void testSignUtility();
	void testFilesWithoutDirectoryEntriesAreIncluded();
	void testDuplicateEntryIsRejected();
	void testNonCanonicalEntryNameIsRejected();
	void testNonCanonicalFileNameInDirectoryIsRejected();

	void setUp() override;
	void tearDown() override;

	static CppUnit::Test* suite();

private:
	std::string _workDir;
	std::string _bundleFile;
};


#endif // SignUtilityTest_INCLUDED
