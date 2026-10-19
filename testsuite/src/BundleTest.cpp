//
// BundleTest.cpp
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#include "BundleTest.h"
#include "OSPBundleTestSuite.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "Poco/OSP/Bundle.h"
#include "Poco/OSP/BundleFactory.h"
#include "Poco/OSP/BundleContextFactory.h"
#include "Poco/OSP/BundleLoader.h"
#include "Poco/OSP/CodeCache.h"
#include "Poco/OSP/ServiceRegistry.h"
#include "Poco/OSP/LanguageTag.h"
#include "Poco/OSP/Version.h"
#include "Poco/OSP/OSPException.h"
#include "Poco/OSP/SystemEvents.h"
#include "Poco/Path.h"
#include "Poco/File.h"
#include "Poco/FileStream.h"
#include "Poco/Environment.h"
#include "Poco/Delegate.h"
#include "Poco/Logger.h"
#include "Poco/ConsoleChannel.h"
#include <memory>


using Poco::OSP::Bundle;
using Poco::OSP::BundleFactory;
using Poco::OSP::BundleContextFactory;
using Poco::OSP::BundleLoader;
using Poco::OSP::CodeCache;
using Poco::OSP::ServiceRegistry;
using Poco::OSP::LanguageTag;
using Poco::OSP::Version;
using Poco::OSP::BundleEvent;
using Poco::Path;
using Poco::File;
using Poco::Delegate;
using Poco::Logger;


const std::string BundleTest::TEST_BUNDLES_DIR("generated-bundles");


BundleTest::BundleTest(const std::string& name): CppUnit::TestCase(name)
{
}


BundleTest::~BundleTest()
{
}


void BundleTest::testLoadOne()
{
	CodeCache cc("codeCache");
	ServiceRegistry reg;
	LanguageTag lang("en", "US");

	BundleFactory::Ptr pBundleFactory(new BundleFactory(lang));
	Poco::OSP::SystemEvents systemEvents;
	BundleContextFactory::Ptr pBundleContextFactory(new BundleContextFactory(reg, systemEvents));
	BundleLoader loader(cc, pBundleFactory, pBundleContextFactory);

	Bundle::Ptr pBundle = loader.createBundle(findBundle("com.appinf.osp.bundle1_1.0.0"));

	assert (pBundle->name() == "OSP Sample Bundle 1");
	assert (pBundle->symbolicName() == "com.appinf.osp.bundle1");
	assert (pBundle->version() == Version("1.0.0"));
	assert (pBundle->vendor() == "Applied Informatics");
	assert (pBundle->copyright() == "(c) 2007, Applied Informatics Software Engineering GmbH");

	assert (pBundle->state() == Bundle::BUNDLE_INSTALLED);
	assert (!pBundle->isResolved());
	assert (!pBundle->isStarted());
	assert (!pBundle->isActive());

	assert (pBundle->activatorClass() == "");
	assert (pBundle->activatorLibrary() == "");
	assertNull (pBundle->activator());

	loader.events().bundleLoaded    += Delegate<BundleTest, BundleEvent>(this, &BundleTest::handleEvent);
	loader.events().bundleResolving += Delegate<BundleTest, BundleEvent>(this, &BundleTest::handleEvent);
	loader.events().bundleResolved  += Delegate<BundleTest, BundleEvent>(this, &BundleTest::handleEvent);
	loader.events().bundleStarting  += Delegate<BundleTest, BundleEvent>(this, &BundleTest::handleStartingEvent);
	loader.events().bundleStarted   += Delegate<BundleTest, BundleEvent>(this, &BundleTest::handleStartedEvent);
	loader.events().bundleStopping  += Delegate<BundleTest, BundleEvent>(this, &BundleTest::handleStoppingEvent);
	loader.events().bundleStopped   += Delegate<BundleTest, BundleEvent>(this, &BundleTest::handleStoppedEvent);
	loader.events().bundleUnloaded  += Delegate<BundleTest, BundleEvent>(this, &BundleTest::handleEvent);

	_events.clear();
	loader.loadBundle(pBundle);
	assert (_events.size() == 1);
	assert (_events[0].bundle() == pBundle);
	assert (_events[0].what() == BundleEvent::EV_BUNDLE_LOADED);

	try
	{
		pBundle->start();
		fail("bundle not resolved - must throw");
	}
	catch (Poco::OSP::BundleStateException&)
	{
	}

	try
	{
		pBundle->stop();
		fail("bundle not started - must throw");
	}
	catch (Poco::OSP::BundleStateException&)
	{
	}

	_events.clear();

	pBundle->resolve();

	assert (pBundle->state() == Bundle::BUNDLE_RESOLVED);
	assert (pBundle->isResolved());

	assert (_events.size() == 2);
	assert (_events[0].bundle() == pBundle);
	assert (_events[0].what() == BundleEvent::EV_BUNDLE_RESOLVING);
	assert (_events[1].bundle() == pBundle);
	assert (_events[1].what() == BundleEvent::EV_BUNDLE_RESOLVED);

	try
	{
		pBundle->stop();
		fail("bundle not started - must throw");
	}
	catch (Poco::OSP::BundleStateException&)
	{
	}

	_events.clear();

	pBundle->start();

	assert (pBundle->state() == Bundle::BUNDLE_ACTIVE);
	assert (pBundle->isStarted());
	assert (pBundle->isActive());

	assert (_events.size() == 2);
	assert (_events[0].bundle() == pBundle);
	assert (_events[0].what() == BundleEvent::EV_BUNDLE_STARTING);
	assert (_events[1].bundle() == pBundle);
	assert (_events[1].what() == BundleEvent::EV_BUNDLE_STARTED);

	_events.clear();

	pBundle->stop();

	assert (pBundle->state() == Bundle::BUNDLE_RESOLVED);
	assert (!pBundle->isStarted());
	assert (!pBundle->isActive());
	assert (pBundle->isResolved());

	assert (_events.size() == 2);
	assert (_events[0].bundle() == pBundle);
	assert (_events[0].what() == BundleEvent::EV_BUNDLE_STOPPING);
	assert (_events[1].bundle() == pBundle);
	assert (_events[1].what() == BundleEvent::EV_BUNDLE_STOPPED);

	_events.clear();
	loader.unloadBundle(pBundle);
	assert (_events.size() == 1);
	assert (_events[0].bundle() == pBundle);
	assert (_events[0].what() == BundleEvent::EV_BUNDLE_UNLOADED);
}


