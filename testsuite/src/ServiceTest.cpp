//
// ServiceTest.cpp
//
// Copyright (c) 2021-2025, Aleph ONE Software Engineering LLC.
// All rights reserved.
//
// License: Aleph ONE Software and Source Code License Agreement
//


#include "ServiceTest.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "Poco/OSP/ServiceImpl.h"
#include "Poco/OSP/Bundle.h"
#include "Poco/OSP/BundleFactory.h"
#include "Poco/OSP/BundleContextFactory.h"
#include "Poco/OSP/BundleLoader.h"
#include "Poco/OSP/CodeCache.h"
#include "Poco/OSP/ServiceRegistry.h"
#include "Poco/OSP/LanguageTag.h"
#include "Poco/OSP/SystemEvents.h"
#include "Poco/OSP/PreferencesService.h"
#include "Poco/OSP/ServiceFinder.h"
#include "Poco/OSP/Properties.h"
#include "Poco/Util/MapConfiguration.h"
#include "Poco/TemporaryFile.h"
#include "Poco/Exception.h"
#include "OSPBundleTestSuite.h"


using Poco::OSP::Service;
using Poco::OSP::ServiceImpl;
using Poco::OSP::Bundle;
using Poco::OSP::BundleFactory;
using Poco::OSP::BundleContext;
using Poco::OSP::BundleContextFactory;
using Poco::OSP::BundleLoader;
using Poco::OSP::CodeCache;
using Poco::OSP::ServiceRegistry;
using Poco::OSP::LanguageTag;
using Poco::OSP::PreferencesService;
using Poco::OSP::ServiceFinder;
using Poco::AutoPtr;
using Poco::Util::MapConfiguration;
using Poco::Util::AbstractConfiguration;


namespace
{
	class TestService: public ServiceImpl<TestService>
	{
	public:
		using Ptr = AutoPtr<TestService>;

		static inline const std::string DEFAULT_NAME = "test.service";

		TestService(
			const std::string& name = DEFAULT_NAME,
			BundleContext::Ptr pContext = nullptr,
			ConfigPtr pConf = nullptr
		):
			ServiceImpl(name, pContext, pConf)
		{
		}

		// Expose protected methods for testing
		using ServiceImpl::config;
		using ServiceImpl::setConfig;
		using ServiceImpl::context;
		using ServiceImpl::logger;
	};

	// Service that uses SERVICE_NAME as default (for unique name testing)
	class UniqueNameService: public ServiceImpl<UniqueNameService>
	{
	public:
		using Ptr = AutoPtr<UniqueNameService>;

		UniqueNameService(const std::string& name = SERVICE_NAME):
			ServiceImpl(name)
		{
		}
	};
}


ServiceTest::ServiceTest(const std::string& name): CppUnit::TestCase(name)
{
}


ServiceTest::~ServiceTest()
{
}


void ServiceTest::testServiceType()
{
	TestService::Ptr pService = new TestService;

	assert(pService->type() == typeid(TestService));
	assert(pService->isA(typeid(TestService)));
	assert(pService->isA(typeid(Service)));
	assert(pService->name() == "test.service");

	// Test custom name
	TestService::Ptr pService2 = new TestService("custom.service");
	assert(pService2->name() == "custom.service");
}


void ServiceTest::testServiceConfig()
{
	// Create initial config
	AutoPtr<MapConfiguration> pInitConf = new MapConfiguration;
	pInitConf->setString("key1", "value1");
	pInitConf->setString("key2", "value2");
	pInitConf->setString("nested.key3", "value3");

	TestService::Ptr pService = new TestService(TestService::DEFAULT_NAME, nullptr, pInitConf.cast<AbstractConfiguration>());

	auto pConf = pService->config();
	assert(pConf->getString("key1") == "value1");
	assert(pConf->getString("key2") == "value2");
	assert(pConf->getString("nested.key3") == "value3");
}


void ServiceTest::testServiceSetConfig()
{
	TestService::Ptr pService = new TestService;

	// Add configuration
	AutoPtr<MapConfiguration> pConf1 = new MapConfiguration;
	pConf1->setString("key1", "value1");
	pConf1->setString("key2", "value2");

	pService->setConfig(pConf1);

	auto pConf = pService->config();
	assert(pConf->getString("key1") == "value1");
	assert(pConf->getString("key2") == "value2");

	// Add more configuration (should merge)
	AutoPtr<MapConfiguration> pConf2 = new MapConfiguration;
	pConf2->setString("key3", "value3");
	pConf2->setString("key1", "newvalue1"); // Overwrite

	pService->setConfig(pConf2);

	pConf = pService->config();
	assert(pConf->getString("key1") == "newvalue1");
	assert(pConf->getString("key2") == "value2");
	assert(pConf->getString("key3") == "value3");
}


