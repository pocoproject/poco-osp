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
#include "CppUnit/TestCase.h"


class AuthAdminServiceTest: public CppUnit::TestCase
{
public:
	AuthAdminServiceTest(const std::string& name);
	~AuthAdminServiceTest() override;
	
	void testLDAPParamsHaveDefaultTimeout();
	void testDefaultAuthenticateAccountReturnsUserName();
#if defined(POCO_OSP_AUTH_ENABLE_LDAP)
	void testEscapeLDAP();
	void testEscapeLDAPFilterValue();
#endif

	void setUp() override;
	void tearDown() override;

	static CppUnit::Test* suite();
};


#endif // AuthAdminServiceTest_INCLUDED
