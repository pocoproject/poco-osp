//
// AuthAdminServiceTest.cpp
//
// Copyright (c) 2012, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties, 
// copied or duplicated in any form, in whole or in part.
//


#include "Poco/OSP/Auth/AbstractLDAPAuthAdminService.h"
#include "Poco/OSP/Auth/AuthService.h"
#include "Poco/AutoPtr.h"
#if defined(POCO_OSP_AUTH_ENABLE_LDAP)
#include "Poco/OSP/Auth/LDAPClient.h"
#endif
#include "AuthAdminServiceTest.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestCase.h"
#include "CppUnit/TestSuite.h"
#include <iostream>
#include <string>


using Poco::OSP::Auth::AbstractLDAPAuthAdminService;
#if defined(POCO_OSP_AUTH_ENABLE_LDAP)
using Poco::OSP::Auth::LDAPClient;
#endif
using namespace std::string_literals;


namespace
{
	class PasswordAuthService: public Poco::OSP::Auth::AuthService
		/// Knows the user "Alice" with the password "secret" and implements
		/// only authenticate().
	{
	public:
		PasswordAuthService() = default;
		PasswordAuthService(const PasswordAuthService&) = delete;
		PasswordAuthService(PasswordAuthService&&) = delete;
		PasswordAuthService& operator = (const PasswordAuthService&) = delete;
		PasswordAuthService& operator = (PasswordAuthService&&) = delete;
		~PasswordAuthService() override = default;

		bool authenticate(const std::string& userName, const std::string& credentials) const override
		{
			return userName == "Alice"s && credentials == "secret"s;
		}

		bool authorize(const std::string&, const std::string&) const override
		{
			return false;
		}

		bool authorize(const std::string&, const std::string&, const std::string&) const override
		{
			return false;
		}

		bool userExists(const std::string& userName) const override
		{
			return userName == "Alice"s;
		}
	};
}


AuthAdminServiceTest::AuthAdminServiceTest(const std::string& name): CppUnit::TestCase(name)
{
}


AuthAdminServiceTest::~AuthAdminServiceTest()
{
}


void AuthAdminServiceTest::testLDAPParamsHaveDefaultTimeout()
{
	const AbstractLDAPAuthAdminService::LDAPParams params;
	assertEqual (30, params.timeout);
}


void AuthAdminServiceTest::testDefaultAuthenticateAccountReturnsUserName()
{
	Poco::AutoPtr<PasswordAuthService> pService = new PasswordAuthService;
	std::string accountName("unchanged"s);

	assertFalse (pService->authenticateAccount("Alice"s, "wrong"s, accountName));
	assertEqual ("unchanged"s, accountName);
	assertTrue (pService->authenticateAccount("Alice"s, "secret"s, accountName));
	assertEqual ("Alice"s, accountName);
}


#if defined(POCO_OSP_AUTH_ENABLE_LDAP)
void AuthAdminServiceTest::testEscapeLDAP()
{
	assertTrue (LDAPClient::escapeLDAP("admin") == "admin");
	assertTrue (LDAPClient::escapeLDAP("ad+min") == "ad\\+min");
	assertTrue (LDAPClient::escapeLDAP(" admin") == "\\ admin");
	assertTrue (LDAPClient::escapeLDAP("admin ") == "admin\\ ");
	assertTrue (LDAPClient::escapeLDAP("ad min") == "ad min");
	assertTrue (LDAPClient::escapeLDAP(" ") == "\\ ");
	assertTrue (LDAPClient::escapeLDAP(std::string("ad\0min", 6)) == "ad\\00min");
}


void AuthAdminServiceTest::testEscapeLDAPFilterValue()
{
	assert (LDAPClient::escapeFilterValue("admin") == "admin");
	assert (LDAPClient::escapeFilterValue("ad*min") == "ad\\2Amin");
	assert (LDAPClient::escapeFilterValue("ad(min)") == "ad\\28min\\29");
	assert (LDAPClient::escapeFilterValue("ad\\min") == "ad\\5Cmin");
	assert (LDAPClient::escapeFilterValue(std::string("ad\0min", 6)) == std::string("ad\\00min"));
}
#endif


void AuthAdminServiceTest::setUp()
{
}


void AuthAdminServiceTest::tearDown()
{
}


CppUnit::Test* AuthAdminServiceTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("AuthAdminServiceTest");

	CppUnit_addTest(pSuite, AuthAdminServiceTest, testLDAPParamsHaveDefaultTimeout);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testDefaultAuthenticateAccountReturnsUserName);
#if defined(POCO_OSP_AUTH_ENABLE_LDAP)
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testEscapeLDAP);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testEscapeLDAPFilterValue);
#endif

	return pSuite;
}
