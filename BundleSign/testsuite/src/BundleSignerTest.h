//
// BundleSignerTest.h
//
// Definition of the BundleSignerTest class.
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties, 
// copied or duplicated in any form, in whole or in part.
//


#ifndef BundleSignerTest_INCLUDED
#define BundleSignerTest_INCLUDED


#include "Poco/OSP/BundleSign/BundleSign.h"
#include "CppUnit/TestCase.h"


class BundleSignerTest: public CppUnit::TestCase
{
public:
	BundleSignerTest(const std::string& name);
	~BundleSignerTest();

	void testSignFile();
	void testSignFileSHA1();
	void testSignDirectory();
	void testSignDirectoryCert();
	void testUnsupportedDigestAlgorithmIsRejected();
	void testLongPathIsRejected();

	void setUp();
	void tearDown();

	static CppUnit::Test* suite();

private:
	std::string _workDir;
	std::string _bundleFile;
	std::string _bundleDirectory;
};


#endif // BundleSignerTest_INCLUDED