void BundleTest::testVersionConflict()
{
	CodeCache cc("codeCache");
	ServiceRegistry reg;
	LanguageTag lang("en", "US");

	BundleFactory::Ptr pBundleFactory(new BundleFactory(lang));
	Poco::OSP::SystemEvents systemEvents;
	BundleContextFactory::Ptr pBundleContextFactory(new BundleContextFactory(reg, systemEvents));
	BundleLoader loader(cc, pBundleFactory, pBundleContextFactory);

	Bundle::Ptr pBundle100 = loader.createBundle(findBundle("com.appinf.osp.bundle1_1.0.0"));
	Bundle::Ptr pBundle091 = loader.createBundle(findBundle("com.appinf.osp.bundle1_0.9.1"));

	loader.loadBundle(pBundle100);
	try
	{
		loader.loadBundle(pBundle091);
		fail("conflicting versions - must throw");
	}
	catch (Poco::OSP::BundleVersionConflictException&)
	{
	}
}


void BundleTest::testProperties()
{
	CodeCache cc("codeCache");
	ServiceRegistry reg;
	LanguageTag lang("en", "US");

	BundleFactory::Ptr pBundleFactory(new BundleFactory(lang));
	Poco::OSP::SystemEvents systemEvents;
	BundleContextFactory::Ptr pBundleContextFactory(new BundleContextFactory(reg, systemEvents));
	BundleLoader loader(cc, pBundleFactory, pBundleContextFactory);

	Bundle::Ptr pBundle = loader.createBundle(findBundle("com.appinf.osp.bundle2_1.0.0"));

	assert (pBundle->name() == "OSP Sample Bundle 2");
	assert (pBundle->symbolicName() == "com.appinf.osp.bundle2");
	assert (pBundle->version() == Version("1.0.0"));
	assert (pBundle->vendor() == "Applied Informatics");
	assert (pBundle->copyright() == "(c) 2007, Applied Informatics Software Engineering GmbH");

	assert (pBundle->properties().getString("greeting") == "Hello, world!");

	std::unique_ptr<std::istream> pStream(pBundle->getResource("lang.txt"));
	assert (pStream.get() != 0);
	std::string s;
	*pStream >> s;
	assert (s == "en-US");
}


void BundleTest::testPropertiesLocalization1()
{
	CodeCache cc("codeCache");
	ServiceRegistry reg;
	LanguageTag lang("de", "AT");

	BundleFactory::Ptr pBundleFactory(new BundleFactory(lang));
	Poco::OSP::SystemEvents systemEvents;
	BundleContextFactory::Ptr pBundleContextFactory(new BundleContextFactory(reg, systemEvents));
	BundleLoader loader(cc, pBundleFactory, pBundleContextFactory);

	Bundle::Ptr pBundle = loader.createBundle(findBundle("com.appinf.osp.bundle2_1.0.0"));

	assert (pBundle->name() == "OSP Sample Bundle 2");
	assert (pBundle->symbolicName() == "com.appinf.osp.bundle2");
	assert (pBundle->version() == Version("1.0.0"));
	assert (pBundle->vendor() == "Applied Informatics");
	assert (pBundle->copyright() == "(c) 2007, Applied Informatics Software Engineering GmbH");

	assert (pBundle->properties().getString("greeting") == "Servus!");

	std::unique_ptr<std::istream> pStream(pBundle->getLocalizedResource("lang.txt"));
	assert (pStream.get() != 0);
	std::string s;
	*pStream >> s;
	assert (s == "de-AT");
}


void BundleTest::testPropertiesLocalization2()
{
	CodeCache cc("codeCache");
	ServiceRegistry reg;
	LanguageTag lang("de", "DE");

	BundleFactory::Ptr pBundleFactory(new BundleFactory(lang));
	Poco::OSP::SystemEvents systemEvents;
	BundleContextFactory::Ptr pBundleContextFactory(new BundleContextFactory(reg, systemEvents));
	BundleLoader loader(cc, pBundleFactory, pBundleContextFactory);

	Bundle::Ptr pBundle = loader.createBundle(findBundle("com.appinf.osp.bundle2_1.0.0"));

	assert (pBundle->name() == "OSP Sample Bundle 2");
	assert (pBundle->symbolicName() == "com.appinf.osp.bundle2");
	assert (pBundle->version() == Version("1.0.0"));
	assert (pBundle->vendor() == "Applied Informatics");
	assert (pBundle->copyright() == "(c) 2007, Applied Informatics Software Engineering GmbH");

	assert (pBundle->properties().getString("greeting") == "Guten Tag!");

	std::unique_ptr<std::istream> pStream(pBundle->getLocalizedResource("lang.txt"));
	assert (pStream.get() != 0);
	std::string s;
	*pStream >> s;
	assert (s == "de");
}


