//
// BundleManifestTest.h
//
// Definition of the BundleManifestTest class.
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#ifndef BundleManifestTest_INCLUDED
#define BundleManifestTest_INCLUDED


#include "Poco/OSP/OSP.h"
#include "CppUnit/TestCase.h"


class BundleManifestTest: public CppUnit::TestCase
{
public:
	BundleManifestTest(const std::string& name);
	~BundleManifestTest();

	void testBasicPropertiesAreRead();
	void testMissingPropertiesAreEmptyAndLazyStartIsRead();
	void testActivatorWithoutLibrary();
	void testActivatorWithLibrary();
	void testActivatorNamespacedClassAndSpaces();
	void testRequiredBundleWithoutVersionMatchesAny();
	void testRequiredBundleSingleVersion();
	void testRequiredBundleSingleVersionWithSpaces();
	void testRequiredBundleVersionAppliesToItsBundle();
	void testRequiredBundleVersionBetweenBundlesWithoutVersion();
	void testRequiredBundleRangeIncludingLowerBound();
	void testRequiredBundleRangeIncludingUpperBound();
	void testRequiredBundleOpenRange();
	void testRequiredBundleWhitespaceIgnored();
	void testRequiredModuleWithoutVersionMatchesAny();
	void testRequiredModuleSingleVersion();
	void testRequiredModuleSingleVersionWithSpaces();
	void testRequiredModuleVersionAppliesToItsModule();
	void testProvidedModuleWithVersion();
	void testProvidedModules();
	void testProvidedModulesWhitespaceIgnored();
	void testProvidedModuleWithoutVersionIsRejected();
	void testSymbolicNameThatIsNoFileNameIsRejected();
	void testSymbolicNameWithUnderscoreIsAccepted();
	void testVersionDesignationThatIsNoFileNameIsRejected();
	void testActivatorLibraryThatIsNoFileNameIsRejected();
	void testReversedVersionRangeIsRejected();
	void testEmptyBundleVersionIsRejected();
	void testMissingRequiredHeaderIsRejected();
	void testMalformedVersionIsRejected();
	void testVersionRangeThatIsNotClosedIsRejected();
	void testVersionRangeWithoutVersionIsRejected();
	void testItemsNotSeparatedByCommaAreRejected();
	void testItemWithoutNameIsRejected();
	void testFlagsRunLevelAndExtendedBundle();
	void testUnknownHeaderIsIgnoredAndLinesContinue();

	void setUp();
	void tearDown();

	static CppUnit::Test* suite();

private:
};


#endif // BundleManifestTest_INCLUDED
