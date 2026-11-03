//
// WebServerDispatcherTest.h
//
// Definition of the WebServerDispatcherTest class.
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
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
	void testRemoveDir();
	void testRemovePatternRemovesEveryMapping();
	void testParseRange();
	void testCanonicalPathWithQuery();
	void testRedirectLocationOn401();
	void testStaticResourceHasExactLength();
	void testStaticResourceResponses();
	void testCleanPathRejectsDotSegments();
	void testDotSegmentsAnswerBadRequest();
	void testMalformedURIAnswersBadRequest();
	void testPersistentResourceStaysInsideItsDirectory();
	void testPersistentResourceEndingWithSlash();
	void testRangeRequests();
	void testClientAuthorizationHeadersAreRemoved();
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
	void testUnregisteredSessionManagerIsNotUsed();
	void testSecondFilterForMediaTypeIsRejected();
	void testLogMessagesHoldNoTokenAndNoControlCharacters();
	void testFullRequestLogHasNoCredentials();
	void testForwardedClientAddressIsAnIPAddressOnly();
	void testJSONErrorResponseEscapesRequestData();
	void testCORSOrigins();
	void testIsSameOrigin();
	void testInvalidIfModifiedSinceIsIgnored();
	void testCompressedResponses();
	void testStrictPersistentResourceIsNotReplacedOnError();
	void testUnreadRequestBodyClosesConnection();
#if defined(OSPWEB_TEST_LIBRARY)
	void testBundleLibraryStaysLoadedUntilBundleStopped();
#endif

	void setUp() override;
	void tearDown() override;

	static CppUnit::Test* suite();

private:
	struct Fixture;
	std::unique_ptr<Fixture> _pFixture;
};


#endif // WebServerDispatcherTest_INCLUDED
