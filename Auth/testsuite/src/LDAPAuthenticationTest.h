//
// LDAPAuthenticationTest.h
//
// Definition of the LDAPAuthenticationTest class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef LDAPAuthenticationTest_INCLUDED
#define LDAPAuthenticationTest_INCLUDED


#include "Poco/OSP/Auth/Auth.h"
#include "CppUnit/TestCase.h"


#if defined(POCO_OSP_AUTH_ENABLE_LDAP)


class LDAPAuthenticationTest: public CppUnit::TestCase
{
public:
	LDAPAuthenticationTest() = delete;
	explicit LDAPAuthenticationTest(const std::string& name);
	~LDAPAuthenticationTest() override;

	void testEmptyPasswordIsRejectedWithoutBind();
	void testNulCharacterIsRejectedWithoutBind();
	void testServiceBindWithoutBindPasswordIsDirectoryError();
	void testRejectedServiceAccountIsDirectoryError();
	void testWrongPasswordIsRejected();
	void testRefusedUserBindDoesNotFallBack();
	void testAcceptedUserIsStored();
	void testBindDNIsBuiltFromBindDN();
	void testBindDNFallsBackToSearchBase();
	void testServiceBindModeVerifiesFoundUserDN();
	void testUnknownUserInServiceBindModeIsRejected();
	void testUserBindModeBindsWithAccountName();
	void testServiceBindModeReadsAccountNameBeforeUserBind();
	void testServiceBindModeKeepsAccountNameOfSearch();
	void testLockedOutAccountIsNotBoundInServiceBindMode();
	void testUserBindModeReadsAccountNameAfterBind();
	void testUserBindModeRefusesNameThatEntryDoesNotHave();
	void testLockedOutAccountIsRefusedAfterBindInUserBindMode();
	void testAccountWithoutAccountNameIsCheckedUnderGivenName();
	void testAcceptedUserIsStoredUnderAccountName();
	void testRoleFilterSelectsImportedRoles();
	void testLocalRolesAreNotImported();
	void testLocalRoleIsNotOverwritten();
	void testUnreadableUserEntryAfterBindThrows();
	void testUnreadableGroupAfterBindThrows();
	void testInaccessibleServerFallsBackWithinLifetime();
	void testUnreachableServerFallsBack();
	void testUnansweredServerFallsBackToLocalCredentials();
	void testTimeoutOfZeroIsReplacedByDefault();

	void setUp() override;
	void tearDown() override;

	static CppUnit::Test* suite();
};


#endif // POCO_OSP_AUTH_ENABLE_LDAP


#endif // LDAPAuthenticationTest_INCLUDED