void ServiceTest::testServiceSetConfigWithRemove()
{
	TestService::Ptr pService = new TestService;

	// Add initial configuration
	AutoPtr<MapConfiguration> pConf1 = new MapConfiguration;
	pConf1->setString("key1", "value1");
	pConf1->setString("key2", "value2");
	pConf1->setString("key3", "value3");
	pConf1->setString("nested.a", "valueA");
	pConf1->setString("nested.b", "valueB");

	pService->setConfig(pConf1);

	auto pConf = pService->config();
	assert(pConf->getString("key1") == "value1");
	assert(pConf->getString("key2") == "value2");
	assert(pConf->getString("key3") == "value3");
	assert(pConf->getString("nested.a") == "valueA");
	assert(pConf->getString("nested.b") == "valueB");

	// Remove some entries and add new ones
	AutoPtr<MapConfiguration> pRemove = new MapConfiguration;
	pRemove->setString("key2", ""); // Value doesn't matter, just the key
	pRemove->setString("nested.a", "");

	AutoPtr<MapConfiguration> pAdd = new MapConfiguration;
	pAdd->setString("key4", "value4");
	pAdd->setString("key1", "newvalue1");

	pService->setConfig(pAdd, pRemove);

	pConf = pService->config();
	assert(pConf->getString("key1") == "newvalue1");
	assert(!pConf->hasProperty("key2")); // Removed
	assert(pConf->getString("key3") == "value3");
	assert(pConf->getString("key4") == "value4");
	assert(!pConf->hasProperty("nested.a")); // Removed
	assert(pConf->getString("nested.b") == "valueB");
}


void ServiceTest::testServiceConfigMerge()
{
	// Test that service config overwrites initial values
	AutoPtr<MapConfiguration> pInitConf = new MapConfiguration;
	pInitConf->setString("key1", "initial1");
	pInitConf->setString("key2", "initial2");

	TestService::Ptr pService = new TestService(TestService::DEFAULT_NAME, nullptr, pInitConf.cast<AbstractConfiguration>());

	auto pConf = pService->config();
	assert(pConf->getString("key1") == "initial1");
	assert(pConf->getString("key2") == "initial2");

	// setConfig should overwrite
	AutoPtr<MapConfiguration> pConf2 = new MapConfiguration;
	pConf2->setString("key1", "overwritten1");
	pConf2->setString("key3", "new3");

	pService->setConfig(pConf2);

	pConf = pService->config();
	assert(pConf->getString("key1") == "overwritten1");
	assert(pConf->getString("key2") == "initial2");
	assert(pConf->getString("key3") == "new3");
}


void ServiceTest::testServiceBundleConfig()
{
	// Set up bundle infrastructure
	Poco::TemporaryFile codeCacheDir;
	CodeCache cc(codeCacheDir.path());
	ServiceRegistry reg;
	LanguageTag lang("en", "US");

	BundleFactory::Ptr pBundleFactory(new BundleFactory(lang));
	Poco::OSP::SystemEvents systemEvents;
	BundleContextFactory::Ptr pBundleContextFactory(new BundleContextFactory(reg, systemEvents));
	BundleLoader loader(cc, pBundleFactory, pBundleContextFactory);

	// Load a test bundle (bundle1 has properties in bundle.properties)
	Bundle::Ptr pBundle = loader.createBundle(OSPBundleTestSuite::findBundle("com.appinf.osp.bundle1_1.0.0"));
	loader.loadBundle(pBundle);
	loader.resolveAllBundles();
	loader.startAllBundles();

	// Get the bundle context
	BundleContext::Ptr pContext = loader.contextForBundle(pBundle);
	assert(!pContext.isNull());

	// Create service with bundle context and initial service config
	AutoPtr<MapConfiguration> pInitConf = new MapConfiguration;
	pInitConf->setString("service.key", "serviceValue");
	pInitConf->setString("shared.key", "fromService"); // Should overwrite bundle value

	TestService::Ptr pService = new TestService(TestService::DEFAULT_NAME, pContext, pInitConf.cast<AbstractConfiguration>());

	// Verify context is available
	assert(pService->context() == pContext);

	// Verify bundle properties are in config
	auto pConf = pService->config();
	assert(!pConf.isNull());
	assert(pConf->getString("bundle.key1") == "bundleValue1");
	assert(pConf->getString("bundle.key2") == "bundleValue2");

	// Verify service config is present
	assert(pConf->getString("service.key") == "serviceValue");

	// Verify service config overwrites bundle config (merge order)
	assert(pConf->getString("shared.key") == "fromService");

	// setConfig should also merge correctly
	AutoPtr<MapConfiguration> pNewConf = new MapConfiguration;
	pNewConf->setString("bundle.key1", "overwritten");
	pNewConf->setString("new.key", "newValue");

	pService->setConfig(pNewConf);

	pConf = pService->config();
	assert(pConf->getString("bundle.key1") == "overwritten");
	assert(pConf->getString("bundle.key2") == "bundleValue2"); // Unchanged
	assert(pConf->getString("new.key") == "newValue");

	// Clean up
	loader.stopAllBundles();
	loader.unloadBundle(pBundle);
}