void BundleTest::testBundleFile()
{
	CodeCache cc("codeCache");
	ServiceRegistry reg;
	LanguageTag lang("de", "DE");

	BundleFactory::Ptr pBundleFactory(new BundleFactory(lang));
	Poco::OSP::SystemEvents systemEvents;
	BundleContextFactory::Ptr pBundleContextFactory(new BundleContextFactory(reg, systemEvents));
	BundleLoader loader(cc, pBundleFactory, pBundleContextFactory);

	Bundle::Ptr pBundle = loader.createBundle(findBundle("com.appinf.osp.bundle2_1.0.0.bndl"));

	assert (pBundle->name() == "OSP Sample Bundle 2");
	assert (pBundle->symbolicName() == "com.appinf.osp.bundle2");
	assert (pBundle->version() == Version("1.0.0"));
	assert (pBundle->vendor() == "Applied Informatics");
	assert (pBundle->copyright() == "(c) 2007, Applied Informatics Software Engineering GmbH");

	assert (pBundle->properties().getString("greeting") == "Guten Tag!");

	std::unique_ptr<std::istream> pStream(pBundle->getLocalizedResource("lang.txt"));
	assert (pStream.get() != 0);
	std::string s;
	*pStream >> s;
	assert (s == "de");
}


void BundleTest::testResolve()
{
	CodeCache cc("codeCache");
	ServiceRegistry reg;
	LanguageTag lang("de", "DE");

	BundleFactory::Ptr pBundleFactory(new BundleFactory(lang));
	Poco::OSP::SystemEvents systemEvents;
	BundleContextFactory::Ptr pBundleContextFactory(new BundleContextFactory(reg, systemEvents));
	BundleLoader loader(cc, pBundleFactory, pBundleContextFactory);

	Bundle::Ptr pBundle1 = loader.createBundle(findBundle("com.appinf.osp.bundle1_1.0.0"));
	Bundle::Ptr pBundle2 = loader.createBundle(findBundle("com.appinf.osp.bundle2_1.0.0"));
	Bundle::Ptr pBundle3 = loader.createBundle(findBundle("com.appinf.osp.bundle3_1.0.0"));
	Bundle::Ptr pBundle4 = loader.createBundle(findBundle("com.appinf.osp.bundle4_1.0.0"));

	loader.loadBundle(pBundle1);
	loader.loadBundle(pBundle3);
	loader.loadBundle(pBundle4);

	// bundle3 requires bundle1, bundle2
	try
	{
		pBundle3->resolve();
		fail("bundle has unsatisfied dependency - must throw");
	}
	catch (Poco::OSP::BundleResolveException&)
	{
	}

	assert (pBundle1->isResolved());
	assert (!pBundle2->isResolved());
	assert (!pBundle3->isResolved());

	loader.loadBundle(pBundle2);
	pBundle3->resolve();

	assert (pBundle1->isResolved());
	assert (pBundle2->isResolved());
	assert (pBundle3->isResolved());

	// bundle4 requires bundle1, bundle2 v2.0.0
	try
	{
		pBundle4->resolve();
		fail("bundle has unsatisfied dependency - must throw");
	}
	catch (Poco::OSP::BundleResolveException&)
	{
	}
	assert (!pBundle4->isResolved());

	// bundle5 has cyclic dependency
	Bundle::Ptr pBundle5 = loader.createBundle(findBundle("com.appinf.osp.bundle5_1.0.0"));
	Bundle::Ptr pBundle6 = loader.createBundle(findBundle("com.appinf.osp.bundle6_1.0.0"));
	Bundle::Ptr pBundle7 = loader.createBundle(findBundle("com.appinf.osp.bundle7_1.0.0"));

	loader.loadBundle(pBundle5);
	loader.loadBundle(pBundle6);
	loader.loadBundle(pBundle7);

	try
	{
		pBundle5->resolve();
	}
	catch (Poco::OSP::BundleResolveException&)
	{
	}

	// bundle8 requires bundle3
	Bundle::Ptr pBundle8 = loader.createBundle(findBundle("com.appinf.osp.bundle8_1.0.0"));
	loader.loadBundle(pBundle8);

	pBundle8->resolve();
	assert (pBundle8->isResolved());
}


