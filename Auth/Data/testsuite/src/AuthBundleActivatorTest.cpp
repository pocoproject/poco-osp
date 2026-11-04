//
// AuthBundleActivatorTest.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "AuthBundleActivatorTest.h"
#include "LogCapture.h"
#include "TemporaryBundle.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "CppUnit/TestCase.h"
#include "CppUnit/Test.h"
#include "Poco/OSP/Auth/AuthAdminService.h"
#include "Poco/OSP/BundleActivator.h"
#include "Poco/OSP/PreferencesService.h"
#include "Poco/OSP/Properties.h"
#include "Poco/Util/MapConfiguration.h"
#include "Poco/AutoPtr.h"
#include "Poco/ClassLoader.h"
#include "Poco/Environment.h"
#include "Poco/FileStream.h"
#include "Poco/Message.h"
#include "Poco/Path.h"
#include "Poco/StreamCopier.h"
#include "Poco/TemporaryFile.h"
#include <algorithm>
#include <memory>
#include <string>


using Poco::OSP::Auth::AuthAdminService;
using Poco::OSP::PreferencesService;
using namespace std::string_literals;


namespace
{
	[[nodiscard]] std::string shippedBundleProperties()
		/// Returns the content of the bundle.properties file of the bundle.
	{
		Poco::FileInputStream istr(Poco::Environment::get("POCO_BASE"s) + "/Auth/Data/bundle/bundle.properties"s);
		std::string properties;
		Poco::StreamCopier::copyToString(istr, properties);
		return properties;
	}
}


AuthBundleActivatorTest::AuthBundleActivatorTest(const std::string& name): CppUnit::TestCase(name)
{
}


AuthBundleActivatorTest::~AuthBundleActivatorTest() = default;


void AuthBundleActivatorTest::testUnreachableDatabaseIsNotRetriedAfterStartupTimeout()
{
	TemporaryBundle bundles(shippedBundleProperties());
	Poco::TemporaryFile persistency;
	Poco::AutoPtr<Poco::Util::MapConfiguration> pSettings = new Poco::Util::MapConfiguration;
	// No connector of this name is registered, so each check fails at once.
	pSettings->setString("auth.db.connector"s, "Unregistered"s);
	pSettings->setString("auth.db.connectionString"s, "auth.db"s);
	pSettings->setInt("auth.db.databaseStartupTimeout"s, 0);
	bundles.registry().registerService(PreferencesService::SERVICE_NAME,
		new PreferencesService(Poco::Path(persistency.path()), pSettings), Poco::OSP::Properties());
	Poco::ClassLoader<Poco::OSP::BundleActivator> loader;
	loader.loadLibrary(DATA_BUNDLE_LIBRARY);
	const std::unique_ptr<Poco::OSP::BundleActivator> pActivator(
		loader.create("Poco::OSP::Auth::Data::AuthBundleActivator"s));
	LogCapture capture(bundles.context()->logger().name());

	pActivator->start(bundles.context());
	pActivator->stop(bundles.context());

	const auto messages = capture.channel().waitForMessages(0);
	const auto retries = std::count_if(messages.begin(), messages.end(),
		[](const Poco::Message& msg) { return msg.getPriority() == Poco::Message::PRIO_WARNING; });
	assertEqual (0, static_cast<int>(retries));
	assertTrue (bundles.registry().findByName(AuthAdminService::SERVICE_NAME) == nullptr);
}


void AuthBundleActivatorTest::setUp()
{
}


void AuthBundleActivatorTest::tearDown()
{
}


CppUnit::Test* AuthBundleActivatorTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("AuthBundleActivatorTest");

	CppUnit_addTest(pSuite, AuthBundleActivatorTest, testUnreachableDatabaseIsNotRetriedAfterStartupTimeout);

	return pSuite;
}
