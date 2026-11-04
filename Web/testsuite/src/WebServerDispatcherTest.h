//
// WebServerDispatcherTest.h
//
// Definition of the WebServerDispatcherTest class.
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef WebServerDispatcherTest_INCLUDED
#define WebServerDispatcherTest_INCLUDED


#include "Poco/OSP/Web/Web.h"
#include "CppUnit/TestCase.h"
#include <memory>


class WebServerDispatcherTest: public CppUnit::TestCase
{
public:
	WebServerDispatcherTest(const std::string& name);
	~WebServerDispatcherTest() override;

	void testVirtualDirectoryRoot();
	void testVirtualDirectoryAllow();
	void testVirtualDirectoryFail();
	void testChildInOtherCaseConflictsWithParent();
	void testRemoveDir();
	void testRemovePatternRemovesEveryMapping();
	void testParseRange();
	void testCanonicalPathWithQuery();
	void testRedirectLocationOn401();
	void testStaticResourceHasExactLength();
	void testCachedResourceOutlivesBundleStop();
	void testStaticResourceResponses();
	void testCleanPathRejectsDotSegments();
	void testCleanPathRejectsControlCharacters();
	void testControlCharacterInPathAnswersBadRequest();
	void testCleanPathRejectsSegmentsEndingWithDotOrSpace();
	void testCaseVariantOfProtectedPathIsProtected();
	void testNonASCIIVariantOfProtectedPathIsProtected();
	void testLongerFormOfProtectedPathIsProtected();
	void testFoldedSpellingOfProtectedPathIsNotFound();
	void testExactMatchIgnoresCase();
	void testResourcePathRejectsOtherNamesOfFiles();
	void testColonAnswersBadRequestForResourcesOnly();
	void testDotSegmentsAnswerBadRequest();
	void testMalformedURIAnswersBadRequest();
	void testPersistentResourceStaysInsideItsDirectory();
	void testPersistentResourceEndingWithSlash();
	void testPersistentResourceWithEmptySegment();
	void testPersistentResourceIsFoundOnlyUnderItsName();
	void testRangeRequests();
	void testClientAuthorizationHeadersAreRemoved();
	void testRequestIdentityIsSetWhileHandlerRuns();
	void testAuthorizationDoesNotBlockOtherRequests();
	void testUnavailableAuthServiceAnswers503();
	void testUnavailableAuthServiceIsLoggedOnce();
	void testOptionalCredentialsWithoutAuthService();
	void testOptionalCredentialsWithSessionWithoutAuthService();
	void testUnavailableAnswerHasBundleMessage();
	void testAuthorizationResponses();
	void testBasicCredentialsAreAuthorizedUnderAccountName();
	void testSessionCSRFProtection();
	void testSessionUserWithoutPermissionIsForbidden();
	void testSessionUserWithoutPermissionForHomePageIsNotAuthorized();
	void testDisabledUserIsNotAuthenticated();
	void testUnregisteredSessionManagerIsNotUsed();
	void testSecondFilterForMediaTypeIsRejected();
	void testLogMessagesHoldNoTokenAndNoControlCharacters();
	void testLoggedRequestDataHasNoControlCharacters();
	void testFullRequestLogEscapesMethodAndFieldValue();
	void testFullResponseLogEscapesFieldValue();
	void testFullResponseLogEscapesReasonAndFieldOfHandler();
	void testBadRequestDebugMessageHasNoControlCharacters();
	void testFullRequestLogHasNoCredentials();
	void testFullRequestLogRedactsFieldsWithCredentialOrNonTokenNames();
	void testFullRequestLogKeepsRequestLineWithFieldNameAsMethod();
	void testForwardedClientAddressIsAnIPAddressOnly();
	void testJSONErrorResponseEscapesRequestData();
	void testCORSOrigins();
	void testOriginDependentResponsesVaryByOrigin();
	void testIsSameOrigin();
	void testInvalidIfModifiedSinceIsIgnored();
	void testChangedResourceOfDirectoryBundleIsModified();
	void testCompressedResponses();
	void testStrictPersistentResourceIsNotReplacedOnError();
	void testUnreadRequestBodyClosesConnection();
#if defined(OSPWEB_TEST_LIBRARY)
	void testBundleLibraryStaysLoadedWhileSessionsExist();
	void testValueInSessionOfOtherBundleOutlivesBundleStop();
	void testFilterLibraryStaysLoadedWhileSessionsExist();
	void testRequestDuringBundleStopKeepsLibraryForItsSession();
	void testRequestDuringFilterRemovalKeepsLibraryForItsSession();
	void testUninstallingBundleRemovesSessionsThatKeepItsLibrary();
#endif

	void setUp() override;
	void tearDown() override;

	static CppUnit::Test* suite();

private:
	struct Fixture;
	std::unique_ptr<Fixture> _pFixture;
};


#endif // WebServerDispatcherTest_INCLUDED
