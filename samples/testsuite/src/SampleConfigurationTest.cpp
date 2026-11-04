//
// SampleConfigurationTest.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "SampleConfigurationTest.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "CppUnit/TestCase.h"
#include "CppUnit/Test.h"
#include "Poco/Util/PropertyFileConfiguration.h"
#include "Poco/AutoPtr.h"
#include "Poco/Environment.h"
#include "Poco/Path.h"
#include <string>


using namespace std::string_literals;


namespace
{
	[[nodiscard]] std::string sampleConfiguration(const std::string& program)
		/// Returns the path of the configuration file of the given sample program.
	{
		Poco::Path path(Poco::Path::forDirectory(Poco::Environment::get("POCO_BASE"s)));
		path.pushDirectory("samples"s);
		path.pushDirectory(program);
		path.setFileName(program + ".properties"s);
		return path.toString();
	}
}


SampleConfigurationTest::SampleConfigurationTest(const std::string& name): CppUnit::TestCase(name)
{
}


SampleConfigurationTest::~SampleConfigurationTest() = default;


void SampleConfigurationTest::testBundleServerListensOnLoopbackAddress()
{
	checkWebServersListenOnLoopbackAddress("BundleServer"s);
}


void SampleConfigurationTest::testBundleContainerListensOnLoopbackAddress()
{
	checkWebServersListenOnLoopbackAddress("BundleContainer"s);
}


void SampleConfigurationTest::checkWebServersListenOnLoopbackAddress(const std::string& program)
{
	// The web server bundles read the addresses to listen on from these
	// settings. The test does not start a server: the bundles start none
	// with port 0, and another process can take any other port.
	Poco::AutoPtr<Poco::Util::PropertyFileConfiguration> pConfig = new Poco::Util::PropertyFileConfiguration(sampleConfiguration(program));
	assertEqual ("127.0.0.1"s, pConfig->getString("osp.web.server.host"s));
	assertEqual ("127.0.0.1"s, pConfig->getString("osp.web.server.secureHost"s));
}


void SampleConfigurationTest::testBundleServerVerifiesServerCertificates()
{
	Poco::AutoPtr<Poco::Util::PropertyFileConfiguration> pConfig = new Poco::Util::PropertyFileConfiguration(sampleConfiguration("BundleServer"s));
	assertEqual ("relaxed"s, pConfig->getString("openSSL.client.verificationMode"s));
	assertEqual ("RejectCertificateHandler"s, pConfig->getString("openSSL.client.invalidCertificateHandler.name"s));
	assertTrue (pConfig->getBool("openSSL.client.loadDefaultCAFile"s));
	// The key in the sample's any.pem is not encrypted.
	assertFalse (pConfig->has("openSSL.server.privateKeyPassphraseHandler.options.password"s));
}


CppUnit::Test* SampleConfigurationTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("SampleConfigurationTest");

	CppUnit_addTest(pSuite, SampleConfigurationTest, testBundleServerListensOnLoopbackAddress);
	CppUnit_addTest(pSuite, SampleConfigurationTest, testBundleContainerListensOnLoopbackAddress);
	CppUnit_addTest(pSuite, SampleConfigurationTest, testBundleServerVerifiesServerCertificates);

	return pSuite;
}
