//
// TwoFactorBundleActivatorTest.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "TwoFactorBundleActivatorTest.h"
#include "MemoryAuthAdminService.h"
#include "TemporaryBundle.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "CppUnit/TestCase.h"
#include "CppUnit/Test.h"
#include "Poco/OSP/Auth/TwoFactor/TwoFactorService.h"
#include "Poco/OSP/Auth/TwoFactor/TwoFactorServiceImpl.h"
#include "Poco/OSP/Auth/AuthAdminService.h"
#include "Poco/OSP/BundleActivator.h"
#include "Poco/OSP/PreferencesService.h"
#include "Poco/OSP/Properties.h"
#include "Poco/OSP/ServiceRef.h"
#include "Poco/OSP/ServiceRegistry.h"
#include "Poco/Util/MapConfiguration.h"
#include "Poco/AutoPtr.h"
#include "Poco/ClassLoader.h"
#include "Poco/Exception.h"
#include "Poco/Logger.h"
#include "Poco/Path.h"
#include "Poco/TemporaryFile.h"
#include <memory>
#include <string>


using Poco::OSP::Auth::AuthAdminService;
using Poco::OSP::Auth::TwoFactor::TwoFactorService;
using Poco::OSP::Auth::TwoFactor::TwoFactorServiceImpl;
using Poco::OSP::PreferencesService;
using namespace std::string_literals;


struct TwoFactorBundleActivatorTest::Fixture
	/// The bundle activator of the bundle library, with a bundle context in
	/// whose registry the preferences service is registered, configured with
	/// a passphrase and an issuer.
{
	Fixture():
		bundles(""s),
		pSettings(new Poco::Util::MapConfiguration),
		pPreferences(new PreferencesService(Poco::Path(persistency.path()), pSettings))
	{
		bundles.registry().registerService(PreferencesService::SERVICE_NAME, pPreferences, Poco::OSP::Properties());
		pSettings->setString("auth.twofactor.passphrase"s, "test passphrase"s);
		pSettings->setString("auth.twofactor.issuer"s, "Example"s);
		loader.loadLibrary(TWOFACTOR_BUNDLE_LIBRARY);
		pActivator.reset(loader.create("Poco::OSP::Auth::TwoFactor::TwoFactorBundleActivator"s));
	}

	~Fixture() = default;

	Fixture(const Fixture&) = delete;
	Fixture(Fixture&&) = delete;
	Fixture& operator = (const Fixture&) = delete;
	Fixture& operator = (Fixture&&) = delete;

	void start()
	{
		pActivator->start(bundles.context());
		started = true;
	}

	void stop()
	{
		started = false;
		pActivator->stop(bundles.context());
	}

	[[nodiscard]] Poco::OSP::ServiceRef::Ptr registerAuthService(const AuthAdminService::Ptr& pAuthService)
	{
		return bundles.registry().registerService(AuthAdminService::SERVICE_NAME, pAuthService,
			Poco::OSP::Properties());
	}

	[[nodiscard]] TwoFactorService::Ptr twoFactorService()
	{
		const Poco::OSP::ServiceRef::Ptr pRef = bundles.registry().findByName(TwoFactorService::SERVICE_NAME);
		if (pRef == nullptr) throw Poco::NotFoundException(TwoFactorService::SERVICE_NAME);
		return pRef->castedInstance<TwoFactorService>();
	}

	[[nodiscard]] std::string storedKeySalt()
		/// Returns the key salt that the activator has generated and kept in
		/// the preferences of the bundle.
	{
		return pPreferences->preferences(bundles.bundle()->symbolicName())->getString("keySalt"s, ""s);
	}

	TemporaryBundle bundles;
	Poco::TemporaryFile persistency;
	Poco::AutoPtr<Poco::Util::MapConfiguration> pSettings;
	PreferencesService::Ptr pPreferences;
	Poco::ClassLoader<Poco::OSP::BundleActivator> loader;
	// Declared after the loader, so that it is destroyed before the library is unloaded.
	std::unique_ptr<Poco::OSP::BundleActivator> pActivator;
	bool started = false;
};


TwoFactorBundleActivatorTest::TwoFactorBundleActivatorTest(const std::string& name): CppUnit::TestCase(name)
{
}


TwoFactorBundleActivatorTest::~TwoFactorBundleActivatorTest() = default;


