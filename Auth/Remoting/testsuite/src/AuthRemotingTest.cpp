//
// AuthRemotingTest.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "AuthRemotingTest.h"
#include "TemporaryBundle.h"
#include "ServerRequestStub.h"
#include "Poco/OSP/Auth/Remoting/Authenticator.h"
#include "Poco/OSP/Auth/Remoting/Authorizer.h"
#include "Poco/OSP/Auth/RequestIdentity.h"
#include "Poco/OSP/Auth/AuthService.h"
#include "Poco/OSP/Properties.h"
#include "Poco/OSP/ServiceRegistry.h"
#include "Poco/RemotingNG/Authenticator.h"
#include "Poco/RemotingNG/Context.h"
#include "Poco/RemotingNG/Credentials.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "CppUnit/TestCase.h"
#include "CppUnit/Test.h"
#include <memory>
#include <string>
#include <thread>


using Poco::OSP::Auth::RequestIdentity;
using Poco::OSP::Auth::Remoting::Authenticator;
using Poco::OSP::Auth::Remoting::Authorizer;
using Poco::RemotingNG::AuthenticateResult;
using namespace std::string_literals;


namespace
{
	class TestAuthService: public Poco::OSP::Auth::AuthService
		/// Grants the user "alice" the permissions "read" and "write";
		/// the scope "readonly" grants only "read".
	{
	public:
		[[nodiscard]] bool authenticate(const std::string& /*userName*/,
			const std::string& /*credentials*/) const override
		{
			return false;
		}

		[[nodiscard]] bool authorize(const std::string& userName, const std::string& permission) const override
		{
			return userName == "alice" && (permission == "read" || permission == "write");
		}

		[[nodiscard]] bool authorize(const std::string& userName, const std::string& scope,
			const std::string& permission) const override
		{
			return authorize(userName, permission) && (scope != "readonly" || permission == "read");
		}

		[[nodiscard]] bool userExists(const std::string& userName) const override
		{
			return userName == "alice";
		}
	};

	class RemotingRequest
		/// A request with an X-OSP-Authorized-User header field that a client has sent, in the
		/// RemotingNG context of the current thread, as the RemotingNG HTTP transports set it up.
	{
	public:
		RemotingRequest():
			_request("127.0.0.1:50000"s, false)
		{
			_request.set("X-OSP-Authorized-User"s, "alice"s);
			_context.context()->setValue("httpRequest"s, static_cast<Poco::Net::HTTPServerRequest*>(&_request));
		}

	private:
		ServerRequestStub _request;
		Poco::RemotingNG::ScopedContext _context;
	};
}


struct AuthRemotingTest::Fixture
{
	Fixture():
		bundles(""s)
	{
	}

	TemporaryBundle bundles;
};


AuthRemotingTest::AuthRemotingTest(const std::string& name): CppUnit::TestCase(name)
{
}


AuthRemotingTest::~AuthRemotingTest() = default;


void AuthRemotingTest::testRequestIdentitiesNest()
{
	assertTrue (RequestIdentity::current() == nullptr);
	{
		const RequestIdentity outer("alice"s, ""s);
		assertTrue (RequestIdentity::current() == &outer);
		{
			const RequestIdentity inner("bob"s, "api"s);
			assertEqual ("bob"s, RequestIdentity::current()->user());
			assertEqual ("api"s, RequestIdentity::current()->scope());
		}
		assertTrue (RequestIdentity::current() == &outer);
		assertEqual ("alice"s, RequestIdentity::current()->user());
	}
	assertTrue (RequestIdentity::current() == nullptr);
}


void AuthRemotingTest::testRequestIdentityBelongsToItsThread()
{
	const RequestIdentity identity("alice"s, ""s);
	const RequestIdentity* pOtherThreadIdentity = &identity;
	std::thread other([&pOtherThreadIdentity]()
	{
		pOtherThreadIdentity = RequestIdentity::current();
	});
	other.join();

	assertTrue (pOtherThreadIdentity == nullptr);
}


void AuthRemotingTest::testRequestWithoutIdentityIsNotAuthenticated()
{
	const RemotingRequest request;
	Authenticator authenticator;

	const AuthenticateResult result = authenticator.authenticate(Poco::RemotingNG::Credentials());

	assertTrue (result.state() == AuthenticateResult::AUTH_FAILED);
}


void AuthRemotingTest::testRequestIdentityIsAuthenticated()
{
	const RequestIdentity identity("alice"s, ""s);
	Authenticator authenticator;

	const AuthenticateResult result = authenticator.authenticate(Poco::RemotingNG::Credentials());

	assertTrue (result.state() == AuthenticateResult::AUTH_DONE);
	assertEqual ("alice"s, result.credentials().getAttribute(Poco::RemotingNG::Credentials::ATTR_USERNAME));
}


void AuthRemotingTest::testRequestWithoutIdentityIsNotAuthorized()
{
	_pFixture->bundles.registry().registerService("osp.auth"s, new TestAuthService, Poco::OSP::Properties());
	const RemotingRequest request;
	Authorizer authorizer(_pFixture->bundles.context());

	assertFalse (authorizer.authorize("method"s, "read"s));
}


void AuthRemotingTest::testAuthorizerWithoutAuthServiceDenies()
{
	const RemotingRequest request;
	const RequestIdentity identity("alice"s, ""s);
	Authorizer authorizer(_pFixture->bundles.context());

	assertFalse (authorizer.authorize("method"s, "read"s));
}


void AuthRemotingTest::testAuthorizerChecksUserAndScope()
{
	_pFixture->bundles.registry().registerService("osp.auth"s, new TestAuthService, Poco::OSP::Properties());
	Authorizer authorizer(_pFixture->bundles.context());
	{
		const RequestIdentity identity("alice"s, ""s);
		assertTrue (authorizer.authorize("method"s, "write"s));
		assertFalse (authorizer.authorize("method"s, "admin"s));
	}
	{
		const RequestIdentity identity("alice"s, "readonly"s);
		assertTrue (authorizer.authorize("method"s, "read"s));
		assertFalse (authorizer.authorize("method"s, "write"s));
	}
	{
		const RequestIdentity identity("bob"s, ""s);
		assertFalse (authorizer.authorize("method"s, "read"s));
	}
}


void AuthRemotingTest::setUp()
{
	_pFixture = std::make_unique<Fixture>();
}


void AuthRemotingTest::tearDown()
{
	_pFixture.reset();
}


CppUnit::Test* AuthRemotingTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("AuthRemotingTest");

	CppUnit_addTest(pSuite, AuthRemotingTest, testRequestIdentitiesNest);
	CppUnit_addTest(pSuite, AuthRemotingTest, testRequestIdentityBelongsToItsThread);
	CppUnit_addTest(pSuite, AuthRemotingTest, testRequestWithoutIdentityIsNotAuthenticated);
	CppUnit_addTest(pSuite, AuthRemotingTest, testRequestIdentityIsAuthenticated);
	CppUnit_addTest(pSuite, AuthRemotingTest, testRequestWithoutIdentityIsNotAuthorized);
	CppUnit_addTest(pSuite, AuthRemotingTest, testAuthorizerWithoutAuthServiceDenies);
	CppUnit_addTest(pSuite, AuthRemotingTest, testAuthorizerChecksUserAndScope);

	return pSuite;
}
