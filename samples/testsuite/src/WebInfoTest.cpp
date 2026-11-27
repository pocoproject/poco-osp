//
// WebInfoTest.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "WebInfoTest.h"
#include "TestApplication.h"
#include "ServerRequestStub.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "CppUnit/TestCase.h"
#include "CppUnit/Test.h"
#include "Poco/Net/HTTPBasicCredentials.h"
#include "Poco/Net/HTTPResponse.h"
#include "Poco/Environment.h"
#include <string>


using Poco::Net::HTTPResponse;
using namespace std::string_literals;


namespace
{
	const std::string CLIENT_ADDRESS("127.0.0.1:40000"s);
}


WebInfoTest::WebInfoTest(const std::string& name): CppUnit::TestCase(name)
{
}


WebInfoTest::~WebInfoTest() = default;


void WebInfoTest::testServerInformationRequiresLogin()
{
	TestApplication::Ptr pApp = new TestApplication({"poco.net"s, "osp.web"s, "com.appinf.osp.samples.auth"s, "com.appinf.osp.samples.webinfo"s});
	pApp->config().setString("adminPassword"s, "admin-secret"s);
	pApp->start();

	ServerRequestStub anonymous(CLIENT_ADDRESS, false);
	anonymous.setURI("/info"s);
	pApp->dispatch(anonymous);
	assertTrue (anonymous.response().getStatus() == HTTPResponse::HTTP_UNAUTHORIZED);
	assertTrue (anonymous.response().body().find(Poco::Environment::nodeName()) == std::string::npos);

	ServerRequestStub admin(CLIENT_ADDRESS, false);
	admin.setURI("/info"s);
	Poco::Net::HTTPBasicCredentials("admin"s, "admin-secret"s).authenticate(admin);
	pApp->dispatch(admin);
	assertTrue (admin.response().getStatus() == HTTPResponse::HTTP_OK);
	assertTrue (admin.response().body().find(Poco::Environment::nodeName()) != std::string::npos);
}


CppUnit::Test* WebInfoTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("WebInfoTest");

	CppUnit_addTest(pSuite, WebInfoTest, testServerInformationRequiresLogin);

	return pSuite;
}
