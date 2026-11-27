//
// BundleAdminTest.h
//
// Definition of the BundleAdminTest class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef BundleAdminTest_INCLUDED
#define BundleAdminTest_INCLUDED


#include "CppUnit/TestCase.h"
#include <memory>
#include <string>


class BundleAdminTest: public CppUnit::TestCase
{
public:
	BundleAdminTest() = delete;
	explicit BundleAdminTest(const std::string& name);
	~BundleAdminTest() override;

	void testListPageEscapesBundleName();
	void testInfoPageEscapesBundleName();
	void testInfoPageWithoutSymbolicNameShowsUnknownBundle();
	void testInstallCompletePageEscapesBundleName();
	void testListPageEncodesSymbolicName();
	void testActionFormEscapesSymbolicName();
	void testInstallCompletePageEncodesSymbolicName();
	void testGetDoesNotChangeBundle();
	void testCrossOriginPostDoesNotChangeBundle();
	void testSameOriginPostStopsBundle();
	void testSessionRequiresCSRFToken();
	void testActionFormCarriesCSRFToken();
	void testPageIsCompleteWithoutSessionManager();
	void testErrorAfterPartOfPageIsLogged();
	void testCrossOriginUploadDoesNotInstall();
	void testUploadWithPutIsRefused();
	void testUploadInSessionRequiresCSRFToken();
	void testStopOfNeededBundleIsRefused();
	void testStopOfBundleWithModuleDependentAsksForConfirmation();
	void testNeededBundleHasNoStopButton();
	void testUpgradeOfNeededBundleIsRefused();
	void testUpgradeOfUnknownBundle();
	void testInstallErrorIsEscaped();
	void testOnlyFirstBundleFileIsInstalled();
	void testUploadSizeIsLimited();
	void testMissingInstallerService();

	void setUp() override;
	void tearDown() override;

	static CppUnit::Test* suite();

private:
	struct Fixture;
	std::unique_ptr<Fixture> _pFixture;
};


#endif // BundleAdminTest_INCLUDED
