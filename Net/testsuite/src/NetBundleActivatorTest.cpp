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
#include "Poco/Exception.h"
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


namespace
{
	struct Fixture
		/// The bundle activator of the bundle library, with a bundle context in
		/// whose registry the preferences service is registered.
	{
		Fixture():
			bundles(""s),
			pSettings(new Poco::Util::MapConfiguration)
		{
			bundles.registry().registerService(PreferencesService::SERVICE_NAME,
				new PreferencesService(Poco::Path(persistency.path()), pSettings), Poco::OSP::Properties());
			loader.loadLibrary(NET_BUNDLE_LIBRARY);
			pActivator.reset(loader.create("NetBundleActivator"s));
		}

		TemporaryBundle bundles;
		Poco::TemporaryFile persistency;
		Poco::AutoPtr<Poco::Util::MapConfiguration> pSettings;
		Poco::ClassLoader<Poco::OSP::BundleActivator> loader;
		// Declared after the loader, so that it is destroyed before the library is unloaded.
		std::unique_ptr<Poco::OSP::BundleActivator> pActivator;
	};
}


NetBundleActivatorTest::NetBundleActivatorTest(const std::string& name): CppUnit::TestCase(name)
{
}


NetBundleActivatorTest::~NetBundleActivatorTest() = default;


void NetBundleActivatorTest::testStopClearsProxyConfiguration()
{
	Fixture fixture;
	fixture.pSettings->setString("http.proxy.host"s, "proxy.example.com"s);
	fixture.pSettings->setString("http.proxy.username"s, "user"s);
	fixture.pSettings->setString("http.proxy.password"s, "secret"s);

	fixture.pActivator->start(fixture.bundles.context());
	assertEqual ("proxy.example.com"s, HTTPClientSession::getGlobalProxyConfig().host);
	assertEqual ("secret"s, HTTPClientSession::getGlobalProxyConfig().password);
	fixture.pActivator->stop(fixture.bundles.context());

	assertTrue (HTTPClientSession::getGlobalProxyConfig().host.empty());
	assertTrue (HTTPClientSession::getGlobalProxyConfig().password.empty());
}


void NetBundleActivatorTest::testFailedStartCanBeRepeated()
{
	Fixture fixture;
	fixture.pSettings->setString("http.proxy.host"s, "proxy.example.com"s);
	fixture.pSettings->setString("http.proxy.port"s, "8080/tcp"s);
	try
	{
		fixture.pActivator->start(fixture.bundles.context());
		fail("the proxy port is not a number");
	}
	catch (Poco::SyntaxException&)
	{
	}

	fixture.pSettings->setString("http.proxy.port"s, "8080"s);
	fixture.pActivator->start(fixture.bundles.context());

	assertEqual (8080, static_cast<int>(HTTPClientSession::getGlobalProxyConfig().port));
	fixture.pActivator->stop(fixture.bundles.context());
}


void NetBundleActivatorTest::testFailedPasswordReadCanBeRepeated()
{
	Fixture fixture;
	fixture.pSettings->setString("http.proxy.host"s, "proxy.example.com"s);
	fixture.pSettings->setString("http.proxy.password"s, "${http.proxy.password}"s);
	try
	{
		fixture.pActivator->start(fixture.bundles.context());
		fail("the proxy password refers to itself");
	}
	catch (Poco::CircularReferenceException&)
	{
	}

	fixture.pSettings->setString("http.proxy.password"s, "secret"s);
	fixture.pActivator->start(fixture.bundles.context());

	assertEqual ("secret"s, HTTPClientSession::getGlobalProxyConfig().password);
	fixture.pActivator->stop(fixture.bundles.context());
}


void NetBundleActivatorTest::testProxyPortOutsideRangeIsRejected()
{
	Fixture fixture;
	fixture.pSettings->setString("http.proxy.host"s, "proxy.example.com"s);
	for (const int port: {0, 70000})
	{
		fixture.pSettings->setInt("http.proxy.port"s, port);
		try
		{
			fixture.pActivator->start(fixture.bundles.context());
			fail("the proxy port is outside 1 to 65535");
		}
		catch (Poco::InvalidArgumentException&)
		{
		}
		assertTrue (HTTPClientSession::getGlobalProxyConfig().host.empty());
	}
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
	CppUnit_addTest(pSuite, NetBundleActivatorTest, testFailedStartCanBeRepeated);
	CppUnit_addTest(pSuite, NetBundleActivatorTest, testFailedPasswordReadCanBeRepeated);
	CppUnit_addTest(pSuite, NetBundleActivatorTest, testProxyPortOutsideRangeIsRejected);

	return pSuite;
}
