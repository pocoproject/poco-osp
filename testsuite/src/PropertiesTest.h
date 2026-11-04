//
// PropertiesTest.h
//
// Definition of the PropertiesTest class.
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#ifndef PropertiesTest_INCLUDED
#define PropertiesTest_INCLUDED


#include "Poco/OSP/OSP.h"
#include "CppUnit/TestCase.h"


class PropertiesTest: public CppUnit::TestCase
{
public:
	PropertiesTest(const std::string& name);
	~PropertiesTest() override;

	void testGettersAreCaseInsensitiveAndUseDefaults();
	void testTypedSettersRoundTrip();
	void testNumberGetterOfNonNumberThrowsSyntaxException();
	void testKeysDifferingInCaseAreOneKey();
	void testCopyAssignAndSwap();
	void testCopyAndSwapWhileModified();
	void testOppositeAndSelfSwapsInThreadsComplete();

	void setUp() override;
	void tearDown() override;

	static CppUnit::Test* suite();

private:
	static constexpr int ITERATIONS = 10000;
	static constexpr int KEYS = 100;
	static constexpr long WAIT_MS = 10000;
};


#endif // PropertiesTest_INCLUDED
