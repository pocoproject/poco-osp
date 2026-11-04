//
// SimpleAuthTest.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "SimpleAuthTest.h"
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
#include "Poco/OSP/ServiceRegistry.h"
#include "Poco/OSP/SystemEvents.h"
#include "Poco/AutoPtr.h"
#include "Poco/File.h"
#include "Poco/FileStream.h"
#include "Poco/Path.h"
#include "Poco/TemporaryFile.h"
#include <set>
#include <string>


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


void SimpleAuthTest::testMissingPreferencesServiceIsHandled()
{
	Poco::TemporaryFile directory;
	Poco::Path bundlePath(directory.path());
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

	Poco::OSP::ServiceRegistry registry;
	Poco::OSP::SystemEvents systemEvents;
	Poco::OSP::CodeCache codeCache(Poco::Path(directory.path(), "codeCache/"s).toString());
	Poco::OSP::BundleContextFactory::Ptr pContextFactory = new Poco::OSP::BundleContextFactory(registry, systemEvents,
		Poco::Path(directory.path(), "persistency/"s));
	Poco::OSP::BundleLoader loader(codeCache, new Poco::OSP::BundleFactory(Poco::OSP::LanguageTag("en"s, "US"s)),
		pContextFactory);
	Poco::OSP::Bundle::Ptr pBundle = loader.loadBundle(bundlePath.toString());
	Poco::OSP::BundleContext::Ptr pContext = loader.contextForBundle(pBundle);

	SimpleAuthBundleActivator activator;
	activator.start(pContext);
	assertTrue (registry.findByName("osp.auth"s) == nullptr);
	activator.stop(pContext);
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
	CppUnit_addTest(pSuite, SimpleAuthTest, testMissingPreferencesServiceIsHandled);

	return pSuite;
}
