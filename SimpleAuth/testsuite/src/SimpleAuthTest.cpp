//
// SimpleAuthTest.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "SimpleAuthTest.h"
#include "GlobalSimpleAuthService.h"
#include "SimpleAuth.h"
#include "CppUnit/Test.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestCase.h"
#include "CppUnit/TestSuite.h"
#include "Poco/OSP/BundleContextFactory.h"
#include "Poco/OSP/BundleFactory.h"
#include "Poco/OSP/BundleLoader.h"
#include "Poco/OSP/CodeCache.h"
#include "Poco/OSP/LanguageTag.h"
#include "Poco/OSP/PreferencesService.h"
#include "Poco/OSP/Properties.h"
#include "Poco/OSP/ServiceRef.h"
#include "Poco/OSP/ServiceRegistry.h"
#include "Poco/OSP/SystemEvents.h"
#include "Poco/Util/MapConfiguration.h"
#include "Poco/AutoPtr.h"
#include "Poco/File.h"
#include "Poco/FileStream.h"
#include "Poco/Path.h"
#include "Poco/TemporaryFile.h"
#include <set>
#include <string>
#include <typeinfo>


using Poco::OSP::SimpleAuth::SimpleAuthBundleActivator;
using Poco::OSP::SimpleAuth::SimpleAuthService;
using namespace std::string_literals;


namespace
{
	// Computed independently of POCO, with Python's hashlib: the version 4
	// format of the Auth Data bundle (PBKDF2-HMAC-SHA256 with 50000 iterations
	// and the salt "0123456789abcdef" over the hexadecimal SHA-1 hash of
	// "user:poco:secret"), and MD5 of "poco" followed by the password.
	const std::string V4_HASH_OF_SECRET(
		"v=4;s=MDEyMzQ1Njc4OWFiY2RlZg==;h=i5YfnN2CC8LKVNCVDdXwU6+qbLYW28Q1Siqz9sqPdpE=;i=50000");
	const std::string MD5_HASH_OF_SECRET("4a20f7a891f585bc9d0fb797cbf60401");
	const std::string SALT("poco");


	Poco::AutoPtr<SimpleAuthService> createService(const std::string& adminPasswordHash,
		const std::string& userPasswordHash)
	{
		return new SimpleAuthService("admin"s, adminPasswordHash, "user"s, userPasswordHash,
			std::set<std::string>{"read"s}, SALT);
	}


	class TestBundle
		/// A bundle without code, loaded from a temporary directory with its
		/// own service registry.
	{
	public:
		TestBundle():
			_codeCache(Poco::Path(_directory.path(), "codeCache/"s).toString()),
			_loader(_codeCache, new Poco::OSP::BundleFactory(Poco::OSP::LanguageTag("en"s, "US"s)),
				new Poco::OSP::BundleContextFactory(_registry, _systemEvents,
					Poco::Path(_directory.path(), "persistency/"s)))
		{
			Poco::Path bundlePath(_directory.path());
			bundlePath.makeDirectory();
			bundlePath.pushDirectory("osp.simpleauth.test_1.0.0"s);
			Poco::File(Poco::Path(bundlePath, "META-INF/"s)).createDirectories();
			{
				Poco::FileOutputStream ostr(Poco::Path(bundlePath, "META-INF/manifest.mf"s).toString());
				ostr << "Manifest-Version: 1.0\n"
					"Bundle-Name: SimpleAuth Test Bundle\n"
					"Bundle-SymbolicName: osp.simpleauth.test\n"
					"Bundle-Version: 1.0.0\n";
			}
			_pContext = _loader.contextForBundle(_loader.loadBundle(bundlePath.toString()));
		}

		TestBundle(const TestBundle&) = delete;
		TestBundle(TestBundle&&) = delete;
		TestBundle& operator = (const TestBundle&) = delete;
		TestBundle& operator = (TestBundle&&) = delete;
		~TestBundle() = default;

		[[nodiscard]] Poco::OSP::BundleContext::Ptr context() const
		{
			return _pContext;
		}

		[[nodiscard]] Poco::OSP::ServiceRegistry& registry()
		{
			return _registry;
		}

	private:
		Poco::TemporaryFile _directory;
		Poco::OSP::ServiceRegistry _registry;
		Poco::OSP::SystemEvents _systemEvents;
		Poco::OSP::CodeCache _codeCache;
		Poco::OSP::BundleLoader _loader;
		Poco::OSP::BundleContext::Ptr _pContext;
	};
}


SimpleAuthTest::SimpleAuthTest(const std::string& name): CppUnit::TestCase(name)
{
}


