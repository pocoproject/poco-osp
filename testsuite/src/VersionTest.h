//
// VersionTest.h
//
// Definition of the VersionTest class.
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef VersionTest_INCLUDED
#define VersionTest_INCLUDED


#include "Poco/OSP/OSP.h"
#include "CppUnit/TestCase.h"


class VersionTest: public CppUnit::TestCase
{
public:
	VersionTest(const std::string& name);
	~VersionTest() override;

	void testConstruct();
	void testCompare();
	void testToString();
	void testParse();
	void testParseRejectsMalformed();
	void testCompareIgnoresDesignation();
	void testAssignFromStringKeepsValueOnError();
	void testSwap();

	void setUp() override;
	void tearDown() override;

	static CppUnit::Test* suite();

private:
};


#endif // VersionTest_INCLUDED
