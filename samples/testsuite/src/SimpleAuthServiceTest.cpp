//
// SimpleAuthServiceTest.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "SimpleAuthServiceTest.h"
#include "TestApplication.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "CppUnit/TestCase.h"
#include "CppUnit/Test.h"
#include "Poco/OSP/Auth/AuthService.h"
#include "Poco/OSP/Bundle.h"
#include "Poco/OSP/Properties.h"
#include "Poco/OSP/Service.h"
#include "Poco/OSP/ServiceRef.h"
#include "Poco/AutoPtr.h"
#include <string>
#include <typeinfo>


using Poco::OSP::Auth::AuthService;
using namespace std::string_literals;


namespace
{
	const std::string SAMPLE_BUNDLE("com.appinf.osp.samples.auth"s);
	const std::string AUTH_SERVICE("osp.auth"s);

	class OtherAuthService: public Poco::OSP::Service
		/// Stands for a service that another bundle registers under the same name.
	{
	public:
		OtherAuthService() = default;
		~OtherAuthService() override = default;

		OtherAuthService(const OtherAuthService&) = delete;
		OtherAuthService(OtherAuthService&&) = delete;
		OtherAuthService& operator = (const OtherAuthService&) = delete;
		OtherAuthService& operator = (OtherAuthService&&) = delete;

		[[nodiscard]] const std::type_info& type() const override
		{
			return typeid(OtherAuthService);
		}

		[[nodiscard]] bool isA(const std::type_info& otherType) const override
		{
			return otherType == typeid(OtherAuthService) || Poco::OSP::Service::isA(otherType);
		}
	};
}


SimpleAuthServiceTest::SimpleAuthServiceTest(const std::string& name): CppUnit::TestCase(name)
{
}


SimpleAuthServiceTest::~SimpleAuthServiceTest() = default;


void SimpleAuthServiceTest::testServiceIsNotRegisteredWithoutAdminPassword()
{
	TestApplication::Ptr pApp = new TestApplication({SAMPLE_BUNDLE});
	pApp->start();

	assertNull (pApp->serviceRegistry().findByName(AUTH_SERVICE).get());
}


void SimpleAuthServiceTest::testUserCannotLogInWithoutPassword()
{
	TestApplication::Ptr pApp = new TestApplication({SAMPLE_BUNDLE});
	pApp->config().setString("adminPassword"s, "admin-secret"s);
	pApp->start();

	AuthService::Ptr pAuth = pApp->serviceRegistry().findByName(AUTH_SERVICE)->castedInstance<AuthService>();
	assertFalse (pAuth->authenticate("user"s, "anything"s));
	assertFalse (pAuth->authenticate("user"s, ""s));
}


void SimpleAuthServiceTest::testPasswordsAreChecked()
{
	TestApplication::Ptr pApp = new TestApplication({SAMPLE_BUNDLE});
	pApp->config().setString("adminPassword"s, "admin-secret"s);
	pApp->config().setString("userPassword"s, "user-secret"s);
	pApp->start();

	AuthService::Ptr pAuth = pApp->serviceRegistry().findByName(AUTH_SERVICE)->castedInstance<AuthService>();
	assertTrue (pAuth->authenticate("admin"s, "admin-secret"s));
	assertFalse (pAuth->authenticate("admin"s, "admin"s));
	assertFalse (pAuth->authenticate("admin"s, "user-secret"s));
	assertTrue (pAuth->authenticate("user"s, "user-secret"s));
	assertFalse (pAuth->authenticate("user"s, "admin-secret"s));
	assertFalse (pAuth->authenticate("other"s, "admin-secret"s));
}


void SimpleAuthServiceTest::testUserHasNoPermissionsByDefault()
{
	TestApplication::Ptr pApp = new TestApplication({SAMPLE_BUNDLE});
	pApp->config().setString("adminPassword"s, "admin-secret"s);
	pApp->start();

	AuthService::Ptr pAuth = pApp->serviceRegistry().findByName(AUTH_SERVICE)->castedInstance<AuthService>();
	assertFalse (pAuth->authorize("user"s, "admin"s));
	assertTrue (pAuth->authorize("admin"s, "admin"s));
}


void SimpleAuthServiceTest::testRestartWithoutAdminPasswordKeepsOtherAuthService()
{
	TestApplication::Ptr pApp = new TestApplication({SAMPLE_BUNDLE});
	pApp->config().setString("adminPassword"s, "admin-secret"s);
	pApp->start();
	Poco::OSP::Bundle::Ptr pSample = pApp->bundleLoader().findBundle(SAMPLE_BUNDLE);
	pSample->stop();
	pApp->config().remove("adminPassword"s);
	pSample->start();
	pApp->serviceRegistry().registerService(AUTH_SERVICE, Poco::makeAuto<OtherAuthService>(), Poco::OSP::Properties());

	pSample->stop();

	assertNotNull (pApp->serviceRegistry().findByName(AUTH_SERVICE).get());
}


CppUnit::Test* SimpleAuthServiceTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("SimpleAuthServiceTest");

	CppUnit_addTest(pSuite, SimpleAuthServiceTest, testServiceIsNotRegisteredWithoutAdminPassword);
	CppUnit_addTest(pSuite, SimpleAuthServiceTest, testUserCannotLogInWithoutPassword);
	CppUnit_addTest(pSuite, SimpleAuthServiceTest, testPasswordsAreChecked);
	CppUnit_addTest(pSuite, SimpleAuthServiceTest, testUserHasNoPermissionsByDefault);
	CppUnit_addTest(pSuite, SimpleAuthServiceTest, testRestartWithoutAdminPasswordKeepsOtherAuthService);

	return pSuite;
}
