//
// PreferencesTest.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "PreferencesTest.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "Poco/OSP/PreferencesService.h"
#include "Poco/Util/MapConfiguration.h"
#include "Poco/AutoPtr.h"
#include "Poco/TemporaryFile.h"
#include "Poco/File.h"
#include "Poco/Path.h"
#include "Poco/Delegate.h"
#include "Poco/Exception.h"


using Poco::OSP::Preferences;
using Poco::OSP::PreferencesEvent;
using Poco::OSP::PreferencesService;


PreferencesTest::PreferencesTest(const std::string& name): CppUnit::TestCase(name)
{
}


PreferencesTest::~PreferencesTest() = default;


void PreferencesTest::testDelegateReadsPreferences()
{
	Preferences::Ptr pPrefs(new Preferences(Poco::Path(_path, "test.properties").toString()));
	pPrefs->setString("key", "old");

	_pPrefs = pPrefs;
	pPrefs->propertyChanged += Poco::delegate(this, &PreferencesTest::onPropertyChanged);
	pPrefs->setString("key", "new");
	pPrefs->propertyChanged -= Poco::delegate(this, &PreferencesTest::onPropertyChanged);
	_pPrefs = nullptr;

	// the event is fired before the value is changed
	assertTrue (_valueSeen == "old");
	assertTrue (pPrefs->getString("key") == "new");
}


void PreferencesTest::testFailedSaveKeepsPreviousPreferences()
{
	const std::string prefsPath(Poco::Path(_path, "test.properties").toString());
	Preferences::Ptr pPrefs(new Preferences(prefsPath));
	pPrefs->setString("key", "saved");
	pPrefs->save();

	// a directory where the new preferences would be written makes the save fail
	Poco::File blocker(prefsPath + ".tmp");
	blocker.createDirectories();
	assertTrue (Poco::File(Poco::Path(blocker.path(), "file")).createFile());
	pPrefs->setString("key", "unsaved");
	try
	{
		pPrefs->save();
		fail("preferences cannot be written - must throw");
	}
	catch (Poco::FileException&)
	{
	}
	blocker.remove(true);

	Preferences::Ptr pReloaded(new Preferences(prefsPath));
	assertTrue (pReloaded->getString("key") == "saved");
}


void PreferencesTest::testPreferencesPathThatCannotBeReadThrows()
{
	// a directory instead of a preferences file
	const std::string prefsPath(Poco::Path(_path, "dir.properties").toString());
	Poco::File(prefsPath).createDirectories();
	try
	{
		Preferences::Ptr pPrefs(new Preferences(prefsPath));
		fail("preferences cannot be read - must throw");
	}
	catch (Poco::IOException&)
	{
	}
}


void PreferencesTest::testServiceKeepsPreferencesUnderItsDirectory()
{
	Poco::AutoPtr<Poco::Util::MapConfiguration> pGlobalConfig(new Poco::Util::MapConfiguration);
	pGlobalConfig->setString("global.key", "globalValue");

	PreferencesService::Ptr pService(new PreferencesService(_path, pGlobalConfig));
	Preferences::Ptr pPrefs = pService->preferences("com.appinf.osp.test");
	assertTrue (pService->preferences("com.appinf.osp.test") == pPrefs);
	pPrefs->setString("key", "saved");
	pPrefs->save();

	Poco::Path prefsPath(Poco::Path::forDirectory(_path));
	prefsPath.pushDirectory(PreferencesService::SERVICE_NAME);
	prefsPath.setFileName("com.appinf.osp.test.properties");
	assertTrue (Poco::File(prefsPath).exists());

	PreferencesService::Ptr pReloaded(new PreferencesService(_path, pGlobalConfig));
	assertEqual ("saved", pReloaded->preferences("com.appinf.osp.test")->getString("key"));

	assertEqual ("globalValue", pService->configuration()->getString("global.key"));
	try
	{
		pService->configuration()->setString("global.key", "changed");
		fail("the global configuration is read-only - must throw");
	}
	catch (Poco::InvalidAccessException&)
	{
	}
	assertEqual ("globalValue", pGlobalConfig->getString("global.key"));
}


void PreferencesTest::onPropertyChanged(const void* pSender, PreferencesEvent& event)
{
	_valueSeen = _pPrefs->getString(event.name());
}


void PreferencesTest::setUp()
{
	_path = Poco::TemporaryFile::tempName();
	Poco::File(_path).createDirectories();
	_valueSeen.clear();
}


void PreferencesTest::tearDown()
{
	_pPrefs = nullptr;
	try
	{
		Poco::File(_path).remove(true);
	}
	catch (Poco::Exception&)
	{
	}
}


CppUnit::Test* PreferencesTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("PreferencesTest");

	CppUnit_addTest(pSuite, PreferencesTest, testDelegateReadsPreferences);
	CppUnit_addTest(pSuite, PreferencesTest, testFailedSaveKeepsPreviousPreferences);
	CppUnit_addTest(pSuite, PreferencesTest, testPreferencesPathThatCannotBeReadThrows);
	CppUnit_addTest(pSuite, PreferencesTest, testServiceKeepsPreferencesUnderItsDirectory);

	return pSuite;
}
