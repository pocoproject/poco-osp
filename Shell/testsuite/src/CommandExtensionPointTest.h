//
// CommandExtensionPointTest.h
//
// Definition of the CommandExtensionPointTest class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef CommandExtensionPointTest_INCLUDED
#define CommandExtensionPointTest_INCLUDED


#include "CppUnit/TestCase.h"
#include <memory>
#include <string>


class ShellFixture;


class CommandExtensionPointTest: public CppUnit::TestCase
{
public:
	CommandExtensionPointTest() = delete;
	explicit CommandExtensionPointTest(const std::string& name);
	~CommandExtensionPointTest() override;

	void setUp() override;
	void tearDown() override;

	void testDuplicateCommandIsLoggedAndNotAdded();
	void testCommandsOfStoppedBundleAreRemoved();
	void testCommandsOfStoppedBundleAreNotAdded();
	void testCommandRunsToEndWhenItsBundleStops();
	void testFactoryClassComesFromBundleLibrary();
	void testFactoryClassNotInBundleLibraryIsNotFound();

	static CppUnit::Test* suite();

private:
	std::unique_ptr<ShellFixture> _pFixture;
};


#endif // CommandExtensionPointTest_INCLUDED
