//
// WebEventRequestHandlerTest.h
//
// Definition of the WebEventRequestHandlerTest class.
//
// Copyright (c) 2026, Aleph ONE Software Engineering LLC
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef WebEventRequestHandlerTest_INCLUDED
#define WebEventRequestHandlerTest_INCLUDED


#include "CppUnit/TestCase.h"
#include <string>


class WebEventRequestHandlerTest: public CppUnit::TestCase
	/// Tests the WebEvent endpoint as the web server serves it: the request
	/// handler behind a WebServerDispatcher, mapped with the permission and
	/// the web session that the bundle's extension declares.
{
public:
	WebEventRequestHandlerTest() = delete;
	explicit WebEventRequestHandlerTest(const std::string& name);
	~WebEventRequestHandlerTest() override;

	void testExtensionRequiresAuthentication();
	void testExtensionTakesSettingsFromConfiguration();
	void testUnauthenticatedUpgradeIsRefused();
	void testUnauthorizedUpgradeIsRefused();
	void testUpgradeWithoutPermissionIsRefused();
	void testUpgradeWithSessionCookieIsAdmitted();
	void testUpgradeWithSessionCookieFromForeignOriginIsRefused();
	void testUpgradeWithoutWebSessionIgnoresCookie();
	void testUpgradeWithBasicCredentialsIsAdmitted();
	void testUpgradeWithBearerTokenIsAdmitted();
	void testEmptyPermissionAdmitsUnauthenticatedConnection();
	void testEmptyPermissionKeepsOriginCheck();
	void testOptionalCredentialsPermissionAdmitsRequestWithoutCredentials();
	void testConnectionBelongsToAuthenticatedUser();
	void testUserResolverReplacesAuthenticatedUser();
	void testScopedTokenConnectionIsNotReachedByNotifyUser();
	void testResolvedUserOfScopedConnectionKeepsScope();
	void testInitialDataRequestNamesUserAndScope();

	void setUp() override;
	void tearDown() override;

	static CppUnit::Test* suite();
};


#endif // WebEventRequestHandlerTest_INCLUDED
