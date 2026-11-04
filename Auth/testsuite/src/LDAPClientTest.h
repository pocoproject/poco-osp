//
// LDAPClientTest.h
//
// Definition of the LDAPClientTest class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef LDAPClientTest_INCLUDED
#define LDAPClientTest_INCLUDED


#include "Poco/OSP/Auth/Auth.h"
#include "CppUnit/TestCase.h"


#if defined(POCO_OSP_AUTH_ENABLE_LDAP)


class LDAPClientTest: public CppUnit::TestCase
{
public:
	LDAPClientTest() = delete;
	explicit LDAPClientTest(const std::string& name);
	~LDAPClientTest() override;

	void testEmptyOrNulPasswordIsNotSentToServer();
	void testUnansweredBindTimesOut();
	void testBindResultsAreMapped();
	void testFindUserDNNeedsExactlyOneEntry();
	void testFailedSearchIsReported();
	void testAttributeValuesKeepNulCharacters();
	void testAtMostEightExtraAttributesAreRequested();
	void testAccountNameIsReadInAccountMode();
	void testSeveralAccountNamesAreDirectoryError();
	void testPrincipalNamesAreReadInAccountMode();
	void testUserSearchRequestsPermissionAttributeOfUsers();
	void testGroupWithoutEntryIsNotRead();

	void setUp() override;
	void tearDown() override;

	static CppUnit::Test* suite();
};


#endif // POCO_OSP_AUTH_ENABLE_LDAP


#endif // LDAPClientTest_INCLUDED
