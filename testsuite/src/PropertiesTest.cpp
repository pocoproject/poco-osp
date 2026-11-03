//
// PropertiesTest.cpp
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#include "PropertiesTest.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "Poco/OSP/Properties.h"
#include "Poco/Exception.h"
#include "Poco/Types.h"
#include <atomic>
#include <string>
#include <thread>
#include <vector>


using Poco::OSP::Properties;


PropertiesTest::PropertiesTest(const std::string& name): CppUnit::TestCase(name)
{
}


PropertiesTest::~PropertiesTest() = default;


void PropertiesTest::testGettersAreCaseInsensitiveAndUseDefaults()
{
	Properties props;
	
	props.set("foo", "bar");
	assert (props.has("foo"));
	assert (props.has("Foo"));
	assert (props["foo"] == "bar");
	assert (props.get("foo") == "bar");
	assert (props.get("FOO") == "bar");
	
	assert (!props.has("f00"));
	
	try
	{
		props.get("fuu");
		fail("nonexistent property - must throw");
	}
	catch (Poco::NotFoundException&)
	{
	}
	assert (props.get("f00", "bar") == "bar");
	
	props.set("n", "42");
	props.set("x", "0.5");
	props.set("f", "false");
	props.set("t", "true");
	
	assert (props.getInt("n") == 42);
	assert (props.getInt("N") == 42);
	
	try
	{
		props.getInt("f00");
		fail("nonexistent property - must throw");
	}
	catch (Poco::NotFoundException&)
	{
	}
	assert (props.getInt("f00", 100) == 100);
	
	assert (props.getFloat("x") == 0.5);
	try
	{
		props.getFloat("f00");
		fail("nonexistent property - must throw");
	}
	catch (Poco::NotFoundException&)
	{
	}
	assert (props.getFloat("f00", 0.5) == 0.5);
	
	assert (props.getBool("f") == false);
	assert (props.getBool("t") == true);
	try
	{
		props.getBool("f00");
		fail("nonexistent property - must throw");
	}
	catch (Poco::NotFoundException&)
	{
	}
	assert (props.getBool("f00", true) == true);
	
	std::vector<std::string> keys;
	props.keys(keys);
	
	assert (keys.size() == 5);
	assert (keys[0] == "f");
	assert (keys[1] == "foo");
	assert (keys[2] == "n");
	assert (keys[3] == "t");
	assert (keys[4] == "x");
}


void PropertiesTest::testTypedSettersRoundTrip()
{
	Properties props;
	props.set("b", true);
	props.set("i", 42);
	props.set("l", static_cast<Poco::Int64>(9000000000LL));
	props.set("d", 0.25);
	props.set("c", "text");

	assertTrue (props.getBool("b"));
	assertEqual ("true", props.get("b"));
	assertEqual (42, props.getInt("i"));
	assertTrue (props.getInt64("l") == 9000000000LL);
	assertTrue (props.getInt64("missing", 7) == 7);
	assertTrue (props.getFloat("d") == 0.25);
	assertEqual ("text", props.get("c"));

	// any value other than "false" is true
	props.set("zero", "0");
	assertTrue (props.getBool("zero"));
	props.set("b", false);
	assertFalse (props.getBool("b"));
}


void PropertiesTest::testNumberGetterOfNonNumberThrowsSyntaxException()
{
	Properties props;
	props.set("s", "foo");
	try
	{
		props.getInt("s");
		fail("not a number - must throw");
	}
	catch (Poco::SyntaxException&)
	{
	}
	try
	{
		props.getInt64("s");
		fail("not a number - must throw");
	}
	catch (Poco::SyntaxException&)
	{
	}
	try
	{
		props.getFloat("s");
		fail("not a number - must throw");
	}
	catch (Poco::SyntaxException&)
	{
	}
	try
	{
		props.getInt("s", 1);
		fail("not a number - must throw");
	}
	catch (Poco::SyntaxException&)
	{
	}
}


void PropertiesTest::testKeysDifferingInCaseAreOneKey()
{
	Properties props;
	props.set("Foo", "1");
	props.set("foo", "2");
	assertTrue (props.keys() == std::vector<std::string>({"Foo"}));
	assertEqual ("2", props.get("FOO"));
}


void PropertiesTest::testCopyAssignAndSwap()
{
	Properties props;
	props.set("a", "1");

	Properties copy(props);
	props.set("a", "2");
	assertEqual ("1", copy.get("a"));

	Properties assigned;
	assigned.set("b", "3");
	assigned = props;
	assertEqual ("2", assigned.get("a"));
	assertFalse (assigned.has("b"));

	Properties other;
	other.set("c", "4");
	swap(props, other);
	assertTrue (props.keys() == std::vector<std::string>({"c"}));
	assertTrue (other.keys() == std::vector<std::string>({"a"}));
}


void PropertiesTest::testCopyAndSwapWhileModified()
{
	Properties props;
	props.set("key", "value");

	std::atomic<bool> done(false);
	std::thread writer([&]()
	{
		for (int i = 0; i < ITERATIONS; ++i)
		{
			props.set("key" + std::to_string(i % KEYS), "value");
		}
		done = true;
	});
	Properties other;
	bool copiesComplete = true;
	while (!done)
	{
		Properties copy(props);
		copiesComplete = copiesComplete && copy.has("key");
		copy.swap(other);
		other.swap(copy);
	}
	writer.join();
	assertTrue (copiesComplete);
	assertTrue (props.has("key0"));
}


void PropertiesTest::setUp()
{
}


void PropertiesTest::tearDown()
{
}


CppUnit::Test* PropertiesTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("PropertiesTest");

	CppUnit_addTest(pSuite, PropertiesTest, testGettersAreCaseInsensitiveAndUseDefaults);
	CppUnit_addTest(pSuite, PropertiesTest, testTypedSettersRoundTrip);
	CppUnit_addTest(pSuite, PropertiesTest, testNumberGetterOfNonNumberThrowsSyntaxException);
	CppUnit_addTest(pSuite, PropertiesTest, testKeysDifferingInCaseAreOneKey);
	CppUnit_addTest(pSuite, PropertiesTest, testCopyAssignAndSwap);
	CppUnit_addTest(pSuite, PropertiesTest, testCopyAndSwapWhileModified);

	return pSuite;
}
