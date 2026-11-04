//
// BundleSignerTest.h
//
// Definition of the BundleSignerTest class.
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef BundleSignerTest_INCLUDED
#define BundleSignerTest_INCLUDED


#include "Poco/OSP/BundleSign/BundleSign.h"
#include "CppUnit/TestCase.h"


class BundleSignerTest: public CppUnit::TestCase
{
public:
	BundleSignerTest(const std::string& name);
	~BundleSignerTest() override;

	void testSignFile();
	void testSignFileSHA1();
	void testSignDirectory();
	void testSignDirectoryCert();
	void testUnsupportedDigestAlgorithmIsRejected();
	void testLongPathIsRejected();
	void testEntryNameWithLineFeedIsRejected();
	void testSignFileWithExtraFields();
	void testRejectedBundleIsUnchanged();
	void testAliasThatIsNotAFileNameIsRejected();
	void testResignReplacesSignatureFiles();

	void setUp() override;
	void tearDown() override;

	static CppUnit::Test* suite();

private:
	std::string _workDir;
	std::string _bundleFile;
	std::string _bundleDirectory;
};


#endif // BundleSignerTest_INCLUDED