void BundleTest::testResolveModules()
{
	CodeCache cc("codeCache");
	ServiceRegistry reg;
	LanguageTag lang("de", "DE");

	BundleFactory::Ptr pBundleFactory(new BundleFactory(lang));
	Poco::OSP::SystemEvents systemEvents;
	BundleContextFactory::Ptr pBundleContextFactory(new BundleContextFactory(reg, systemEvents));
	BundleLoader loader(cc, pBundleFactory, pBundleContextFactory);

	Bundle::Ptr pBundle1 = loader.createBundle(findBundle("com.appinf.osp.bundleA_1.0.0"));
	Bundle::Ptr pBundle2 = loader.createBundle(findBundle("com.appinf.osp.bundleB_1.0.0"));
	Bundle::Ptr pBundle3 = loader.createBundle(findBundle("com.appinf.osp.bundleC_1.0.0"));
	Bundle::Ptr pBundle4 = loader.createBundle(findBundle("com.appinf.osp.bundleD_1.0.0"));
	Bundle::Ptr pBundle5 = loader.createBundle(findBundle("com.appinf.osp.bundleE_1.0.0"));
	Bundle::Ptr pBundle6 = loader.createBundle(findBundle("com.appinf.osp.bundleF_1.0.0"));

	loader.loadBundle(pBundle2);

	// bundle2 requires module provided by bundle1
	try
	{
		pBundle2->resolve();
		fail("bundle has unsatisfied module dependency - must throw");
	}
	catch (Poco::OSP::BundleResolveException&)
	{
	}

	assert (!pBundle1->isResolved());
	assert (!pBundle2->isResolved());

	loader.loadBundle(pBundle1);
	pBundle2->resolve();

	assert (pBundle1->isResolved());
	assert (pBundle2->isResolved());

	assert (pBundle1->resolvedDependencies().empty());
	assert (pBundle2->resolvedDependencies().size() == 1);
	assert (pBundle2->resolvedDependencies()[0].symbolicName == "com.appinf.osp.bundleA");

	const Bundle::ModuleProviders& provs2 = pBundle2->moduleProviders();
	assert (provs2.size() == 1);
	assert (provs2[0].module.symbolicName == "com.appinf.osp.module1");
	assert (provs2[0].bundles.size() == 1);
	assert (provs2[0].bundles[0].symbolicName == "com.appinf.osp.bundleA");

	loader.loadBundle(pBundle3);
	pBundle3->resolve();
	assert (pBundle3->isResolved());

	assert (pBundle3->resolvedDependencies().size() == 1);
	assert (pBundle3->resolvedDependencies()[0].symbolicName == "com.appinf.osp.bundleB");

	const Bundle::ModuleProviders& provs3 = pBundle3->moduleProviders();
	assert (provs3.size() == 1);
	assert (provs3[0].module.symbolicName == "com.appinf.osp.module2");
	assert (provs3[0].bundles.size() == 1);
	assert (provs3[0].bundles[0].symbolicName == "com.appinf.osp.bundleB");

	loader.loadBundle(pBundle5);
	// bundle5 requires module provided by bundle4
	try
	{
		pBundle5->resolve();
		fail("bundle has unsatisfied module dependency - must throw");
	}
	catch (Poco::OSP::BundleResolveException&)
	{
	}

	loader.loadBundle(pBundle4);
	pBundle5->resolve();
	assert (pBundle4->isResolved());
	assert (pBundle5->isResolved());

	assert (pBundle5->resolvedDependencies().size() == 2);
	assert (pBundle5->resolvedDependencies()[0].symbolicName == "com.appinf.osp.bundleA");
	assert (pBundle5->resolvedDependencies()[1].symbolicName == "com.appinf.osp.bundleD");

	const Bundle::ModuleProviders& provs5 = pBundle5->moduleProviders();
	assert (provs5.size() == 2);
	assert (provs5[0].module.symbolicName == "com.appinf.osp.module1");
	assert (provs5[0].bundles.size() == 1);
	assert (provs5[0].bundles[0].symbolicName == "com.appinf.osp.bundleA");
	assert (provs5[1].module.symbolicName == "com.appinf.osp.module3");
	assert (provs5[1].bundles.size() == 1);
	assert (provs5[1].bundles[0].symbolicName == "com.appinf.osp.bundleD");

	loader.loadBundle(pBundle6);
	// bundle6 requires module provided by bundle2, but in different version
	try
	{
		pBundle6->resolve();
		fail("bundle has unsatisfied module dependency (version) - must throw");
	}
	catch (Poco::OSP::BundleResolveException&)
	{
	}
}


void BundleTest::testStart()
{
	CodeCache cc("codeCache");
	ServiceRegistry reg;
	LanguageTag lang("en", "US");

	BundleFactory::Ptr pBundleFactory(new BundleFactory(lang));
	Poco::OSP::SystemEvents systemEvents;
	BundleContextFactory::Ptr pBundleContextFactory(new BundleContextFactory(reg, systemEvents));
	BundleLoader loader(cc, pBundleFactory, pBundleContextFactory);

	Bundle::Ptr pBundle1 = loader.createBundle(findBundle("com.appinf.osp.bundle1_1.0.0"));
	Bundle::Ptr pBundle2 = loader.createBundle(findBundle("com.appinf.osp.bundle2_1.0.0"));
	Bundle::Ptr pBundle3 = loader.createBundle(findBundle("com.appinf.osp.bundle3_1.0.0"));
	Bundle::Ptr pBundle8 = loader.createBundle(findBundle("com.appinf.osp.bundle8_1.0.0"));

	loader.loadBundle(pBundle1);
	loader.loadBundle(pBundle2);
	loader.loadBundle(pBundle3);
	loader.loadBundle(pBundle8);

	pBundle8->resolve();

	assert (pBundle1->isResolved());
	assert (pBundle2->isResolved());
	assert (pBundle3->isResolved());
	assert (pBundle8->isResolved());

	pBundle8->start();

	assert (pBundle1->isActive());
	assert (pBundle2->isActive());
	assert (pBundle3->isActive());
	assert (pBundle8->isActive());
}


void BundleTest::testActivator()
{
	CodeCache cc("codeCache");
	ServiceRegistry reg;
	LanguageTag lang("en", "US");

	BundleFactory::Ptr pBundleFactory(new BundleFactory(lang));
	Poco::OSP::SystemEvents systemEvents;
	BundleContextFactory::Ptr pBundleContextFactory(new BundleContextFactory(reg, systemEvents));
	BundleLoader loader(cc, pBundleFactory, pBundleContextFactory);

	Bundle::Ptr pBundle9 = loader.createBundle(findBundle("com.appinf.osp.bundle9_1.0.0"));
	loader.loadBundle(pBundle9);

	pBundle9->resolve();
	pBundle9->start();
	pBundle9->stop();

	loader.unloadBundle(pBundle9);
}


