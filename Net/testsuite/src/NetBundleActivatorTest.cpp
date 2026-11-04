//
// NetBundleActivatorTest.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "NetBundleActivatorTest.h"
#include "TemporaryBundle.h"
#include "Poco/OSP/BundleActivator.h"
#include "Poco/OSP/PreferencesService.h"
#include "Poco/OSP/Properties.h"
#include "Poco/Net/HTTPClientSession.h"
#include "Poco/Util/MapConfiguration.h"
#include "Poco/AutoPtr.h"
#include "Poco/ClassLoader.h"
#include "Poco/Path.h"
#include "Poco/TemporaryFile.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "CppUnit/TestCase.h"
#include "CppUnit/Test.h"
#include <memory>
#include <string>


using Poco::OSP::PreferencesService;
using Poco::Net::HTTPClientSession;
using namespace std::string_literals;


NetBundleActivatorTest::NetBundleActivatorTest(const std::string& name): CppUnit::TestCase(name)
{
}


NetBundleActivatorTest::~NetBundleActivatorTest() = default;


void NetBundleActivatorTest::testStopClearsProxyConfiguration()
{
	TemporaryBundle bundles(""s);
	Poco::TemporaryFile persistency;
	Poco::AutoPtr<Poco::Util::MapConfiguration> pSettings = new Poco::Util::MapConfiguration;
	pSettings->setString("http.proxy.host"s, "proxy.example.com"s);
	pSettings->setString("http.proxy.username"s, "user"s);
	pSettings->setString("http.proxy.password"s, "secret"s);
	bundles.registry().registerService(PreferencesService::SERVICE_NAME, new PreferencesService(Poco::Path(persistency.path()), pSettings), Poco::OSP::Properties());
	Poco::ClassLoader<Poco::OSP::BundleActivator> loader;
	loader.loadLibrary(NET_BUNDLE_LIBRARY);
	std::unique_ptr<Poco::OSP::BundleActivator> pActivator(loader.create("NetBundleActivator"s));

	pActivator->start(bundles.context());
	assertEqual ("proxy.example.com"s, HTTPClientSession::getGlobalProxyConfig().host);
	assertEqual ("secret"s, HTTPClientSession::getGlobalProxyConfig().password);
	pActivator->stop(bundles.context());

	assertTrue (HTTPClientSession::getGlobalProxyConfig().host.empty());
	assertTrue (HTTPClientSession::getGlobalProxyConfig().password.empty());
}


void NetBundleActivatorTest::setUp()
{
}


void NetBundleActivatorTest::tearDown()
{
	HTTPClientSession::setGlobalProxyConfig(Poco::Net::ProxyConfig());
}


CppUnit::Test* NetBundleActivatorTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("NetBundleActivatorTest");

	CppUnit_addTest(pSuite, NetBundleActivatorTest, testStopClearsProxyConfiguration);

	return pSuite;
}
