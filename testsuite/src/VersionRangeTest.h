//
// VersionRangeTest.h
//
// Definition of the VersionRangeTest class.
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef VersionRangeTest_INCLUDED
#define VersionRangeTest_INCLUDED


#include "Poco/OSP/OSP.h"
#include "CppUnit/TestCase.h"


class VersionRangeTest: public CppUnit::TestCase
{
public:
	VersionRangeTest(const std::string& name);
	~VersionRangeTest() override;

	void testClosedRangeIncludesBounds();
	void testOpenRangeExcludesBounds();
	void testSingleVersionRange();
	void testEmptyRangeContainsNoVersion();
	void testToString();
	void testAssignAndSwap();

	void setUp() override;
	void tearDown() override;

	static CppUnit::Test* suite();

private:
};


#endif // VersionRangeTest_INCLUDED
