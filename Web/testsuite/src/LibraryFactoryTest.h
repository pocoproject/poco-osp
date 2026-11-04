//
// LibraryFactoryTest.h
//
// Definition of the LibraryFactoryTest class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef LibraryFactoryTest_INCLUDED
#define LibraryFactoryTest_INCLUDED


#include "Poco/OSP/Web/Web.h"
#include "CppUnit/TestCase.h"


class LibraryFactoryTest: public CppUnit::TestCase
{
public:
	LibraryFactoryTest() = delete;
	explicit LibraryFactoryTest(const std::string& name);
	~LibraryFactoryTest() override;

	void setUp() override;
	void tearDown() override;

	void testRequestHandlerKeepsLibraryLoaded();
	void testRequestHandlerFactoryWithoutHandler();
	void testFilterKeepsLibraryLoaded();
	void testFilterFactoryWithoutFilter();
#if defined(OSPWEB_TEST_LIBRARY)
	void testLibraryStaysLoadedWhileReferenced();
	void testObjectIsCreatedFromItsOwnLibrary();
#endif

	static CppUnit::Test* suite();
};


#endif // LibraryFactoryTest_INCLUDED