void BundleTest::testStopAll()
{
	CodeCache cc("codeCache");
	ServiceRegistry reg;
	LanguageTag lang("en", "US");

	BundleFactory::Ptr pBundleFactory(new BundleFactory(lang));
	Poco::OSP::SystemEvents systemEvents;
	BundleContextFactory::Ptr pBundleContextFactory(new BundleContextFactory(reg, systemEvents));
	BundleLoader loader(cc, pBundleFactory, pBundleContextFactory);

	Bundle::Ptr pBundle1 = loader.createBundle(findBundle("com.appinf.osp.bundle1_1.0.0"));
	Bundle::Ptr pBundle2 = loader.createBundle(findBundle("com.appinf.osp.bundle2_1.0.0"));
	Bundle::Ptr pBundle3 = loader.createBundle(findBundle("com.appinf.osp.bundle3_1.0.0"));
	Bundle::Ptr pBundle8 = loader.createBundle(findBundle("com.appinf.osp.bundle8_1.0.0"));

	loader.loadBundle(pBundle1);
	loader.loadBundle(pBundle2);
	loader.loadBundle(pBundle3);
	loader.loadBundle(pBundle8);

	pBundle8->resolve();
	pBundle8->start();

	assert (pBundle1->isActive());
	assert (pBundle2->isActive());
	assert (pBundle3->isActive());
	assert (pBundle8->isActive());

	loader.stopAllBundles();

	assert (pBundle1->state() == Bundle::BUNDLE_RESOLVED);
	assert (pBundle2->state() == Bundle::BUNDLE_RESOLVED);
	assert (pBundle3->state() == Bundle::BUNDLE_RESOLVED);
	assert (pBundle8->state() == Bundle::BUNDLE_RESOLVED);
}


void BundleTest::testResolveStartStopUnloadAll()
{
	CodeCache cc("codeCache");
	ServiceRegistry reg;
	LanguageTag lang("en", "US");

	BundleFactory::Ptr pBundleFactory(new BundleFactory(lang));
	Poco::OSP::SystemEvents systemEvents;
	BundleContextFactory::Ptr pBundleContextFactory(new BundleContextFactory(reg, systemEvents));
	BundleLoader loader(cc, pBundleFactory, pBundleContextFactory);

	Bundle::Ptr pBundle1 = loader.createBundle(findBundle("com.appinf.osp.bundle1_1.0.0"));
	Bundle::Ptr pBundle2 = loader.createBundle(findBundle("com.appinf.osp.bundle2_1.0.0"));
	Bundle::Ptr pBundle3 = loader.createBundle(findBundle("com.appinf.osp.bundle3_1.0.0"));
	Bundle::Ptr pBundle4 = loader.createBundle(findBundle("com.appinf.osp.bundle4_1.0.0"));
	Bundle::Ptr pBundle8 = loader.createBundle(findBundle("com.appinf.osp.bundle8_1.0.0"));
	Bundle::Ptr pBundle9 = loader.createBundle(findBundle("com.appinf.osp.bundle9_1.0.0"));

	loader.loadBundle(pBundle1);
	loader.loadBundle(pBundle2);
	loader.loadBundle(pBundle3);
	loader.loadBundle(pBundle4);
	loader.loadBundle(pBundle8);
	loader.loadBundle(pBundle9);

	loader.resolveAllBundles();

	assert (pBundle1->isResolved());
	assert (pBundle2->isResolved());
	assert (pBundle3->isResolved());
	assert (!pBundle4->isResolved());
	assert (pBundle8->isResolved());
	assert (pBundle9->isResolved());

	loader.startAllBundles();

	assert (pBundle1->isActive());
	assert (pBundle2->isActive());
	assert (pBundle3->isActive());
	assert (!pBundle4->isActive());
	assert (pBundle8->isActive());
	assert (pBundle9->isActive());

	loader.stopAllBundles();

	assert (pBundle1->state() == Bundle::BUNDLE_RESOLVED);
	assert (pBundle2->state() == Bundle::BUNDLE_RESOLVED);
	assert (pBundle3->state() == Bundle::BUNDLE_RESOLVED);
	assert (pBundle4->state() == Bundle::BUNDLE_INSTALLED);
	assert (pBundle8->state() == Bundle::BUNDLE_RESOLVED);
	assert (pBundle9->state() == Bundle::BUNDLE_RESOLVED);

	loader.unloadAllBundles();
}


void BundleTest::testExtensionBundle()
{
	CodeCache cc("codeCache");
	ServiceRegistry reg;
	LanguageTag lang("fr", "FR");

	BundleFactory::Ptr pBundleFactory(new BundleFactory(lang));
	Poco::OSP::SystemEvents systemEvents;
	BundleContextFactory::Ptr pBundleContextFactory(new BundleContextFactory(reg, systemEvents));
	BundleLoader loader(cc, pBundleFactory, pBundleContextFactory);

	Bundle::Ptr pBundle2 = loader.createBundle(findBundle("com.appinf.osp.bundle2_1.0.0"));
	Bundle::Ptr pBundleX = loader.createBundle(findBundle("com.appinf.osp.bundleX_1.0.0"));

	loader.loadBundle(pBundle2);
	loader.loadBundle(pBundleX);

	pBundle2->resolve();

	assert (pBundle2->properties().getString("someProperty") == "someValue");

	pBundleX->resolve();

	assert (pBundle2->properties().getString("someProperty") == "overriddenValue");

	assert (pBundleX->isExtensionBundle());
	assert (!pBundle2->isExtensionBundle());
	assert (pBundleX->extendedBundle() == pBundle2);
	assert (pBundle2->extendedBundle().isNull());

	assert (pBundle2->properties().getString("greeting") == "Bonjour!");

	std::unique_ptr<std::istream> pStream(pBundle2->getLocalizedResource("lang.txt"));
	assert (pStream.get() != 0);
	std::string s;
	*pStream >> s;
	assert (s == "fr-FR");

	std::unique_ptr<std::istream> pStream2(pBundle2->getLocalizedResource("lang.txt", LanguageTag("de", "AT")));
	assert (pStream2.get() != 0);
	*pStream2 >> s;
	assert (s == "de-AT");

	loader.unloadBundle(pBundleX);
	assert (pBundle2->properties().getString("greeting") == "Hello, world!");
}


