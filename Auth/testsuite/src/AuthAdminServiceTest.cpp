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
#include "Poco/OSP/Auth/LDAPClient.h"
#include "AuthAdminServiceTest.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include <iostream>


using namespace Poco::OSP::Auth;


AuthAdminServiceTest::AuthAdminServiceTest(const std::string& name): CppUnit::TestCase(name)
{
}


AuthAdminServiceTest::~AuthAdminServiceTest()
{
}


void AuthAdminServiceTest::testEscapeLDAP()
{
	assert (AbstractLDAPAuthAdminService::escapeLDAP("admin") == "admin");
	assert (AbstractLDAPAuthAdminService::escapeLDAP("ad+min") == "ad\\+min");	
}


void AuthAdminServiceTest::testEscapeLDAPFilterValue()
{
	assert (LDAPClient::escapeFilterValue("admin") == "admin");
	assert (LDAPClient::escapeFilterValue("ad*min") == "ad\\2Amin");
	assert (LDAPClient::escapeFilterValue("ad(min)") == "ad\\28min\\29");
	assert (LDAPClient::escapeFilterValue("ad\\min") == "ad\\5Cmin");
	assert (LDAPClient::escapeFilterValue(std::string("ad\0min", 6)) == std::string("ad\\00min"));
}


void AuthAdminServiceTest::setUp()
{
}


void AuthAdminServiceTest::tearDown()
{
}


CppUnit::Test* AuthAdminServiceTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("AuthAdminServiceTest");

	CppUnit_addTest(pSuite, AuthAdminServiceTest, testEscapeLDAP);
	CppUnit_addTest(pSuite, AuthAdminServiceTest, testEscapeLDAPFilterValue);

	return pSuite;
}
