//
// VersionTest.cpp
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#include "VersionTest.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "Poco/OSP/Version.h"
#include "Poco/Exception.h"
#include <vector>


using Poco::OSP::Version;


VersionTest::VersionTest(const std::string& name): CppUnit::TestCase(name)
{
}


VersionTest::~VersionTest()
{
}


void VersionTest::testConstruct()
{
	Version v0;
	assert (v0.major() == 0);
	assert (v0.minor() == 0);
	assert (v0.revision() == 0);
	assert (v0.designation().empty());
	
	Version v1(1, 2, 3);
	assert (v1.major() == 1);
	assert (v1.minor() == 2);
	assert (v1.revision() == 3);
	assert (v1.designation().empty());

	Version v2(1, 2, 3, "20070403");
	assert (v2.major() == 1);
	assert (v2.minor() == 2);
	assert (v2.revision() == 3);
	assert (v2.designation() == "20070403");
	
	Version v3(v2);
	assert (v3.major() == 1);
	assert (v3.minor() == 2);
	assert (v3.revision() == 3);
	assert (v3.designation() == "20070403");
	
	v0 = v3;
	assert (v0.major() == 1);
	assert (v0.minor() == 2);
	assert (v0.revision() == 3);
	assert (v0.designation() == "20070403");
}


void VersionTest::testCompare()
{
	Version v100(1, 0, 0);
	Version v105(1, 0, 5);
	Version v150(1, 5, 0);
	Version v200(2, 0, 0);
	
	Version v(v100);
	assert (v == v100);
	assert (!(v == v200));
	assert (v != v200);
	assert (!(v != v100));
	
	v = v105;
	assert (v != v100);
	assert (v != v150);
	assert (v != v200);
	assert (v == v105);
	assert (v >  v100);
	assert (v >= v100);
	assert (v >= v105);
	assert (v <  v150);
	assert (v <= v150);
	assert (v <= v105);
	
	assert (!(v != v105));
	assert (!(v == v100));
	assert (!(v == v150));
	assert (!(v == v200));
	assert (!(v >  v105));
	assert (!(v >  v200));
	assert (!(v >= v200));
	assert (!(v <  v100));
	assert (!(v <= v100));
	
	v = v150;
	assert (v != v100);
	assert (v != v105);
	assert (v != v200);
	assert (v >  v100);
	assert (v >  v105);
	assert (v >= v100);
	assert (v >= v105);
	assert (v >= v150);
	assert (v <  v200);
	assert (v <= v150);
	
	assert (!(v != v150));
	assert (!(v == v100));
	assert (!(v == v105));
	assert (!(v == v200));
	assert (!(v >  v150));
	assert (!(v >  v200));
	assert (!(v >= v200));
	assert (!(v <  v100));
	assert (!(v <= v100));
	assert (!(v <  v105));
	assert (!(v <= v105));
}


void VersionTest::testToString()
{
	Version v1(1, 0, 0);
	assert (v1.toString() == "1.0.0");
	
	Version v2(1, 2, 3);
	assert (v2.toString() == "1.2.3");
	
	Version v3(2, 5, 11, "foo");
	assert (v3.toString() == "2.5.11-foo");
}


void VersionTest::testParse()
{
	Version v1("1.0.0");
	assert (v1.major() == 1);
	assert (v1.minor() == 0);
	assert (v1.revision() == 0);
	assert (v1.designation() == "");

	Version v2("2.10.23");
	assert (v2.major() == 2);
	assert (v2.minor() == 10);
	assert (v2.revision() == 23);
	assert (v2.designation() == "");

	Version v3("2.10.23-foo");
	assert (v3.major() == 2);
	assert (v3.minor() == 10);
	assert (v3.revision() == 23);
	assert (v3.designation() == "foo");

	Version v4("2.3.4-");
	assert (v4.major() == 2);
	assert (v4.minor() == 3);
	assert (v4.revision() == 4);
	assert (v4.designation() == "");
	
	Version v5("1");
	assert (v5.major() == 1);
	assert (v5.minor() == 0);
	assert (v5.revision() == 0);
	assert (v5.designation() == "");

	Version v6("1.2");
	assert (v6.major() == 1);
	assert (v6.minor() == 2);
	assert (v6.revision() == 0);
	assert (v6.designation() == "");
}


void VersionTest::testParseRejectsMalformed()
{
	const std::vector<std::string> versions = {"", "1.", "1..2", "1.2.", "99999999999", "1.2147483648", "-1", "1.x", ".1"};
	for (const auto& version: versions)
	{
		try
		{
			Version v(version);
			failmsg("version '" + version + "' must be rejected");
		}
		catch (Poco::SyntaxException&)
		{
		}
	}

	Version largest("2147483647.0.0");
	assertTrue (largest.major() == 2147483647);
}


void VersionTest::testCompareIgnoresDesignation()
{
	Version release(1, 0, 0);
	Version beta(1, 0, 0, "b1");
	assertTrue (beta == release);
	assertFalse (beta != release);
	assertFalse (beta < release);
	assertFalse (beta > release);
	assertTrue (beta <= release);
	assertTrue (beta >= release);
	assertTrue (Version("1.0.0-b1") == Version("1.0.0-b2"));
}


void VersionTest::testAssignFromStringKeepsValueOnError()
{
	Version v(1, 2, 3, "foo");
	v = "2.3.4-rc1";
	assertEqual (2, v.major());
	assertEqual (3, v.minor());
	assertEqual (4, v.revision());
	assertEqual ("rc1", v.designation());

	try
	{
		v = "2.x";
		fail("malformed version - must throw");
	}
	catch (Poco::SyntaxException&)
	{
	}
	assertEqual ("2.3.4-rc1", v.toString());
}


void VersionTest::testSwap()
{
	Version v1(1, 2, 3, "a");
	Version v2(4, 5, 6);
	swap(v1, v2);
	assertEqual ("4.5.6", v1.toString());
	assertEqual ("1.2.3-a", v2.toString());
}


void VersionTest::setUp()
{
}


void VersionTest::tearDown()
{
}


CppUnit::Test* VersionTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("VersionTest");

	CppUnit_addTest(pSuite, VersionTest, testConstruct);
	CppUnit_addTest(pSuite, VersionTest, testCompare);
	CppUnit_addTest(pSuite, VersionTest, testToString);
	CppUnit_addTest(pSuite, VersionTest, testParse);
	CppUnit_addTest(pSuite, VersionTest, testParseRejectsMalformed);
	CppUnit_addTest(pSuite, VersionTest, testCompareIgnoresDesignation);
	CppUnit_addTest(pSuite, VersionTest, testAssignFromStringKeepsValueOnError);
	CppUnit_addTest(pSuite, VersionTest, testSwap);

	return pSuite;
}
