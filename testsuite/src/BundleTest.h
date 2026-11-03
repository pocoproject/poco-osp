//
// BundleTest.h
//
// Definition of the BundleTest class.
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#ifndef BundleTest_INCLUDED
#define BundleTest_INCLUDED


#include "Poco/OSP/OSP.h"
#include "CppUnit/TestCase.h"
#include "Poco/OSP/BundleEvent.h"
#include <vector>


class BundleTest: public CppUnit::TestCase
{
public:
	BundleTest(const std::string& name);
	~BundleTest();

	void testLoadOne();
	void testVersionConflict();
	void testProperties();
	void testPropertiesLocalization1();
	void testPropertiesLocalization2();
	void testBundleFile();
	void testResolve();
	void testResolveModules();
	void testStart();
	void testActivator();
	void testStopAll();
	void testResolveStartStopUnloadAll();
	void testExtensionBundle();
	void testExtensionBundleReplacesResource();
	void testExtensionBundleAfterUnloadingUnresolvedExtension();
	void testSealedBundle();
	void testStartAfterDependencyReplaced();
	void testStartAfterDependencyReplacedSameVersion();
	void testStartAfterDependencyReplacedOutOfRange();
	void testStartAfterDependencyReplacedResolved();
	void testStartAfterDependencyReplacedUnresolvable();

	void setUp();
	void tearDown();

	static CppUnit::Test* suite();

protected:
	std::string findBundle(const std::string& name);

	std::string createTestBundle(const std::string& symbolicName, const std::string& version, const std::string& requireBundle = "", const std::string& extraHeader = "");
		/// Creates a bundle directory holding just a manifest below
		/// TEST_BUNDLES_DIR and returns its path.

	void handleEvent(const void* sender, Poco::OSP::BundleEvent& event);
	void handleStartingEvent(const void* sender, Poco::OSP::BundleEvent& event);
	void handleStartedEvent(const void* sender, Poco::OSP::BundleEvent& event);
	void handleStoppingEvent(const void* sender, Poco::OSP::BundleEvent& event);
	void handleStoppedEvent(const void* sender, Poco::OSP::BundleEvent& event);

private:
	static const std::string TEST_BUNDLES_DIR;

	std::vector<Poco::OSP::BundleEvent> _events;
};


#endif // BundleTest_INCLUDED
