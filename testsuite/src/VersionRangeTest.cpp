//
// VersionRangeTest.cpp
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#include "VersionRangeTest.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "Poco/OSP/VersionRange.h"


using Poco::OSP::Version;
using Poco::OSP::VersionRange;


VersionRangeTest::VersionRangeTest(const std::string& name): CppUnit::TestCase(name)
{
}


VersionRangeTest::~VersionRangeTest()
{
}


void VersionRangeTest::testClosedRangeIncludesBounds()
{
	Version v100(1, 0, 0);
	Version v200(2, 0, 0);
	Version v050(0, 5, 0);
	Version v150(1, 5, 0);
	Version v300(3, 0, 0);
	
	VersionRange vr1(v100, true, v200, true);
	assert (vr1.isInRange(v100));
	assert (vr1.isInRange(v150));
	assert (vr1.isInRange(v200));
	assert (!vr1.isInRange(v050));
	assert (!vr1.isInRange(v300));
	assert (!vr1.isEmpty());
}


void VersionRangeTest::testOpenRangeExcludesBounds()
{
	Version v100(1, 0, 0);
	Version v200(2, 0, 0);
	Version v050(0, 5, 0);
	Version v150(1, 5, 0);
	Version v300(3, 0, 0);
	
	VersionRange vr2(v100, false, v200, false);
	assert (!vr2.isInRange(v100));
	assert (vr2.isInRange(v150));
	assert (!vr2.isInRange(v200));
	assert (!vr2.isInRange(v050));
	assert (!vr2.isInRange(v300));
	assert (!vr2.isEmpty());
}


void VersionRangeTest::testSingleVersionRange()
{
	Version v100(1, 0, 0);
	Version v200(2, 0, 0);
	Version v150(1, 5, 0);
	
	VersionRange vr3(v150, true, v150, true);
	assert (vr3.isInRange(v150));
	assert (!vr3.isInRange(v100));
	assert (!vr3.isInRange(v200));
	assert (!vr3.isEmpty());

	VersionRange single(v150);
	assertTrue (single.lowerBound() == v150);
	assertTrue (single.upperBound() == v150);
	assertTrue (single.includeLower());
	assertTrue (single.includeUpper());
	assertTrue (single.isInRange(v150));
	assertTrue (!single.isInRange(v100));
}


void VersionRangeTest::testEmptyRangeContainsNoVersion()
{
	Version v100(1, 0, 0);
	Version v200(2, 0, 0);

	VersionRange vr4;
	assert (!vr4.isInRange(v100));
	assert (!vr4.isInRange(v200));
	assertTrue (!vr4.isInRange(Version()));
	assert (vr4.isEmpty());
}


void VersionRangeTest::testToString()
{
	Version v100(1, 0, 0);
	Version v200(2, 0, 0);

	assertEqual ("[1.0.0,2.0.0]", VersionRange(v100, true, v200, true).toString());
	assertEqual ("(1.0.0,2.0.0)", VersionRange(v100, false, v200, false).toString());
	assertEqual ("[1.0.0,2.0.0)", VersionRange(v100, true, v200, false).toString());
	assertEqual ("(1.0.0,2.0.0]", VersionRange(v100, false, v200, true).toString());
	assertEqual ("[1.0.0,1.0.0]", VersionRange(v100).toString());
	assertEqual ("()", VersionRange().toString());
}


void VersionRangeTest::testAssignAndSwap()
{
	Version v100(1, 0, 0);
	Version v200(2, 0, 0);

	VersionRange range(v100, true, v200, false);
	range = v200;
	assertEqual ("[2.0.0,2.0.0]", range.toString());

	VersionRange other(v100, false, v200, true);
	range = other;
	assertEqual ("(1.0.0,2.0.0]", range.toString());

	VersionRange empty;
	swap(range, empty);
	assertTrue (range.isEmpty());
	assertEqual ("(1.0.0,2.0.0]", empty.toString());
}


void VersionRangeTest::setUp()
{
}


void VersionRangeTest::tearDown()
{
}


CppUnit::Test* VersionRangeTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("VersionRangeTest");

	CppUnit_addTest(pSuite, VersionRangeTest, testClosedRangeIncludesBounds);
	CppUnit_addTest(pSuite, VersionRangeTest, testOpenRangeExcludesBounds);
	CppUnit_addTest(pSuite, VersionRangeTest, testSingleVersionRange);
	CppUnit_addTest(pSuite, VersionRangeTest, testEmptyRangeContainsNoVersion);
	CppUnit_addTest(pSuite, VersionRangeTest, testToString);
	CppUnit_addTest(pSuite, VersionRangeTest, testAssignAndSwap);

	return pSuite;
}
