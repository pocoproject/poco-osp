//
// WebEchoTest.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "WebEchoTest.h"
#include "TestApplication.h"
#include "ServerRequestStub.h"
#include "LogCapture.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "CppUnit/TestCase.h"
#include "CppUnit/Test.h"
#include "Poco/OSP/Bundle.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/BundleLoader.h"
#include "Poco/Net/HTTPMessage.h"
#include "Poco/Net/HTTPResponse.h"
#include "Poco/Message.h"
#include <string>
#include <vector>


using namespace std::string_literals;


namespace
{
	const std::string CLIENT_ADDRESS("127.0.0.1:40000"s);
}


WebEchoTest::WebEchoTest(const std::string& name): CppUnit::TestCase(name)
{
}


WebEchoTest::~WebEchoTest() = default;


void WebEchoTest::testRequestValuesAreEscaped()
{
	TestApplication::Ptr pApp = new TestApplication({"poco.net"s, "osp.web"s, "com.appinf.osp.samples.webecho"s});
	pApp->start();

	ServerRequestStub request(CLIENT_ADDRESS, false);
	request.setMethod("<method>"s);
	request.setURI("/echo/%3Cs%3E?%3Cscript%3E=1"s);
	request.setVersion("<version>"s);
	request.setHost("<host>"s);
	request.setContentType("text/<i>x</i>"s);
	request.setTransferEncoding("<encoding>"s);
	pApp->dispatch(request);

	assertTrue (request.response().getStatus() == Poco::Net::HTTPResponse::HTTP_OK);
	const std::string body = request.response().body();
	for (const auto& value: {"&lt;method&gt;"s, "/echo/&lt;s&gt;"s, "&lt;script&gt;"s, "&lt;version&gt;"s, "&lt;host&gt;"s, "text/&lt;i&gt;x&lt;/i&gt;"s, "&lt;encoding&gt;"s})
	{
		assertTrue (body.find(value) != std::string::npos);
	}
	for (const auto& markup: {"<method>"s, "<s>"s, "<script>"s, "<version>"s, "<host>"s, "<i>"s, "<encoding>"s})
	{
		assertTrue (body.find(markup) == std::string::npos);
	}
}


void WebEchoTest::testMalformedEscapeIsBadRequest()
{
	TestApplication::Ptr pApp = new TestApplication({"poco.net"s, "osp.web"s, "com.appinf.osp.samples.webecho"s});
	pApp->start();
	Poco::OSP::Bundle::Ptr pWebBundle = pApp->bundleLoader().findBundle("osp.web"s);
	LogCapture capture(pApp->bundleLoader().contextForBundle(pWebBundle)->logger().name());

	ServerRequestStub request(CLIENT_ADDRESS, false);
	request.setURI("/echo?a=%zz"s);
	request.setHost("127.0.0.1"s);
	pApp->dispatch(request);

	assertTrue (request.response().getStatus() == Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
	const std::vector<Poco::Message> messages = capture.channel().waitForMessages(0);
	for (const auto& message: messages)
	{
		assertTrue (message.getPriority() > Poco::Message::PRIO_ERROR);
	}
}


void WebEchoTest::testRequestWithoutHostIsAnswered()
{
	TestApplication::Ptr pApp = new TestApplication({"poco.net"s, "osp.web"s, "com.appinf.osp.samples.webecho"s});
	pApp->start();

	ServerRequestStub request(CLIENT_ADDRESS, false);
	request.setVersion(Poco::Net::HTTPMessage::HTTP_1_0);
	request.setURI("/echo"s);
	pApp->dispatch(request);

	assertTrue (request.response().getStatus() == Poco::Net::HTTPResponse::HTTP_OK);
	assertTrue (request.response().body().find("<li><b>Host:</b> </li>"s) != std::string::npos);
}


CppUnit::Test* WebEchoTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("WebEchoTest");

	CppUnit_addTest(pSuite, WebEchoTest, testRequestValuesAreEscaped);
	CppUnit_addTest(pSuite, WebEchoTest, testMalformedEscapeIsBadRequest);
	CppUnit_addTest(pSuite, WebEchoTest, testRequestWithoutHostIsAnswered);

	return pSuite;
}
