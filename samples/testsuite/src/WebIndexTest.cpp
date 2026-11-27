//
// WebIndexTest.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "WebIndexTest.h"
#include "TestApplication.h"
#include "ServerRequestStub.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "CppUnit/TestCase.h"
#include "CppUnit/Test.h"
#include "Poco/Net/HTTPResponse.h"
#include <string>


using Poco::Net::HTTPResponse;
using namespace std::string_literals;


namespace
{
	const std::string CLIENT_ADDRESS("127.0.0.1:40000"s);
}


WebIndexTest::WebIndexTest(const std::string& name): CppUnit::TestCase(name)
{
}


WebIndexTest::~WebIndexTest() = default;


void WebIndexTest::testIndexIsServedForRequestWithQuery()
{
	TestApplication::Ptr pApp = new TestApplication({"poco.net"s, "osp.web"s, "com.appinf.osp.samples.webindex"s});
	pApp->start();

	for (const auto& uri: {"/?x=1"s, "/index.html?x=1"s})
	{
		ServerRequestStub request(CLIENT_ADDRESS, false);
		request.setURI(uri);
		pApp->dispatch(request);
		assertTrue (request.response().getStatus() == HTTPResponse::HTTP_OK);
		assertTrue (request.response().body().find("Web Server Index"s) != std::string::npos);
	}
}


CppUnit::Test* WebIndexTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("WebIndexTest");

	CppUnit_addTest(pSuite, WebIndexTest, testIndexIsServedForRequestWithQuery);

	return pSuite;
}
