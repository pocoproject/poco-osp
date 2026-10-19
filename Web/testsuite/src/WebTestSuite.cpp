//
// WebServerTestSuite.cpp
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#include "WebTestSuite.h"
#include "WebServerDispatcherTest.h"
#include "MediaTypeMapperTest.h"


CppUnit::Test* WebTestSuite::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("WebTestSuite");

	pSuite->addTest(WebServerDispatcherTest::suite());
	pSuite->addTest(MediaTypeMapperTest::suite());

	return pSuite;
}
