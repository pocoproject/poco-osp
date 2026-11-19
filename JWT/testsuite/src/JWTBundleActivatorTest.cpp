//
// JWTBundleActivatorTest.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "JWTBundleActivatorTest.h"
#include "JWTValidator.h"
#include "TemporaryBundle.h"
#include "Poco/OSP/BundleActivator.h"
#include "Poco/OSP/PreferencesService.h"
#include "Poco/OSP/Properties.h"
#include "Poco/OSP/ServiceRef.h"
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


using Poco::OSP::JWT::JWTValidator;
using Poco::OSP::PreferencesService;
using namespace std::string_literals;


JWTBundleActivatorTest::JWTBundleActivatorTest(const std::string& name): CppUnit::TestCase(name)
{
}


JWTBundleActivatorTest::~JWTBundleActivatorTest() = default;


void JWTBundleActivatorTest::testStopRemovesOnlyItsOwnService()
{
	TemporaryBundle bundles(""s);
	Poco::TemporaryFile persistency;
	Poco::AutoPtr<Poco::Util::MapConfiguration> pSettings = new Poco::Util::MapConfiguration;
	pSettings->setString("jwt.validator.hmac.key"s, "0123456789abcdef0123456789abcdef"s);
	bundles.registry().registerService(PreferencesService::SERVICE_NAME, new PreferencesService(Poco::Path(persistency.path()), pSettings), Poco::OSP::Properties());
	Poco::ClassLoader<Poco::OSP::BundleActivator> loader;
	loader.loadLibrary(JWT_BUNDLE_LIBRARY);
	std::unique_ptr<Poco::OSP::BundleActivator> pActivator(loader.create("Poco::OSP::JWT::BundleActivator"s));
	pActivator->start(bundles.context());
	pActivator->stop(bundles.context());
	JWTValidator::Config config;
	config.algorithms = {"HS256"s};
	config.hmacKey = "0123456789abcdef0123456789abcdef"s;
	bundles.registry().registerService(JWTValidator::SERVICE_NAME, new JWTValidator(config), Poco::OSP::Properties());

	pActivator->stop(bundles.context());

	assertTrue (bundles.registry().findByName(JWTValidator::SERVICE_NAME) != nullptr);
}


void JWTBundleActivatorTest::setUp()
{
}


void JWTBundleActivatorTest::tearDown()
{
}


CppUnit::Test* JWTBundleActivatorTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("JWTBundleActivatorTest");

	CppUnit_addTest(pSuite, JWTBundleActivatorTest, testStopRemovesOnlyItsOwnService);

	return pSuite;
}
