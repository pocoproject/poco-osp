//
// BundleVerifierTest.h
//
// Definition of the BundleVerifierTest class.
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties, 
// copied or duplicated in any form, in whole or in part.
//


#ifndef BundleVerifierTest_INCLUDED
#define BundleVerifierTest_INCLUDED


#include "Poco/OSP/BundleSign/BundleSign.h"
#include "CppUnit/TestCase.h"


class BundleVerifierTest: public CppUnit::TestCase
{
public:
	BundleVerifierTest(const std::string& name);
	~BundleVerifierTest();

	void testVerifyFile();
	void testVerifyFileSHA256();
	void testVerifyDirectory();
	void testVerifyFailModifyFile();
	void testVerifyFailRemoveFile();
	void testVerifyFailAddFile();
	void testVerifyFailBadSignature();
	void testVerifyFailMissingSignature();

	void setUp();
	void tearDown();

	static CppUnit::Test* suite();
};


#endif // BundleVerifierTest_INCLUDED
