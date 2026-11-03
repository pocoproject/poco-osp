//
// BundleRepositoryTest.h
//
// Definition of the BundleRepositoryTest class.
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#ifndef BundleRepositoryTest_INCLUDED
#define BundleRepositoryTest_INCLUDED


#include "Poco/OSP/OSP.h"
#include "CppUnit/TestCase.h"
#include "Poco/OSP/BundleEvent.h"
#include "Poco/OSP/BundleLoader.h"
#include "Poco/AutoPtr.h"
#include "Poco/Channel.h"
#include <vector>


class BundleRepositoryTest: public CppUnit::TestCase
{
public:
	BundleRepositoryTest(const std::string& name);
	~BundleRepositoryTest() override;

	void testLoadFindsNewestVersionOfEachBundle();
	void testLoadKeepsNewerVersionFoundFirst();
	void testLoadChoosesBundleOfSameVersionByName();
	void testInstallWritesArchiveIntoFirstPathOnce();
	void testFilterThatAcceptsAllLoadsEveryBundle();
	void testFilterThatRejectsAllLoadsNoBundle();
	void testFilterThatAcceptsOneLoadsOnlyThatBundle();
	void testInstallRejectsSymbolicNameThatIsNoFileName();
	void testInstallRejectsBundleThatFilterRejects();
	void testTemporaryInstallFileIsNotLoaded();
	void testConcurrentInstallsOfSameBundle();
	void testInstallFiresBundleInstalled();
	void testUninstallKeepsBundleOutsideInstallRepository();
	void testUninstallKeepsBundleInstalledIntoSecondaryPath();
	void testInstallKeepsLeftoverTemporaryFile();
	void testInstallKeepsLeftoverTemporaryDirectory();
	void testInstallKeepsFileOfSameNameThatIsNoLoadedBundle();
	void testReplacementWithSameVersion();
	void testFailedReplacementKeepsReplacedBundle();
	void testFailedReplacementIntoSecondaryPathKeepsReplacedBundle();
	void testFailedReplacementStartsRequiringBundleAgain();
	void testReplacementDoesNotReportReplacedFileAsKept();
	void testReplacementStartsRequiringBundleAgain();
	void testActivatorInstallsBundlesDuringReplacement();
	void testInstallOfCorruptArchiveLeavesNoFile();
	void testMissingRepositoryPathIsSkipped();
	void testRepositoryPathWithGlobPattern();
	void testInstallerServiceInstallsFromUriAndReplaces();
	
	void setUp() override;
	void tearDown() override;

	static CppUnit::Test* suite();

protected:
	[[nodiscard]] std::string findBundleRepository();
	[[nodiscard]] static std::vector<std::string> loadedBundleNames(const Poco::OSP::BundleLoader& loader);
		/// Returns the symbolic names of the bundles of the loader, sorted.
	[[nodiscard]] std::string codeCachePath() const;
		/// Returns the path of the code cache in the directory
		/// that holds the files of the test.
	void handleInstalled(const void* sender, Poco::OSP::BundleEvent& event);
	void handleLoadedFailsForVersion2(const void* sender, Poco::OSP::BundleEvent& event);
	void handleLoadedFailsWithStdExceptionForVersion2(const void* sender, Poco::OSP::BundleEvent& event);

private:
	static constexpr long WAIT_MS = 10000;

	static const std::vector<std::string> TEST_BUNDLE_NAMES;
		/// The symbolic names of the bundles in the test bundle
		/// repository, sorted.
	static constexpr int TEST_BUNDLE_COUNT = 20;
		/// The number of bundles in the test bundle repository: two
		/// versions of bundle1, and bundle2 also as an archive.

	std::string _scratchPath;
		/// A unique temporary directory that holds the files of the test.
	Poco::AutoPtr<Poco::Channel> _pRootChannel;
	int _rootLevel = 0;
		/// The channel and level of the root logger before the test,
		/// which tearDown() gives back to every logger.
	std::string _installPath;
		/// The bundle repository path that bundles are installed into.
	std::string _developmentPath;
		/// A bundle repository path other than the one that
		/// bundles are installed into.

	std::vector<std::string> _installedBundles;
};


#endif // BundleRepositoryTest_INCLUDED