void BundleTest::testSealedBundle()
{
	CodeCache cc("codeCache");
	ServiceRegistry reg;
	LanguageTag lang("en", "US");

	BundleFactory::Ptr pBundleFactory(new BundleFactory(lang));
	Poco::OSP::SystemEvents systemEvents;
	BundleContextFactory::Ptr pBundleContextFactory(new BundleContextFactory(reg, systemEvents));
	BundleLoader loader(cc, pBundleFactory, pBundleContextFactory);

	Bundle::Ptr pBundleG = loader.createBundle(findBundle("com.appinf.osp.bundleG_1.0.0"));
	Bundle::Ptr pBundleY = loader.createBundle(findBundle("com.appinf.osp.bundleY_1.0.0"));

	loader.loadBundle(pBundleG);
	loader.loadBundle(pBundleY);

	pBundleG->resolve();

	try
	{
		pBundleY->resolve();
		fail("sealed bundle - must throw");
	}
	catch (Poco::OSP::BundleSealedException&)
	{
	}

	assert (!pBundleY->isResolved());
}


void BundleTest::testStartAfterDependencyReplaced()
{
	CodeCache cc("codeCache");
	ServiceRegistry reg;
	LanguageTag lang("en", "US");

	BundleFactory::Ptr pBundleFactory(new BundleFactory(lang));
	Poco::OSP::SystemEvents systemEvents;
	BundleContextFactory::Ptr pBundleContextFactory(new BundleContextFactory(reg, systemEvents));
	BundleLoader loader(cc, pBundleFactory, pBundleContextFactory);

	// top requires user, user requires dep [1.0,2.0)
	Bundle::Ptr pDep100 = loader.loadBundle(createTestBundle("com.appinf.osp.test.dep", "1.0.0"));
	Bundle::Ptr pUser = loader.loadBundle(createTestBundle("com.appinf.osp.test.user", "1.0.0", "com.appinf.osp.test.dep;bundle-version=[1.0,2.0)"));
	Bundle::Ptr pTop = loader.loadBundle(createTestBundle("com.appinf.osp.test.top", "1.0.0", "com.appinf.osp.test.user"));

	pTop->resolve();
	pTop->start();

	assert (pDep100->isActive());
	assert (pUser->isActive());
	assert (pTop->isActive());
	assert (pUser->resolvedDependencies().size() == 1);
	assert (pUser->resolvedDependencies()[0].version == Version("1.0.0"));

	// upgrade dep at runtime: its dependents have to be stopped first
	pTop->stop();
	pUser->stop();
	pDep100->stop();
	loader.unloadBundle(pDep100);
	Bundle::Ptr pDep110 = loader.loadBundle(createTestBundle("com.appinf.osp.test.dep", "1.1.0"));

	assert (pDep110->state() == Bundle::BUNDLE_INSTALLED);
	assert (pUser->state() == Bundle::BUNDLE_RESOLVED);

	// 1.1.0 is within the range user requires: starting top resolves user
	// against the new version (and resolves and starts the new dep)
	pTop->start();

	assert (pDep110->isActive());
	assert (pUser->isActive());
	assert (pTop->isActive());
	assert (pUser->resolvedDependencies().size() == 1);
	assert (pUser->resolvedDependencies()[0].version == Version("1.1.0"));
}


void BundleTest::testStartAfterDependencyReplacedSameVersion()
{
	CodeCache cc("codeCache");
	ServiceRegistry reg;
	LanguageTag lang("en", "US");

	BundleFactory::Ptr pBundleFactory(new BundleFactory(lang));
	Poco::OSP::SystemEvents systemEvents;
	BundleContextFactory::Ptr pBundleContextFactory(new BundleContextFactory(reg, systemEvents));
	BundleLoader loader(cc, pBundleFactory, pBundleContextFactory);

	Bundle::Ptr pDep = loader.loadBundle(createTestBundle("com.appinf.osp.test.dep", "1.0.0"));
	Bundle::Ptr pUser = loader.loadBundle(createTestBundle("com.appinf.osp.test.user", "1.0.0", "com.appinf.osp.test.dep;bundle-version=[1.0,2.0)"));

	pUser->resolve();
	pUser->start();
	assert (pDep->isActive());
	assert (pUser->isActive());

	// replace dep by another build of the same version (no version bump)
	pUser->stop();
	pDep->stop();
	loader.unloadBundle(pDep);
	Bundle::Ptr pRebuiltDep = loader.loadBundle(createTestBundle("com.appinf.osp.test.dep", "1.0.0"));

	assert (pRebuiltDep->state() == Bundle::BUNDLE_INSTALLED);
	assert (pUser->state() == Bundle::BUNDLE_RESOLVED);

	// the version user was resolved against has not changed, but the bundle
	// has: starting user has to resolve the new one before starting it
	pUser->start();

	assert (pRebuiltDep->isActive());
	assert (pUser->isActive());
	assert (pUser->resolvedDependencies().size() == 1);
	assert (pUser->resolvedDependencies()[0].version == Version("1.0.0"));
}


