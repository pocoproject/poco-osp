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
#include "Poco/OSP/BundleLoader.h"
#include "Poco/AutoPtr.h"
#include "Poco/Channel.h"
#include <thread>
#include <vector>


class BundleTest: public CppUnit::TestCase
{
public:
	BundleTest(const std::string& name);
	~BundleTest() override;

	void testLoadOne();
	void testVersionConflict();
	void testManifestPlaceholdersAreReplacedByProperties();
	void testLocalizedPropertiesExactLanguage();
	void testLocalizedPropertiesFallBackToLanguage();
	void testBundleFile();
	void testResolve();
	void testResolveCircularDependencyThrows();
	void testResolveModules();
	void testStart();
	void testActivatorStartAndStopAreCalled();
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
	void testLibraryNameThatIsNoFileNameIsNotInstalled();
	void testCodeCacheRejectsLibraryNameThatIsNoFileName();
	void testCopiedFailedEventHasItsOwnException();
	void testStopAllAfterRequiredBundleUninstalled();
	void testResolveModuleWithoutVersion();
	void testDamagedLibraryIsNotInstalled();
	void testStoppedBundleRemovesOnlyItsOwnExtensionPoints();
	void testStartWhileRequiredBundleIsStarting();
	void testStopAllContinuesAfterFailingBundle();
	void testUnloadAllKeepsBundleThatFailsToStop();
	void testEventsAreFiredWithoutLoaderLock();
	void testResolveEventsOfRequiredBundlesKeepTheirOrder();
	void testResolveEventsAreFiredWhenResolutionFails();
	void testActivatorIsResetWhenBundleIsUnloaded();
	void testActivatorIsResetWhenBundlesAreUnloadedWithoutLibraries();
	void testStartAllContinuesAfterUnknownException();
	void testStartErrorNamesBundleThatFailsInStartingEvent();
	void testStartErrorNamesBundleThatFailsWhileAnotherThreadStartsBundle();
	void testUninstallRemovesBundleUnlessPrevented();
	void testStartAllStartsByRunLevelAndSkipsLazyStart();
	void testFindListUnloadAndContextOfBundles();
	void testActivatorStartThatThrowsLeavesBundleResolved();
	void testActivatorThatCannotBeLoadedFailsEveryStart();
	void testCodeCacheInstallsAndUninstallsLibrary();
	void testLibraryEntryOutsideCodeDirectoryIsNotInstalled();
	void testExtensionsAreHandledOnStartAndRemovedOnStop();
	void testSubsystemStartsBundlesOfConfiguredRepository();
	void testConcurrentStartsAndStopsOfDependencyChain();

	void setUp() override;
	void tearDown() override;

	static CppUnit::Test* suite();

protected:
	[[nodiscard]] std::string findBundle(const std::string& name);

	[[nodiscard]] std::string codeCachePath() const;
		/// Returns the path of the code cache in the directory
		/// that holds the files of the test.

	[[nodiscard]] std::string createTestBundle(const std::string& symbolicName, const std::string& version, const std::string& requireBundle = "", const std::string& extraHeader = "");
		/// Creates a bundle directory holding just a manifest in the
		/// directory that holds the files of the test and returns its path.

	[[nodiscard]] std::string createActivatorBundle(const std::string& symbolicName, const std::string& activator);
		/// Creates a bundle directory with the given Bundle-Activator
		/// and the TestBundle library of bundle9, and returns its path.

	[[nodiscard]] std::string createTestBundleArchive(const std::string& fileName, const std::string& archive);
		/// Writes the bundle archive to a file with the given name in the
		/// directory that holds the files of the test and returns its path.

	void handleEvent(const void* sender, Poco::OSP::BundleEvent& event);
	void handleStartingEvent(const void* sender, Poco::OSP::BundleEvent& event);
	void handleStartedEvent(const void* sender, Poco::OSP::BundleEvent& event);
	void handleStoppingEvent(const void* sender, Poco::OSP::BundleEvent& event);
	void handleStoppedEvent(const void* sender, Poco::OSP::BundleEvent& event);
	void handleStoppingEventFails(const void* sender, Poco::OSP::BundleEvent& event);
	void handleStartingEventThrowsUnknown(const void* sender, Poco::OSP::BundleEvent& event);
	void handleBundleError(const void* sender, const Poco::OSP::BundleLoader::BundleError& error);
	void handleEventProbesLoader(const void* sender, Poco::OSP::BundleEvent& event);

private:
	static constexpr long USER_START_WAIT_MS = 200;
	static constexpr long PROBE_WAIT_MS = 10000;
	static constexpr long WAIT_MS = 10000;
	static constexpr int START_STOP_CYCLES = 200;

	std::string _scratchPath;
		/// A unique temporary directory that holds the files of the test.
	Poco::AutoPtr<Poco::Channel> _pRootChannel;
	int _rootLevel = 0;
		/// The channel and level of the root logger before the test,
		/// which tearDown() gives back to every logger.
	std::vector<Poco::OSP::BundleEvent> _events;
	std::vector<std::string> _bundleErrors;
	Poco::OSP::BundleLoader* _pProbedLoader = nullptr;
	int _probes = 0;
	int _probesBlocked = 0;
	std::vector<std::thread> _probeThreads;
};


#endif // BundleTest_INCLUDED
