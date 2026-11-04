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
#include "WebSessionTest.h"
#include "WebSessionManagerTest.h"
#include "LibraryFactoryTest.h"
#include "WebServerExtensionPointTest.h"
#include "AbstractWebServerBundleActivatorTest.h"
#include "CppUnit/Test.h"
#include "CppUnit/TestSuite.h"


CppUnit::Test* WebTestSuite::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("WebTestSuite");

	pSuite->addTest(WebServerDispatcherTest::suite());
	pSuite->addTest(MediaTypeMapperTest::suite());
	pSuite->addTest(WebSessionTest::suite());
	pSuite->addTest(WebSessionManagerTest::suite());
	pSuite->addTest(LibraryFactoryTest::suite());
	pSuite->addTest(WebServerExtensionPointTest::suite());
	pSuite->addTest(AbstractWebServerBundleActivatorTest::suite());

	return pSuite;
}
