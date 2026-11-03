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
	~AuthAdminServiceTest();

	void testUsers();
	void testManyUsers();
	void testUsersWithPermission();
	void testUsersWithRole();
	void testUsersWithAttribute();
	void testRoles();
	void testPermissions();
	void testAuthenticate();
	void testAuthenticateNotification();
	void testAuthenticateV1();
	void testAuthenticateUpgradeFromV1ToV2();
	void testAuthenticateUpgradeFromV1ToV3();
	void testAuthenticateUpgradeFromV2ToV3();
	void testAuthenticateUpgradeFromV3ToV4();
	void testAuthorize();
	void testAttributes();
	void testAttributesForUser();
	void testLockOut();
	void testLockOutAfterLoginFailures();
	void testReset();
#if defined(POCO_OSP_AUTH_ENABLE_LDAP)
	void testExternalRole();
#endif
	void testRoleMappings();

	void setUp();
	void tearDown();

	static CppUnit::Test* suite();

private:
	void handleNotification(const Poco::AutoPtr<Poco::OSP::Auth::AuthNotification>& n);

	Poco::AutoPtr<Poco::Util::PropertyFileConfiguration> _pProperties;
	Poco::OSP::Auth::Data::AuthAdminServiceImpl::Ptr _pAAS;
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
