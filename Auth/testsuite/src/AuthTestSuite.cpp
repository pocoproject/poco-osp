//
// AuthTestSuite.cpp
//
// Copyright (c) 2012, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "AuthTestSuite.h"
#include "AuthAdminServiceTest.h"
#include "LDAPAuthenticationTest.h"
#include "LDAPClientTest.h"


CppUnit::Test* AuthTestSuite::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("AuthTestSuite");

	pSuite->addTest(AuthAdminServiceTest::suite());
#if defined(POCO_OSP_AUTH_ENABLE_LDAP)
	pSuite->addTest(LDAPAuthenticationTest::suite());
	pSuite->addTest(LDAPClientTest::suite());
#endif

	return pSuite;
}
