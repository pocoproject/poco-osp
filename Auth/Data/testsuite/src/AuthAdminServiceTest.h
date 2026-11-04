//
// AuthAdminServiceTest.h
//
// Definition of the AuthAdminServiceImpl class.
//
// Copyright (c) 2012, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties,
// copied or duplicated in any form, in whole or in part.
//


#ifndef AuthAdminServiceTest_INCLUDED
#define AuthAdminServiceTest_INCLUDED


#include "Poco/OSP/Auth/Auth.h"
#include "Poco/OSP/Auth/AuthNotification.h"
#include "Poco/OSP/Auth/Data/AuthAdminServiceImpl.h"
#include "Poco/Util/PropertyFileConfiguration.h"
#include "Poco/AutoPtr.h"
#include "CppUnit/TestCase.h"


class AuthAdminServiceTest: public CppUnit::TestCase
{
public:
	AuthAdminServiceTest(const std::string& name, bool testInlineAttributes = false);
	~AuthAdminServiceTest() override;

	void testUsers();
	void testDatabaseWithoutUsersIsReachable();
	void testManyUsers();
	void testUsersWithNameMatchesSpecialCharacters();
	void testUsersWithPermission();
	void testUsersWithRole();
	void testUsersWithAttribute();
	void testUserQueriesReturnRequestedPage();
	void testRoles();
	void testRemoveRoleRevokesItsPermissions();
	void testPermissions();
	void testReplacePermissionsOfUnknownNameThrows();
	void testAuthenticate();
	void testAuthenticateNotification();
	void testLoginNotificationIsNotLoggedAsInformation();
	void testLoginOfUserRemovedBeforeNotificationSucceeds();
	void testChangePasswordOfUnknownUserThrows();
	void testEmptyPasswordIsRejected();
	void testPasswordChangedBehindCacheIsAccepted();
	void testAuthorizeProceedsWhilePasswordIsChecked();
	void testPasswordChangedDuringCheckRefusesLogin();
	void testAuthorizeProceedsWhileNewPasswordIsHashed();
	void testBrokenSessionWithOpenTransactionIsReplaced();
	void testAuthenticateV1();
	void testAuthenticateUpgradeFromV1ToV2();
	void testAuthenticateUpgradeFromV1ToV3();
	void testAuthenticateUpgradeFromV2ToV3();
	void testAuthenticateUpgradeFromV3ToV4();
	void testAuthenticateUpgradeFromV2ToV4();
	void testAuthenticateRaisesIterationCount();
	void testIterationsOfEmptyPasswordHashAreZero();
	void testRandomSaltsKeepWhitespaceBytes();
	void testInvalidHashSettingsAreRejected();
	void testAuthorize();
	void testLongCacheTimeoutKeepsCachedPermissions();
	void testAttributes();
	void testAuthAttributesAreChangedOnlyThroughAuthInterface();
	void testAttributesForUser();
	void testAttributeWithAuthPrefixWithoutUnderscoreIsUserAttribute();
	void testLockOut();
	void testLockOutAfterLoginFailures();
	void testCachedLoginResetsFailureCount();
	void testNeverLockOutUsersAreNotLockedOut();
	void testValidateUserCanDisableUser();
	void testMalformedLockOutTimeIsLoggedAndIgnored();
	void testLockOutWithNullableFailureCount();
	void testMissingFailureCountColumnIsLogged();
	void testLockOutWithShippedSchema();
	void testPoolRevokePermissionClearsEveryBackend();
	void testPoolRoutesUserNamesIgnoringCase();
	void testPoolAuthenticatesAccountWithBackendOfUser();
	void testPoolForwardsValidateUserWithSender();
	void testReset();
#if defined(POCO_OSP_AUTH_ENABLE_LDAP)
	void testExternalRole();
	void testAdminOfShippedDatabaseIsLocalUserWithLDAP();
	void testLDAPPasswordChangeUpdatesStoredHash();
	void testLocalUserWithCapitalLettersIsNotLDAPUser();
	void testOtherCallsProceedDuringLDAPLogin();
	void testConcurrentLDAPLoginsOfOneUser();
	void testLDAPRoundTripsOfOneUserAreSequential();
	void testConcurrentLDAPAttemptsAreLimitedByFailureCount();
	void testLDAPSuccessDoesNotOverwriteAccountCreatedMeanwhile();
	void testLDAPRejectionKeepsPasswordOfAccountCreatedMeanwhile();
	void testLockOutDuringLDAPLoginDiscardsLDAPAnswer();
	void testLDAPRoundTripsOfDifferentUsersRunInParallel();
	void testLoginWaitingTooLongForOtherLDAPLoginIsRefused();
	void testRejectedLDAPPasswordKeepsUserAndClearsLocalPassword();
	void testRejectedLDAPLoginKeepsPasswordNotStoredByLDAP();
	void testRejectedLDAPPasswordsLockOutUser();
	void testUnreachableLDAPServerKeepsLocalPassword();
	void testLDAPDirectoryProblemKeepsLocalPassword();
	void testLDAPDirectoryErrorWithoutLocalFallbackRefusesStoredPassword();
	void testLDAPDirectoryErrorBeforeUserBindCountsWrongPasswords();
	void testLDAPDirectoryErrorAfterAcceptedPasswordCountsNoFailure();
	void testRefusedLDAPBindCountsFailureAndKeepsPassword();
	void testInaccessibleLDAPServerKeepsStoredPassword();
	void testDatabaseErrorWhileStoringLDAPUserRollsBack();
	void testSessionTimeoutDuringLDAPLoginKeepsSession();
	void testLostDatabaseConnectionDuringLDAPLoginStoresNothing();
	void testAuthorizeProceedsWhileLDAPLoginChecksPassword();
	void testPasswordChangedDuringLDAPLoginCheckRefusesLogin();
	void testLDAPLoginUpdatesRolePermissionsOfOtherUsers();
	void testLoginNamesOfDirectoryUserShareOneAccount();
	void testFailedLoginsThroughEveryNameLockOutAccount();
	void testLockedOutAccountIsNotTriedThroughOtherName();
	void testLockOutDuringLoginThroughOtherNameDiscardsLDAPAnswer();
	void testUserBindModeStoresLoginUnderAccountName();
	void testUserBindModeRefusesNameThatEntryDoesNotHave();
	void testUserBindModeAcceptsDomainNameOfEntry();
	void testOutageAfterSearchChecksStoredPasswordOfAccount();
	void testLoginWithoutAccountNameIsStoredUnderTypedName();
	void testAuthenticateRefusesOtherNameThanAccountName();
	void testVerifyCredentialsRefusesOtherNameThanAccountName();
	void testDatabaseErrorWhileCheckingAccountIsReported();
	void testAccountNameOfLocalUserIsRefused();
	void testEntryWithSeveralAccountNamesIsDirectoryError();
	void testLoginThroughOtherNameWaitsForLoginOfAccount();
	void testLoginThroughOtherNameWaitingTooLongIsRefused();
	void testConcurrentLoginsThroughNamesOfAccountAreLimitedByFailureCount();
#endif
	void testRoleMappings();
	void testInvalidateCachesClearsPermissionsByDefault();
	void testValidateUserEventIsInitialized();

	void setUp() override;
	void tearDown() override;

	static CppUnit::Test* suite();

private:
	void handleNotification(const Poco::AutoPtr<Poco::OSP::Auth::AuthNotification>& n);

	Poco::AutoPtr<Poco::Util::PropertyFileConfiguration> _pProperties;
	Poco::OSP::Auth::Data::AuthAdminServiceImpl::Ptr _pAAS;
	std::string _dbPath;
		/// The SQLite database of the test, with a unique name in the temporary directory.
	std::string _testName;
	bool _testInlineAttributes;
	static bool _firstRun;
	int _notificationCounter {0};
};


class InlineAttributesAuthAdminServiceTest: public AuthAdminServiceTest
{
public:
	InlineAttributesAuthAdminServiceTest(const std::string& name);

	static CppUnit::Test* suite();
};


#endif // AuthAdminServiceTest_INCLUDED