void ServiceTest::testServiceNameAfterPreviousInstance()
{
	// an instance created after the previous one is gone (a bundle that is
	// stopped and started again) is found under the default name
	UniqueNameService::Ptr pService1 = new UniqueNameService;
	assertTrue (pService1->name() == UniqueNameService::SERVICE_NAME);
	pService1 = nullptr;

	UniqueNameService::Ptr pService2 = new UniqueNameService;
	assertTrue (pService2->name() == UniqueNameService::SERVICE_NAME);

	UniqueNameService::Ptr pService3 = new UniqueNameService("custom.unique");
	assertTrue (pService3->name() == "custom.unique");
}


void ServiceTest::testLoggerIsNamedAfterService()
{
	TestService::Ptr pService = new TestService("test.logger.service");
	assertEqual ("test.logger.service", pService->logger().name());
}


void ServiceTest::testServiceConfigIncludesConfigurationOfPreferencesService()
{
	Poco::TemporaryFile codeCacheDir;
	CodeCache cc(codeCacheDir.path());
	ServiceRegistry reg;
	LanguageTag lang("en", "US");

	BundleFactory::Ptr pBundleFactory(new BundleFactory(lang));
	Poco::OSP::SystemEvents systemEvents;
	BundleContextFactory::Ptr pBundleContextFactory(new BundleContextFactory(reg, systemEvents));
	BundleLoader loader(cc, pBundleFactory, pBundleContextFactory);

	AutoPtr<MapConfiguration> pGlobalConf = new MapConfiguration;
	pGlobalConf->setString("global.key", "globalValue");
	pGlobalConf->setString("bundle.key1", "fromGlobal");
	Poco::TemporaryFile prefsDir;
	reg.registerService(PreferencesService::SERVICE_NAME, new PreferencesService(prefsDir.path(), pGlobalConf), Poco::OSP::Properties());

	Bundle::Ptr pBundle = loader.loadBundle(OSPBundleTestSuite::findBundle("com.appinf.osp.bundle1_1.0.0"));
	pBundle->resolve();
	pBundle->start();
	BundleContext::Ptr pContext = loader.contextForBundle(pBundle);

	TestService::Ptr pService = new TestService(TestService::DEFAULT_NAME, pContext);
	assertEqual ("globalValue", pService->config()->getString("global.key"));
	// the bundle's properties come after the global configuration
	assertEqual ("bundleValue1", pService->config()->getString("bundle.key1"));

	loader.unloadAllBundles();
}


void ServiceTest::testServiceFinderFindsServiceOfType()
{
	Poco::TemporaryFile codeCacheDir;
	CodeCache cc(codeCacheDir.path());
	ServiceRegistry reg;
	LanguageTag lang("en", "US");

	BundleFactory::Ptr pBundleFactory(new BundleFactory(lang));
	Poco::OSP::SystemEvents systemEvents;
	BundleContextFactory::Ptr pBundleContextFactory(new BundleContextFactory(reg, systemEvents));
	BundleLoader loader(cc, pBundleFactory, pBundleContextFactory);

	Bundle::Ptr pBundle = loader.loadBundle(OSPBundleTestSuite::findBundle("com.appinf.osp.bundle1_1.0.0"));
	pBundle->resolve();
	pBundle->start();
	BundleContext::Ptr pContext = loader.contextForBundle(pBundle);

	try
	{
		ServiceFinder::find<UniqueNameService>(pContext);
		fail("service not registered - must throw");
	}
	catch (Poco::NotFoundException&)
	{
	}

	UniqueNameService::Ptr pService = new UniqueNameService;
	reg.registerService(UniqueNameService::SERVICE_NAME, pService, Poco::OSP::Properties());
	assertTrue (ServiceFinder::find<UniqueNameService>(pContext) == pService);

	try
	{
		ServiceFinder::findByName<TestService>(pContext, UniqueNameService::SERVICE_NAME);
		fail("service of another type - must throw");
	}
	catch (Poco::BadCastException&)
	{
	}

	loader.unloadAllBundles();
}


void ServiceTest::setUp()
{
}


void ServiceTest::tearDown()
{
}


CppUnit::Test* ServiceTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("ServiceTest");

	CppUnit_addTest(pSuite, ServiceTest, testServiceType);
	CppUnit_addTest(pSuite, ServiceTest, testServiceConfig);
	CppUnit_addTest(pSuite, ServiceTest, testServiceSetConfig);
	CppUnit_addTest(pSuite, ServiceTest, testServiceSetConfigWithRemove);
	CppUnit_addTest(pSuite, ServiceTest, testServiceConfigMerge);
	CppUnit_addTest(pSuite, ServiceTest, testServiceBundleConfig);
	CppUnit_addTest(pSuite, ServiceTest, testServiceNameAfterPreviousInstance);
	CppUnit_addTest(pSuite, ServiceTest, testLoggerIsNamedAfterService);
	CppUnit_addTest(pSuite, ServiceTest, testServiceConfigIncludesConfigurationOfPreferencesService);
	CppUnit_addTest(pSuite, ServiceTest, testServiceFinderFindsServiceOfType);

	return pSuite;
}