void TwoFactorBundleActivatorTest::testServiceUsesAuthServiceRegisteredAtCall()
{
	MemoryAuthAdminService::Ptr pFirst = new MemoryAuthAdminService;
	Poco::OSP::ServiceRef::Ptr pFirstRef = _pFixture->registerAuthService(pFirst);
	_pFixture->start();
	TwoFactorService::Ptr pService = _pFixture->twoFactorService();
	pService->enable2FA("alice"s);
	assertTrue (pService->is2FAEnabled("alice"s));

	_pFixture->bundles.registry().unregisterService(pFirstRef);
	MemoryAuthAdminService::Ptr pSecond = new MemoryAuthAdminService;
	Poco::OSP::ServiceRef::Ptr pSecondRef = _pFixture->registerAuthService(pSecond);
	assertFalse (pService->is2FAEnabled("alice"s));
	pService->enable2FA("bob"s);
	assertTrue (pService->is2FAEnabled("bob"s));

	_pFixture->bundles.registry().unregisterService(pSecondRef);
	pFirstRef = _pFixture->registerAuthService(pFirst);
	assertTrue (pService->is2FAEnabled("alice"s));
	assertFalse (pService->is2FAEnabled("bob"s));
	_pFixture->bundles.registry().unregisterService(pFirstRef);
}


void TwoFactorBundleActivatorTest::testServiceWithoutAuthServiceThrows()
{
	_pFixture->start();
	TwoFactorService::Ptr pService = _pFixture->twoFactorService();

	try
	{
		[[maybe_unused]] const bool enabled = pService->is2FAEnabled("alice"s);
		fail("the service must not be used without an AuthAdminService");
	}
	catch (Poco::NotFoundException&)
	{
	}
}


void TwoFactorBundleActivatorTest::testGeneratedKeySaltIsKeptInPreferences()
{
	MemoryAuthAdminService::Ptr pAuth = new MemoryAuthAdminService;
	Poco::OSP::ServiceRef::Ptr pAuthRef = _pFixture->registerAuthService(pAuth);
	_pFixture->start();
	const std::string keySalt = _pFixture->storedKeySalt();
	_pFixture->twoFactorService()->enable2FA("alice"s);
	const std::string secret = _pFixture->twoFactorService()->secret("alice"s);
	_pFixture->stop();

	_pFixture->start();
	assertFalse (keySalt.empty());
	assertEqual (keySalt, _pFixture->storedKeySalt());
	assertEqual (secret, _pFixture->twoFactorService()->secret("alice"s));
	_pFixture->bundles.registry().unregisterService(pAuthRef);
}


void TwoFactorBundleActivatorTest::testConfiguredKeySaltIsUsed()
{
	const std::string configuredSalt = "0123456789abcdef0123456789abcdef"s;
	_pFixture->pSettings->setString("auth.twofactor.keySalt"s, configuredSalt);
	MemoryAuthAdminService::Ptr pAuth = new MemoryAuthAdminService;
	Poco::OSP::ServiceRef::Ptr pAuthRef = _pFixture->registerAuthService(pAuth);
	_pFixture->start();
	_pFixture->twoFactorService()->enable2FA("alice"s);

	TwoFactorServiceImpl::Config config;
	config.passphrase = "test passphrase"s;
	config.issuer = "Example"s;
	config.keySalt = configuredSalt;
	TwoFactorServiceImpl::Ptr pReader = new TwoFactorServiceImpl(pAuth, config, Poco::Logger::root());
	assertEqual (_pFixture->twoFactorService()->secret("alice"s), pReader->secret("alice"s));
	assertTrue (_pFixture->storedKeySalt().empty());
	_pFixture->bundles.registry().unregisterService(pAuthRef);
}


void TwoFactorBundleActivatorTest::setUp()
{
	_pFixture = std::make_unique<Fixture>();
}


void TwoFactorBundleActivatorTest::tearDown()
{
	if (_pFixture->started) _pFixture->stop();
	_pFixture.reset();
}


CppUnit::Test* TwoFactorBundleActivatorTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("TwoFactorBundleActivatorTest");

	CppUnit_addTest(pSuite, TwoFactorBundleActivatorTest, testServiceUsesAuthServiceRegisteredAtCall);
	CppUnit_addTest(pSuite, TwoFactorBundleActivatorTest, testServiceWithoutAuthServiceThrows);
	CppUnit_addTest(pSuite, TwoFactorBundleActivatorTest, testGeneratedKeySaltIsKeptInPreferences);
	CppUnit_addTest(pSuite, TwoFactorBundleActivatorTest, testConfiguredKeySaltIsUsed);

	return pSuite;
}