void BundleTest::testStartAfterDependencyReplacedOutOfRange()
{
	CodeCache cc("codeCache");
	ServiceRegistry reg;
	LanguageTag lang("en", "US");

	BundleFactory::Ptr pBundleFactory(new BundleFactory(lang));
	Poco::OSP::SystemEvents systemEvents;
	BundleContextFactory::Ptr pBundleContextFactory(new BundleContextFactory(reg, systemEvents));
	BundleLoader loader(cc, pBundleFactory, pBundleContextFactory);

	Bundle::Ptr pDep100 = loader.loadBundle(createTestBundle("com.appinf.osp.test.dep", "1.0.0"));
	Bundle::Ptr pUser = loader.loadBundle(createTestBundle("com.appinf.osp.test.user", "1.0.0", "com.appinf.osp.test.dep;bundle-version=[1.0,2.0)"));

	pUser->resolve();
	loader.unloadBundle(pDep100);

	// required bundle gone
	try
	{
		pUser->start();
		fail("required bundle no longer available - must throw");
	}
	catch (Poco::OSP::BundleResolveException&)
	{
	}
	assert (pUser->state() == Bundle::BUNDLE_RESOLVED);

	// required bundle replaced by a version outside of the required range
	Bundle::Ptr pDep200 = loader.loadBundle(createTestBundle("com.appinf.osp.test.dep", "2.0.0"));
	try
	{
		pUser->start();
		fail("required bundle not available in requested version - must throw");
	}
	catch (Poco::OSP::BundleResolveException&)
	{
	}

	// the previous resolution is kept, and the bundle that does not qualify
	// has not been resolved on the way
	assert (pUser->state() == Bundle::BUNDLE_RESOLVED);
	assert (pDep200->state() == Bundle::BUNDLE_INSTALLED);
	assert (pUser->resolvedDependencies().size() == 1);
	assert (pUser->resolvedDependencies()[0].version == Version("1.0.0"));
}


void BundleTest::testStartAfterDependencyReplacedResolved()
{
	CodeCache cc("codeCache");
	ServiceRegistry reg;
	LanguageTag lang("en", "US");

	BundleFactory::Ptr pBundleFactory(new BundleFactory(lang));
	Poco::OSP::SystemEvents systemEvents;
	BundleContextFactory::Ptr pBundleContextFactory(new BundleContextFactory(reg, systemEvents));
	BundleLoader loader(cc, pBundleFactory, pBundleContextFactory);

	const std::string provideModule("Provide-Module: com.appinf.osp.test.module;module-version=1.0.0");
	const std::string requireModule("Require-Module: com.appinf.osp.test.module;module-version=1.0.0");

	Bundle::Ptr pDep = loader.loadBundle(createTestBundle("com.appinf.osp.test.dep", "1.0.0", "", provideModule));
	Bundle::Ptr pUser = loader.loadBundle(createTestBundle("com.appinf.osp.test.user", "1.0.0", "", requireModule));

	pUser->resolve();
	pUser->start();
	assert (pDep->isActive());
	assert (pUser->isActive());
	assert (pUser->resolvedDependencies().size() == 1);
	assert (pUser->resolvedDependencies()[0].id == pDep->id());

	// replace dep by another build of the same version, which is resolved
	// (e.g. on behalf of another dependent) before user is started again
	pUser->stop();
	pDep->stop();
	loader.unloadBundle(pDep);
	Bundle::Ptr pRebuiltDep = loader.loadBundle(createTestBundle("com.appinf.osp.test.dep", "1.0.0", "", provideModule));
	pRebuiltDep->resolve();

	assert (pRebuiltDep->id() != pDep->id());
	assert (pRebuiltDep->state() == Bundle::BUNDLE_RESOLVED);

	// neither the version nor the state of the replacement give it away
	pUser->start();

	assert (pRebuiltDep->isActive());
	assert (pUser->isActive());
	assert (pUser->resolvedDependencies().size() == 1);
	assert (pUser->resolvedDependencies()[0].id == pRebuiltDep->id());
	assert (pUser->moduleProviders().size() == 1);
	assert (pUser->moduleProviders()[0].bundles.size() == 1);
	assert (pUser->moduleProviders()[0].bundles[0].id == pRebuiltDep->id());

	// the next build no longer provides the module user requires
	pUser->stop();
	pRebuiltDep->stop();
	loader.unloadBundle(pRebuiltDep);
	Bundle::Ptr pStrippedDep = loader.loadBundle(createTestBundle("com.appinf.osp.test.dep", "1.0.0"));
	pStrippedDep->resolve();

	try
	{
		pUser->start();
		fail("required module no longer available - must throw");
	}
	catch (Poco::OSP::BundleResolveException&)
	{
	}
	assert (pUser->state() == Bundle::BUNDLE_RESOLVED);
	assert (pUser->resolvedDependencies().size() == 1);
	assert (pUser->resolvedDependencies()[0].id == pRebuiltDep->id());
}


void BundleTest::testStartAfterDependencyReplacedUnresolvable()
{
	CodeCache cc("codeCache");
	ServiceRegistry reg;
	LanguageTag lang("en", "US");

	BundleFactory::Ptr pBundleFactory(new BundleFactory(lang));
	Poco::OSP::SystemEvents systemEvents;
	BundleContextFactory::Ptr pBundleContextFactory(new BundleContextFactory(reg, systemEvents));
	BundleLoader loader(cc, pBundleFactory, pBundleContextFactory);

	Bundle::Ptr pDep100 = loader.loadBundle(createTestBundle("com.appinf.osp.test.dep", "1.0.0"));
	Bundle::Ptr pUser = loader.loadBundle(createTestBundle("com.appinf.osp.test.user", "1.0.0", "com.appinf.osp.test.dep;bundle-version=[1.0,2.0)"));

	pUser->resolve();
	loader.unloadBundle(pDep100);

	// the replacement is within the required range, but cannot be resolved
	Bundle::Ptr pDep110 = loader.loadBundle(createTestBundle("com.appinf.osp.test.dep", "1.1.0", "com.appinf.osp.test.missing"));

	loader.events().bundleResolving += Delegate<BundleTest, BundleEvent>(this, &BundleTest::handleEvent);
	_events.clear();
	try
	{
		pUser->start();
		fail("required bundle cannot be resolved - must throw");
	}
	catch (Poco::OSP::BundleResolveException&)
	{
	}
	loader.events().bundleResolving -= Delegate<BundleTest, BundleEvent>(this, &BundleTest::handleEvent);

	// resolving the replacement has been attempted once, not once more
	// (under the loader's mutex) while resolving user again
	assert (_events.size() == 1);
	assert (_events[0].bundle() == pDep110);
	assert (_events[0].what() == BundleEvent::EV_BUNDLE_RESOLVING);

	assert (pDep110->state() == Bundle::BUNDLE_INSTALLED);
	assert (pUser->state() == Bundle::BUNDLE_RESOLVED);
	assert (pUser->resolvedDependencies().size() == 1);
	assert (pUser->resolvedDependencies()[0].version == Version("1.0.0"));
}