SimpleAuthTest::~SimpleAuthTest() = default;


void SimpleAuthTest::testPBKDF2HashIsVerified()
{
	Poco::AutoPtr<SimpleAuthService> pService = createService(MD5_HASH_OF_SECRET, V4_HASH_OF_SECRET);

	assertTrue (pService->authenticate("user"s, "secret"s));
	assertFalse (pService->authenticate("user"s, "wrong"s));
	assertFalse (pService->authenticate("user"s, ""s));
}


void SimpleAuthTest::testMD5HashIsVerified()
{
	Poco::AutoPtr<SimpleAuthService> pService = createService(MD5_HASH_OF_SECRET, V4_HASH_OF_SECRET);

	assertTrue (pService->authenticate("admin"s, "secret"s));
	assertFalse (pService->authenticate("admin"s, "wrong"s));
	assertFalse (pService->authenticate("nobody"s, "secret"s));
}


void SimpleAuthTest::testScopedAuthorizationIsDenied()
{
	Poco::AutoPtr<SimpleAuthService> pService = createService(MD5_HASH_OF_SECRET, V4_HASH_OF_SECRET);

	assertTrue (pService->authorize("user"s, "read"s));
	assertTrue (pService->authorize("user"s, ""s, "read"s));
	assertFalse (pService->authorize("user"s, "scope"s, "read"s));
	assertTrue (pService->authorize("admin"s, "write"s));
	assertFalse (pService->authorize("admin"s, "scope"s, "write"s));
}


void SimpleAuthTest::testServiceIsNotTakenForClassOfSameName()
{
	Poco::AutoPtr<SimpleAuthService> pService = createService(MD5_HASH_OF_SECRET, V4_HASH_OF_SECRET);

	assertTrue (pService->isA(typeid(SimpleAuthService)));
	assertFalse (pService->isA(globalSimpleAuthServiceType()));
}


void SimpleAuthTest::testMissingPreferencesServiceIsHandled()
{
	TestBundle bundle;

	SimpleAuthBundleActivator activator;
	activator.start(bundle.context());
	assertTrue (bundle.registry().findByName("osp.auth"s) == nullptr);
	activator.stop(bundle.context());
}


void SimpleAuthTest::testRestartedActivatorRegistersAsConfigured()
{
	TestBundle bundle;
	Poco::TemporaryFile persistency;
	Poco::AutoPtr<Poco::Util::MapConfiguration> pSettings = new Poco::Util::MapConfiguration;
	pSettings->setString("auth.simple.user.passwordHash"s, MD5_HASH_OF_SECRET);
	pSettings->setString("auth.simple.salt"s, SALT);
	pSettings->setString("auth.simple.user.permissions"s, "read, write"s);
	bundle.registry().registerService(Poco::OSP::PreferencesService::SERVICE_NAME,
		new Poco::OSP::PreferencesService(Poco::Path(persistency.path()), pSettings), Poco::OSP::Properties());

	SimpleAuthBundleActivator activator;
	activator.start(bundle.context());
	const Poco::OSP::ServiceRef::Ptr pRef = bundle.registry().findByName("osp.auth"s);
	assertTrue (pRef != nullptr);
	const Poco::AutoPtr<SimpleAuthService> pService = pRef->castedInstance<SimpleAuthService>();
	assertTrue (pService->authenticate("user"s, "secret"s));
	assertTrue (pService->authorize("user"s, "write"s));
	activator.stop(bundle.context());
	assertTrue (bundle.registry().findByName("osp.auth"s) == nullptr);

	pSettings->setBool("auth.simple.enable"s, false);
	activator.start(bundle.context());
	assertTrue (bundle.registry().findByName("osp.auth"s) == nullptr);
	activator.stop(bundle.context());
}


void SimpleAuthTest::setUp()
{
}


void SimpleAuthTest::tearDown()
{
}


CppUnit::Test* SimpleAuthTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("SimpleAuthTest");

	CppUnit_addTest(pSuite, SimpleAuthTest, testPBKDF2HashIsVerified);
	CppUnit_addTest(pSuite, SimpleAuthTest, testMD5HashIsVerified);
	CppUnit_addTest(pSuite, SimpleAuthTest, testScopedAuthorizationIsDenied);
	CppUnit_addTest(pSuite, SimpleAuthTest, testServiceIsNotTakenForClassOfSameName);
	CppUnit_addTest(pSuite, SimpleAuthTest, testMissingPreferencesServiceIsHandled);
	CppUnit_addTest(pSuite, SimpleAuthTest, testRestartedActivatorRegistersAsConfigured);

	return pSuite;
}
