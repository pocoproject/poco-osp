//
// GreetingServiceTest.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "GreetingServiceTest.h"
#include "TestApplication.h"
#include "GreetingService.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "CppUnit/TestCase.h"
#include "CppUnit/Test.h"
#include "Poco/OSP/ServiceRef.h"
#include <string>


using namespace std::string_literals;


GreetingServiceTest::GreetingServiceTest(const std::string& name): CppUnit::TestCase(name)
{
}


GreetingServiceTest::~GreetingServiceTest() = default;


void GreetingServiceTest::testGreetingIsInTheConfiguredLanguage()
{
	assertEqual ("Hello"s, greeting("en-US"s));
	assertEqual ("Servus"s, greeting("de-AT"s));
	assertEqual ("Hej"s, greeting("sv_SE.UTF-8"s));
	assertEqual ("\xE3\x81\x93\xE3\x82\x93\xE3\x81\xAB\xE3\x81\xA1\xE3\x81\xAF"s, greeting("ja_JP.UTF-8"s));
	assertEqual ("\xE4\xBD\xA0\xE5\xA5\xBD"s, greeting("zh_CN.UTF-8"s));
}


std::string GreetingServiceTest::greeting(const std::string& language)
{
	TestApplication::Ptr pApp = new TestApplication({"com.appinf.osp.samples.greetingservice"s});
	pApp->config().setString("osp.language"s, language);
	pApp->start();

	Poco::OSP::ServiceRef::Ptr pServiceRef = pApp->serviceRegistry().findByName("com.appinf.osp.samples.GreetingService"s);
	assertNotNull (pServiceRef.get());
	return pServiceRef->castedInstance<GreetingService>()->greeting();
}


CppUnit::Test* GreetingServiceTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("GreetingServiceTest");

	CppUnit_addTest(pSuite, GreetingServiceTest, testGreetingIsInTheConfiguredLanguage);

	return pSuite;
}
