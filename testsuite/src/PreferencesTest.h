//
// PreferencesTest.h
//
// Definition of the PreferencesTest class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef PreferencesTest_INCLUDED
#define PreferencesTest_INCLUDED


#include "Poco/OSP/OSP.h"
#include "Poco/OSP/Preferences.h"
#include "Poco/OSP/PreferencesEvent.h"
#include "CppUnit/TestCase.h"
#include <string>


class PreferencesTest: public CppUnit::TestCase
{
public:
	PreferencesTest() = delete;
	explicit PreferencesTest(const std::string& name);
	~PreferencesTest() override;

	void testDelegateReadsPreferences();
	void testFailedSaveKeepsPreviousPreferences();
	void testPreferencesPathThatCannotBeReadThrows();
	void testServiceKeepsPreferencesUnderItsDirectory();

	void setUp() override;
	void tearDown() override;

	static CppUnit::Test* suite();

private:
	static constexpr long WAIT_MS = 10000;

	void onPropertyChanged(const void* pSender, Poco::OSP::PreferencesEvent& event);

	std::string _path;
	Poco::OSP::Preferences::Ptr _pPrefs;
	std::string _valueSeen;
};


#endif // PreferencesTest_INCLUDED
