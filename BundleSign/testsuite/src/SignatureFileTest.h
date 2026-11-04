//
// SignatureFileTest.h
//
// Definition of the SignatureFileTest class.
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties, 
// copied or duplicated in any form, in whole or in part.
//


#ifndef SignatureFileTest_INCLUDED
#define SignatureFileTest_INCLUDED


#include "Poco/OSP/BundleSign/BundleSign.h"
#include "CppUnit/TestCase.h"


class SignatureFileTest: public CppUnit::TestCase
{
public:
	SignatureFileTest(const std::string& name);
	~SignatureFileTest() override;

	void testRead();
	void testWrite();
	void testReadRejectsLongEntry();
	void testWriteRejectsLongPath();
	void testWriteRejectsPathThatReadWouldChange();
	void testPreservesOnlyPathThatReadReturnsUnchanged();
	void testMaxSizeIsSizeOfWrittenEntries();

	void setUp() override;
	void tearDown() override;

	static CppUnit::Test* suite();
};


#endif // SignatureFileTest_INCLUDED