void BundleTest::setUp()
{
	Poco::AutoPtr<Poco::Channel> pChannel(new Poco::ConsoleChannel);
	Logger::root().setChannel(pChannel);
	Logger::root().setLevel(Poco::Message::PRIO_DEBUG);

	CodeCache cc("codeCache");
	cc.clear();

	File dir(TEST_BUNDLES_DIR);
	if (dir.exists()) dir.remove(true);
}


void BundleTest::tearDown()
{
	CodeCache cc("codeCache");
	try
	{
		cc.clear();
	}
	catch (Poco::Exception&)
	{
	}

	try
	{
		File dir(TEST_BUNDLES_DIR);
		if (dir.exists()) dir.remove(true);
	}
	catch (Poco::Exception&)
	{
	}
}


std::string BundleTest::findBundle(const std::string& name)
{
	std::string path = OSPBundleTestSuite::findBundle(name);
	assert(!path.empty());
	return path;
}


std::string BundleTest::createTestBundle(const std::string& symbolicName, const std::string& version, const std::string& requireBundle, const std::string& extraHeader)
{
	Path bundlePath(Path::forDirectory(TEST_BUNDLES_DIR), symbolicName + "_" + version);
	bundlePath.makeDirectory();
	Path metaPath(bundlePath);
	metaPath.pushDirectory("META-INF");
	File(metaPath).createDirectories();

	Poco::FileOutputStream ostr(Path(metaPath, "manifest.mf").toString());
	ostr
		<< "Manifest-Version: 1.0\n"
		<< "Bundle-Name: " << symbolicName << "\n"
		<< "Bundle-SymbolicName: " << symbolicName << "\n"
		<< "Bundle-Version: " << version << "\n"
		<< "Bundle-Vendor: Applied Informatics\n";
	if (!requireBundle.empty())
	{
		ostr << "Require-Bundle: " << requireBundle << "\n";
	}
	if (!extraHeader.empty())
	{
		ostr << extraHeader << "\n";
	}
	ostr.close();

	return bundlePath.toString();
}


void BundleTest::handleEvent(const void* sender, Poco::OSP::BundleEvent& event)
{
	_events.push_back(event);
}


void BundleTest::handleStartingEvent(const void* sender, Poco::OSP::BundleEvent& event)
{
	_events.push_back(event);
	assert (event.bundle()->state() == Bundle::BUNDLE_STARTING);
}


void BundleTest::handleStartedEvent(const void* sender, Poco::OSP::BundleEvent& event)
{
	_events.push_back(event);
	assert (event.bundle()->state() == Bundle::BUNDLE_ACTIVE);
}


void BundleTest::handleStoppingEvent(const void* sender, Poco::OSP::BundleEvent& event)
{
	_events.push_back(event);
	assert (event.bundle()->state() == Bundle::BUNDLE_STOPPING);
}


void BundleTest::handleStoppedEvent(const void* sender, Poco::OSP::BundleEvent& event)
{
	_events.push_back(event);
	assert (event.bundle()->state() == Bundle::BUNDLE_RESOLVED);
}


CppUnit::Test* BundleTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("BundleTest");

	CppUnit_addTest(pSuite, BundleTest, testLoadOne);
	CppUnit_addTest(pSuite, BundleTest, testVersionConflict);
	CppUnit_addTest(pSuite, BundleTest, testProperties);
	CppUnit_addTest(pSuite, BundleTest, testPropertiesLocalization1);
	CppUnit_addTest(pSuite, BundleTest, testPropertiesLocalization2);
	CppUnit_addTest(pSuite, BundleTest, testBundleFile);
	CppUnit_addTest(pSuite, BundleTest, testResolve);
	CppUnit_addTest(pSuite, BundleTest, testResolveModules);
	CppUnit_addTest(pSuite, BundleTest, testStart);
	CppUnit_addTest(pSuite, BundleTest, testActivator);
	CppUnit_addTest(pSuite, BundleTest, testStopAll);
	CppUnit_addTest(pSuite, BundleTest, testResolveStartStopUnloadAll);
	CppUnit_addTest(pSuite, BundleTest, testExtensionBundle);
	CppUnit_addTest(pSuite, BundleTest, testSealedBundle);
	CppUnit_addTest(pSuite, BundleTest, testStartAfterDependencyReplaced);
	CppUnit_addTest(pSuite, BundleTest, testStartAfterDependencyReplacedSameVersion);
	CppUnit_addTest(pSuite, BundleTest, testStartAfterDependencyReplacedOutOfRange);
	CppUnit_addTest(pSuite, BundleTest, testStartAfterDependencyReplacedResolved);
	CppUnit_addTest(pSuite, BundleTest, testStartAfterDependencyReplacedUnresolvable);

	return pSuite;
}
