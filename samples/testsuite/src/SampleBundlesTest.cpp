//
// SampleBundlesTest.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "SampleBundlesTest.h"
#include "TestApplication.h"
#include "ServerRequestStub.h"
#include "LogCapture.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "CppUnit/TestCase.h"
#include "CppUnit/Test.h"
#include "Poco/Net/HTTPResponse.h"
#include "Poco/Zip/ZipArchive.h"
#include "Poco/FileStream.h"
#include "Poco/Message.h"
#include <algorithm>
#include <string>
#include <vector>


using namespace std::string_literals;


namespace
{
	const std::string LIBRARY_DIRECTORY("bin/"s);
	const std::string CLIENT_ADDRESS("127.0.0.1:40000"s);
	const std::string EXTENSIBLE_BUNDLE("com.appinf.osp.samples.extensible"s);
	const std::string EXTENSION_BUNDLE("com.appinf.osp.samples.extension"s);
	const std::string EXTENSIBLE_LOGGER("osp.bundle."s + EXTENSIBLE_BUNDLE);
}


SampleBundlesTest::SampleBundlesTest(const std::string& name): CppUnit::TestCase(name)
{
}


SampleBundlesTest::~SampleBundlesTest() = default;


void SampleBundlesTest::testDataOnlyBundlesContainNoLibrary()
{
	for (const auto& bundle: {"com.appinf.osp.samples.webpage"s, "com.appinf.osp.samples.extension"s})
	{
		Poco::FileInputStream istr(TestApplication::bundleFile(bundle));
		const Poco::Zip::ZipArchive archive(istr);
		bool hasExtensions = false;
		for (auto it = archive.fileInfoBegin(); it != archive.fileInfoEnd(); ++it)
		{
			assertTrue (it->first.compare(0, LIBRARY_DIRECTORY.size(), LIBRARY_DIRECTORY) != 0);
			hasExtensions = hasExtensions || it->first == "extensions.xml"s;
		}
		assertTrue (hasExtensions);
	}
}


void SampleBundlesTest::testExtensionBundleExtendsExtensibleBundle()
{
	TestApplication::Ptr pApp = new TestApplication({EXTENSIBLE_BUNDLE, EXTENSION_BUNDLE});
	// Starting the application sets the level of every logger.
	pApp->config().setString("logging.loggers.extensible.name"s, EXTENSIBLE_LOGGER);
	pApp->config().setString("logging.loggers.extensible.level"s, "information"s);
	LogCapture capture(EXTENSIBLE_LOGGER);
	pApp->start();

	const std::vector<Poco::Message> messages = capture.channel().waitForMessages(1);
	assertTrue (std::any_of(messages.begin(), messages.end(), [](const Poco::Message& message)
	{
		return message.getText() == "Handling extension; bundle: "s + EXTENSION_BUNDLE + "; message: Hello, world!"s;
	}));
}


void SampleBundlesTest::testWebPageBundleServesPage()
{
	TestApplication::Ptr pApp = new TestApplication({"poco.net"s, "osp.web"s, "com.appinf.osp.samples.webpage"s});
	pApp->start();

	ServerRequestStub request(CLIENT_ADDRESS, false);
	request.setURI("/sample/index.html"s);
	pApp->dispatch(request);

	assertTrue (request.response().getStatus() == Poco::Net::HTTPResponse::HTTP_OK);
	assertTrue (request.response().body().find("does not contain any code"s) != std::string::npos);
}


CppUnit::Test* SampleBundlesTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("SampleBundlesTest");

	CppUnit_addTest(pSuite, SampleBundlesTest, testDataOnlyBundlesContainNoLibrary);
	CppUnit_addTest(pSuite, SampleBundlesTest, testExtensionBundleExtendsExtensibleBundle);
	CppUnit_addTest(pSuite, SampleBundlesTest, testWebPageBundleServesPage);

	return pSuite;
}
