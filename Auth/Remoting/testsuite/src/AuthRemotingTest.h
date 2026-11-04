//
// AuthRemotingTest.h
//
// Definition of the AuthRemotingTest class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef AuthRemotingTest_INCLUDED
#define AuthRemotingTest_INCLUDED


#include "CppUnit/TestCase.h"
#include <memory>
#include <string>


class AuthRemotingTest: public CppUnit::TestCase
{
public:
	AuthRemotingTest() = delete;
	explicit AuthRemotingTest(const std::string& name);
	~AuthRemotingTest() override;

	void testRequestIdentitiesNest();
	void testRequestIdentityBelongsToItsThread();
	void testRequestWithoutIdentityIsNotAuthenticated();
	void testRequestIdentityIsAuthenticated();
	void testRequestWithoutIdentityIsNotAuthorized();
	void testAuthorizerWithoutAuthServiceDenies();
	void testAuthorizerChecksUserAndScope();

	void setUp() override;
	void tearDown() override;

	static CppUnit::Test* suite();

private:
	struct Fixture;
	std::unique_ptr<Fixture> _pFixture;
};


#endif // AuthRemotingTest_INCLUDED
