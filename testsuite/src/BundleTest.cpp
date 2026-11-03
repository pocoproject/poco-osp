//
// BundleTest.cpp
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "BundleTest.h"
#include "OSPBundleTestSuite.h"
#include "BundleArchive.h"
#include "CallbackDelegate.h"
#include "TestLoader.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestCase.h"
#include "CppUnit/TestSuite.h"
#include "Poco/OSP/Bundle.h"
#include "Poco/OSP/BundleFactory.h"
#include "Poco/OSP/BundleContextFactory.h"
#include "Poco/OSP/BundleLoader.h"
#include "Poco/OSP/CodeCache.h"
#include "Poco/OSP/BundleFilter.h"
#include "Poco/OSP/ServiceRegistry.h"
#include "Poco/OSP/LanguageTag.h"
#include "Poco/OSP/Version.h"
#include "Poco/OSP/OSPException.h"
#include "Poco/OSP/SystemEvents.h"
#include "Poco/OSP/ExtensionPointService.h"
#include "Poco/OSP/ExtensionPoint.h"
#include "Poco/OSP/OSPSubsystem.h"
#include "Poco/OSP/PreferencesService.h"
#include "Poco/OSP/BundleInstallerService.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/Util/Application.h"
#include "Poco/URIStreamOpener.h"
#include "Poco/DOM/Element.h"
#include "Poco/Path.h"
#include "Poco/File.h"
#include "Poco/FileStream.h"
#include "Poco/StreamCopier.h"
#include "Poco/SharedLibrary.h"
#include "Poco/TemporaryFile.h"
#include "Poco/Delegate.h"
#include "Poco/Logger.h"
#include "Poco/StreamChannel.h"
#include "Poco/Channel.h"
#include "Poco/Message.h"
#include "Poco/Event.h"
#include "Poco/AutoPtr.h"
#include "Poco/Exception.h"
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#if POCO_OS == POCO_OS_MAC_OS_X
#include <fcntl.h>
#include <semaphore.h>
#include <sys/stat.h>
#endif


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


namespace
{
	struct NotAnException
		/// Thrown by a test delegate as an exception that derives
		/// neither from Poco::Exception nor from std::exception.
	{
	};


	struct Resolution
		/// What a thread that resolves a bundle shares with the test.
	{
		Poco::Event resolving;
		Poco::Event done;
		bool circular = false;
			/// True iff the resolution has failed with a BundleResolveException.
		std::string error;
			/// The message of another exception that the resolution has thrown.
	};


	class SymbolicNameFilter: public Poco::OSP::BundleFilter
		/// Accepts the bundle with the given symbolic name only.
	{
	public:
		SymbolicNameFilter() = delete;
		explicit SymbolicNameFilter(const std::string& symbolicName):
			_symbolicName(symbolicName)
		{
		}

		~SymbolicNameFilter() override = default;

		[[nodiscard]] bool accept(Bundle::Ptr pBundle) override
		{
			return pBundle->symbolicName() == _symbolicName;
		}

	private:
		std::string _symbolicName;
	};


	class RecordingExtensionPoint: public Poco::OSP::ExtensionPoint
		/// Records each extension it handles or removes as "handled:"
		/// or "removed:", followed by the extension's value attribute.
	{
	public:
		RecordingExtensionPoint() = delete;
		explicit RecordingExtensionPoint(std::vector<std::string>& calls):
			_calls(calls)
		{
		}

		void handleExtension(Bundle::ConstPtr /*pBundle*/, Poco::XML::Element* pExtensionElem) override
		{
			_calls.push_back("handled:" + pExtensionElem->getAttribute("value"));
		}

		void removeExtension(Bundle::ConstPtr /*pBundle*/, Poco::XML::Element* pExtensionElem) override
		{
			_calls.push_back("removed:" + pExtensionElem->getAttribute("value"));
		}

	protected:
		~RecordingExtensionPoint() override = default;

	private:
		std::vector<std::string>& _calls;
	};


	class RecordingSubsystem: public Poco::OSP::OSPSubsystem
		/// Records the system events, each with the state of the
		/// bundle named in the constructor when the event is fired.
	{
	public:
		RecordingSubsystem() = delete;
		explicit RecordingSubsystem(const std::string& symbolicName):
			_symbolicName(symbolicName)
		{
		}

		~RecordingSubsystem() override = default;

		[[nodiscard]] const std::vector<std::string>& events() const
		{
			return _events;
		}

	protected:
		void startBundles(Poco::Util::Application& app) override
		{
			Bundle::Ptr pBundle = bundleLoader().findBundle(_symbolicName);
			if (pBundle == nullptr) throw Poco::NotFoundException("The repository holds no bundle", _symbolicName);
			Poco::OSP::BundleContext::Ptr pContext = bundleLoader().contextForBundle(pBundle);
			if (pContext == nullptr) throw Poco::NotFoundException("The bundle has no context", _symbolicName);
			pContext->systemEvents().systemStarted += Poco::delegate(this, &RecordingSubsystem::onSystemEvent);
			pContext->systemEvents().systemShuttingDown += Poco::delegate(this, &RecordingSubsystem::onSystemEvent);
			OSPSubsystem::startBundles(app);
		}

		void onSystemEvent(const void* /*pSender*/, Poco::OSP::SystemEvents::EventKind& kind)
		{
			const std::string what(kind == Poco::OSP::SystemEvents::EV_SYSTEM_STARTED ? "started" : "shutting down");
			Bundle::Ptr pBundle = bundleLoader().findBundle(_symbolicName);
			_events.push_back(what + ":" + (pBundle != nullptr ? pBundle->stateString() : "missing"));
		}

	private:
		std::string _symbolicName;
		std::vector<std::string> _events;
	};


	class SharedCodeCache: public CodeCache
		/// A shared code cache that removes the name of its named mutex when
		/// it is destroyed: Poco::NamedMutex keeps the name on UNIX platforms,
		/// which would leave one name behind per test run.
	{
	public:
		SharedCodeCache() = delete;
		explicit SharedCodeCache(const std::string& path):
			CodeCache(path, true),
			_mutexFile(mutexFile(path))
		{
		}

		SharedCodeCache(const SharedCodeCache&) = delete;
		SharedCodeCache(SharedCodeCache&&) = delete;

		~SharedCodeCache()
		{
#if POCO_OS == POCO_OS_MAC_OS_X
			sem_unlink(_mutexFile.c_str());
#elif defined(POCO_OS_FAMILY_UNIX)
			std::remove(_mutexFile.c_str());
#endif
		}

		SharedCodeCache& operator = (const SharedCodeCache&) = delete;
		SharedCodeCache& operator = (SharedCodeCache&&) = delete;

		[[nodiscard]] static bool hasMutexName([[maybe_unused]] const std::string& path)
			/// Returns true iff the name of the named mutex of a shared
			/// code cache with the given path exists.
		{
#if POCO_OS == POCO_OS_MAC_OS_X
			const std::string file(mutexFile(path));
			sem_t* pSemaphore = sem_open(file.c_str(), O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 1);
			if (pSemaphore == SEM_FAILED) return errno == EEXIST;
			sem_close(pSemaphore);
			sem_unlink(file.c_str());
			return false;
#elif defined(POCO_OS_FAMILY_UNIX)
			return File(mutexFile(path)).exists();
#else
			return false;
#endif
		}

	private:
		[[nodiscard]] static std::string mutexFile(const std::string& path)
			/// Returns the file that Poco::NamedMutex creates on UNIX platforms
			/// for the named mutex of a shared code cache with the given path.
		{
#if POCO_OS == POCO_OS_MAC_OS_X
			return "/" + mutexName(path) + ".mutex";
#else
			return "/tmp/" + mutexName(path) + ".mutex";
#endif
		}

		std::string _mutexFile;
	};


	class LoaderProbe
		/// For each event, calls into the loader from another thread, which
		/// blocks if the event is fired with the loader's mutex held. The
		/// destructor joins the threads.
	{
	public:
		LoaderProbe() = delete;
		explicit LoaderProbe(BundleLoader& loader):
			_loader(loader)
		{
		}

		LoaderProbe(const LoaderProbe&) = delete;
		LoaderProbe(LoaderProbe&&) = delete;

		~LoaderProbe()
		{
			for (auto& thread: _threads)
			{
				thread.join();
			}
		}

		LoaderProbe& operator = (const LoaderProbe&) = delete;
		LoaderProbe& operator = (LoaderProbe&&) = delete;

		void probe()
		{
			auto pProbeDone = std::make_shared<Poco::Event>();
			_threads.emplace_back([this, pProbeDone]()
			{
				std::vector<Bundle::Ptr> bundles;
				_loader.listBundles(bundles);
				pProbeDone->set();
			});
			++_probes;
			if (!pProbeDone->tryWait(PROBE_WAIT_MS)) ++_blocked;
		}

		[[nodiscard]] int probes() const
		{
			return _probes;
		}

		[[nodiscard]] int blocked() const
		{
			return _blocked;
		}

	private:
		static constexpr long PROBE_WAIT_MS = 10000;

		BundleLoader& _loader;
		std::vector<std::thread> _threads;
		int _probes = 0;
		int _blocked = 0;
	};


	class NullExtensionPoint: public Poco::OSP::ExtensionPoint
	{
	public:
		void handleExtension(Bundle::ConstPtr /*pBundle*/, Poco::XML::Element* /*pExtensionElem*/) override
		{
		}
	};


	class LibraryDecisionGate: public Poco::Channel
		/// Sets decided when the BundleLoader logs whether it keeps or
		/// removes the library libshared, and holds a removal until
		/// removalAllowed is set.
	{
	public:
		void log(const Poco::Message& msg) override
		{
			if (msg.getText() == "Library libshared is kept for another bundle.")
			{
				decided.set();
			}
			else if (msg.getText() == "Uninstalling library libshared.")
			{
				decided.set();
				removalAllowed.tryWait(REMOVAL_WAIT_MS);
			}
		}

		Poco::Event decided;
		Poco::Event removalAllowed{Poco::Event::EVENT_MANUALRESET};

	private:
		static constexpr long REMOVAL_WAIT_MS = 10000;
	};
}


BundleTest::BundleTest(const std::string& name): CppUnit::TestCase(name)
{
}


BundleTest::~BundleTest() = default;


void BundleTest::testLoadOne()
{
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();

	Bundle::Ptr pBundle = loader.createBundle(OSPBundleTestSuite::findBundle("com.appinf.osp.bundle1_1.0.0"));

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
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();

	Bundle::Ptr pBundle100 = loader.createBundle(OSPBundleTestSuite::findBundle("com.appinf.osp.bundle1_1.0.0"));
	Bundle::Ptr pBundle091 = loader.createBundle(OSPBundleTestSuite::findBundle("com.appinf.osp.bundle1_0.9.1"));

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


void BundleTest::testManifestPlaceholdersAreReplacedByProperties()
{
	const LanguageTag lang("en", "US");
	TestLoader testLoader(codeCachePath(), lang);
	BundleLoader& loader = testLoader.loader();

	Bundle::Ptr pBundle = loader.createBundle(OSPBundleTestSuite::findBundle("com.appinf.osp.bundle2_1.0.0"));

	assert (pBundle->name() == "OSP Sample Bundle 2");
	assert (pBundle->symbolicName() == "com.appinf.osp.bundle2");
	assert (pBundle->version() == Version("1.0.0"));
	assert (pBundle->vendor() == "Applied Informatics");
	assert (pBundle->copyright() == "(c) 2007, Applied Informatics Software Engineering GmbH");

	assert (pBundle->properties().getString("greeting") == "Hello, world!");

	std::unique_ptr<std::istream> pStream(pBundle->getResource("lang.txt"));
	assert (pStream != nullptr);
	std::string s;
	*pStream >> s;
	assert (s == "en-US");
}


void BundleTest::testLocalizedPropertiesExactLanguage()
{
	const LanguageTag lang("de", "AT");
	TestLoader testLoader(codeCachePath(), lang);
	BundleLoader& loader = testLoader.loader();

	Bundle::Ptr pBundle = loader.createBundle(OSPBundleTestSuite::findBundle("com.appinf.osp.bundle2_1.0.0"));

	assert (pBundle->name() == "OSP Sample Bundle 2");
	assert (pBundle->symbolicName() == "com.appinf.osp.bundle2");
	assert (pBundle->version() == Version("1.0.0"));
	assert (pBundle->vendor() == "Applied Informatics");
	assert (pBundle->copyright() == "(c) 2007, Applied Informatics Software Engineering GmbH");

	assert (pBundle->properties().getString("greeting") == "Servus!");

	std::unique_ptr<std::istream> pStream(pBundle->getLocalizedResource("lang.txt"));
	assert (pStream != nullptr);
	std::string s;
	*pStream >> s;
	assert (s == "de-AT");
}


void BundleTest::testLocalizedPropertiesFallBackToLanguage()
{
	const LanguageTag lang("de", "DE");
	TestLoader testLoader(codeCachePath(), lang);
	BundleLoader& loader = testLoader.loader();

	Bundle::Ptr pBundle = loader.createBundle(OSPBundleTestSuite::findBundle("com.appinf.osp.bundle2_1.0.0"));

	assert (pBundle->name() == "OSP Sample Bundle 2");
	assert (pBundle->symbolicName() == "com.appinf.osp.bundle2");
	assert (pBundle->version() == Version("1.0.0"));
	assert (pBundle->vendor() == "Applied Informatics");
	assert (pBundle->copyright() == "(c) 2007, Applied Informatics Software Engineering GmbH");

	assert (pBundle->properties().getString("greeting") == "Guten Tag!");

	std::unique_ptr<std::istream> pStream(pBundle->getLocalizedResource("lang.txt"));
	assert (pStream != nullptr);
	std::string s;
	*pStream >> s;
	assert (s == "de");
}


void BundleTest::testBundleFile()
{
	const LanguageTag lang("de", "DE");
	TestLoader testLoader(codeCachePath(), lang);
	BundleLoader& loader = testLoader.loader();

	Bundle::Ptr pBundle = loader.createBundle(createTestBundleArchive("com.appinf.osp.bundle2_1.0.0.bndl",
		BundleArchive::createFromDirectory(OSPBundleTestSuite::findBundle("com.appinf.osp.bundle2_1.0.0"))));

	assert (pBundle->name() == "OSP Sample Bundle 2");
	assert (pBundle->symbolicName() == "com.appinf.osp.bundle2");
	assert (pBundle->version() == Version("1.0.0"));
	assert (pBundle->vendor() == "Applied Informatics");
	assert (pBundle->copyright() == "(c) 2007, Applied Informatics Software Engineering GmbH");

	assert (pBundle->properties().getString("greeting") == "Guten Tag!");

	std::unique_ptr<std::istream> pStream(pBundle->getLocalizedResource("lang.txt"));
	assert (pStream != nullptr);
	std::string s;
	*pStream >> s;
	assert (s == "de");
}


void BundleTest::testResolve()
{
	TestLoader testLoader(codeCachePath(), LanguageTag("de", "DE"));
	BundleLoader& loader = testLoader.loader();

	Bundle::Ptr pBundle1 = loader.createBundle(OSPBundleTestSuite::findBundle("com.appinf.osp.bundle1_1.0.0"));
	Bundle::Ptr pBundle2 = loader.createBundle(OSPBundleTestSuite::findBundle("com.appinf.osp.bundle2_1.0.0"));
	Bundle::Ptr pBundle3 = loader.createBundle(OSPBundleTestSuite::findBundle("com.appinf.osp.bundle3_1.0.0"));
	Bundle::Ptr pBundle4 = loader.createBundle(OSPBundleTestSuite::findBundle("com.appinf.osp.bundle4_1.0.0"));

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

	// bundle8 requires bundle3
	Bundle::Ptr pBundle8 = loader.createBundle(OSPBundleTestSuite::findBundle("com.appinf.osp.bundle8_1.0.0"));
	loader.loadBundle(pBundle8);

	pBundle8->resolve();
	assert (pBundle8->isResolved());
}


void BundleTest::testResolveCircularDependencyThrows()
{
	TestLoader testLoader(codeCachePath(), LanguageTag("de", "DE"));
	BundleLoader& loader = testLoader.loader();

	loader.loadBundle(OSPBundleTestSuite::findBundle("com.appinf.osp.bundle1_1.0.0"));
	loader.loadBundle(OSPBundleTestSuite::findBundle("com.appinf.osp.bundle2_1.0.0"));

	// bundle5 requires bundle6, which requires bundle7, which requires bundle5
	Bundle::Ptr pBundle5 = loader.loadBundle(OSPBundleTestSuite::findBundle("com.appinf.osp.bundle5_1.0.0"));
	Bundle::Ptr pBundle6 = loader.loadBundle(OSPBundleTestSuite::findBundle("com.appinf.osp.bundle6_1.0.0"));
	Bundle::Ptr pBundle7 = loader.loadBundle(OSPBundleTestSuite::findBundle("com.appinf.osp.bundle7_1.0.0"));

	try
	{
		pBundle5->resolve();
		fail("circular dependency - must throw");
	}
	catch (Poco::OSP::BundleResolveException&)
	{
	}

	assertTrue (pBundle5->state() == Bundle::BUNDLE_INSTALLED);
	assertTrue (pBundle6->state() == Bundle::BUNDLE_INSTALLED);
	assertTrue (pBundle7->state() == Bundle::BUNDLE_INSTALLED);
}


void BundleTest::testResolveModules()
{
	TestLoader testLoader(codeCachePath(), LanguageTag("de", "DE"));
	BundleLoader& loader = testLoader.loader();

	Bundle::Ptr pBundle1 = loader.createBundle(OSPBundleTestSuite::findBundle("com.appinf.osp.bundleA_1.0.0"));
	Bundle::Ptr pBundle2 = loader.createBundle(OSPBundleTestSuite::findBundle("com.appinf.osp.bundleB_1.0.0"));
	Bundle::Ptr pBundle3 = loader.createBundle(OSPBundleTestSuite::findBundle("com.appinf.osp.bundleC_1.0.0"));
	Bundle::Ptr pBundle4 = loader.createBundle(OSPBundleTestSuite::findBundle("com.appinf.osp.bundleD_1.0.0"));
	Bundle::Ptr pBundle5 = loader.createBundle(OSPBundleTestSuite::findBundle("com.appinf.osp.bundleE_1.0.0"));
	Bundle::Ptr pBundle6 = loader.createBundle(OSPBundleTestSuite::findBundle("com.appinf.osp.bundleF_1.0.0"));

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
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();

	Bundle::Ptr pBundle1 = loader.createBundle(OSPBundleTestSuite::findBundle("com.appinf.osp.bundle1_1.0.0"));
	Bundle::Ptr pBundle2 = loader.createBundle(OSPBundleTestSuite::findBundle("com.appinf.osp.bundle2_1.0.0"));
	Bundle::Ptr pBundle3 = loader.createBundle(OSPBundleTestSuite::findBundle("com.appinf.osp.bundle3_1.0.0"));
	Bundle::Ptr pBundle8 = loader.createBundle(OSPBundleTestSuite::findBundle("com.appinf.osp.bundle8_1.0.0"));

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


void BundleTest::testActivatorStartAndStopAreCalled()
{
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();
	ServiceRegistry& reg = testLoader.registry();

	Bundle::Ptr pBundle9 = loader.createBundle(OSPBundleTestSuite::findBundle("com.appinf.osp.bundle9_1.0.0"));
	loader.loadBundle(pBundle9);

	pBundle9->resolve();
	pBundle9->start();
	assertTrue (pBundle9->isActive());
	assertTrue (pBundle9->activator() != nullptr);
	assertTrue (reg.findByName("com.appinf.osp.bundle9.activated") != nullptr);

	pBundle9->stop();
	assertTrue (pBundle9->state() == Bundle::BUNDLE_RESOLVED);
	assertTrue (reg.findByName("com.appinf.osp.bundle9.activated") == nullptr);

	loader.unloadBundle(pBundle9);
}


void BundleTest::testStopAll()
{
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();

	Bundle::Ptr pBundle1 = loader.createBundle(OSPBundleTestSuite::findBundle("com.appinf.osp.bundle1_1.0.0"));
	Bundle::Ptr pBundle2 = loader.createBundle(OSPBundleTestSuite::findBundle("com.appinf.osp.bundle2_1.0.0"));
	Bundle::Ptr pBundle3 = loader.createBundle(OSPBundleTestSuite::findBundle("com.appinf.osp.bundle3_1.0.0"));
	Bundle::Ptr pBundle8 = loader.createBundle(OSPBundleTestSuite::findBundle("com.appinf.osp.bundle8_1.0.0"));

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
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();

	Bundle::Ptr pBundle1 = loader.createBundle(OSPBundleTestSuite::findBundle("com.appinf.osp.bundle1_1.0.0"));
	Bundle::Ptr pBundle2 = loader.createBundle(OSPBundleTestSuite::findBundle("com.appinf.osp.bundle2_1.0.0"));
	Bundle::Ptr pBundle3 = loader.createBundle(OSPBundleTestSuite::findBundle("com.appinf.osp.bundle3_1.0.0"));
	Bundle::Ptr pBundle4 = loader.createBundle(OSPBundleTestSuite::findBundle("com.appinf.osp.bundle4_1.0.0"));
	Bundle::Ptr pBundle8 = loader.createBundle(OSPBundleTestSuite::findBundle("com.appinf.osp.bundle8_1.0.0"));
	Bundle::Ptr pBundle9 = loader.createBundle(OSPBundleTestSuite::findBundle("com.appinf.osp.bundle9_1.0.0"));

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
	const LanguageTag lang("fr", "FR");
	TestLoader testLoader(codeCachePath(), lang);
	BundleLoader& loader = testLoader.loader();

	Bundle::Ptr pBundle2 = loader.createBundle(OSPBundleTestSuite::findBundle("com.appinf.osp.bundle2_1.0.0"));
	Bundle::Ptr pBundleX = loader.createBundle(OSPBundleTestSuite::findBundle("com.appinf.osp.bundleX_1.0.0"));

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
	assert (pStream != nullptr);
	std::string s;
	*pStream >> s;
	assert (s == "fr-FR");

	std::unique_ptr<std::istream> pStream2(pBundle2->getLocalizedResource("lang.txt", LanguageTag("de", "AT")));
	assert (pStream2 != nullptr);
	*pStream2 >> s;
	assert (s == "de-AT");

	loader.unloadBundle(pBundleX);
	assert (pBundle2->properties().getString("greeting") == "Hello, world!");
}


void BundleTest::testExtensionBundleReplacesResource()
{
	TestLoader testLoader(codeCachePath(), LanguageTag("fr", "FR"));
	BundleLoader& loader = testLoader.loader();

	Bundle::Ptr pBundle2 = loader.createBundle(OSPBundleTestSuite::findBundle("com.appinf.osp.bundle2_1.0.0"));
	Bundle::Ptr pBundleX = loader.createBundle(OSPBundleTestSuite::findBundle("com.appinf.osp.bundleX_1.0.0"));

	loader.loadBundle(pBundle2);
	loader.loadBundle(pBundleX);

	pBundle2->resolve();
	pBundleX->resolve();

	std::unique_ptr<std::istream> pExtensionStream(pBundle2->getResource("bundle.properties"));
	assert (pExtensionStream != nullptr);
	std::string extensionProperties;
	Poco::StreamCopier::copyToString(*pExtensionStream, extensionProperties);
	assert (extensionProperties.find("someProperty=overriddenValue") != std::string::npos);

	loader.unloadBundle(pBundleX);

	std::unique_ptr<std::istream> pOwnStream(pBundle2->getResource("bundle.properties"));
	assert (pOwnStream != nullptr);
	std::string ownProperties;
	Poco::StreamCopier::copyToString(*pOwnStream, ownProperties);
	assert (ownProperties.find("greeting=Hello, world!") != std::string::npos);
}


void BundleTest::testExtensionBundleAfterUnloadingUnresolvedExtension()
{
	TestLoader testLoader(codeCachePath(), LanguageTag("fr", "FR"));
	BundleLoader& loader = testLoader.loader();

	Bundle::Ptr pBundle2 = loader.loadBundle(OSPBundleTestSuite::findBundle("com.appinf.osp.bundle2_1.0.0"));
	Bundle::Ptr pBundleX = loader.loadBundle(OSPBundleTestSuite::findBundle("com.appinf.osp.bundleX_1.0.0"));

	pBundle2->resolve();
	loader.unloadBundle(pBundleX);

	Bundle::Ptr pBundleXAgain = loader.loadBundle(OSPBundleTestSuite::findBundle("com.appinf.osp.bundleX_1.0.0"));
	pBundleXAgain->resolve();

	std::unique_ptr<std::istream> pStream(pBundle2->getResource("bundle.properties"));
	assert (pStream != nullptr);
	std::string properties;
	Poco::StreamCopier::copyToString(*pStream, properties);
	assert (properties.find("someProperty=overriddenValue") != std::string::npos);
}


void BundleTest::testSealedBundle()
{
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();

	Bundle::Ptr pBundleG = loader.createBundle(OSPBundleTestSuite::findBundle("com.appinf.osp.bundleG_1.0.0"));
	Bundle::Ptr pBundleY = loader.createBundle(OSPBundleTestSuite::findBundle("com.appinf.osp.bundleY_1.0.0"));

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
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();

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
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();

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
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();

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
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();

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
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();

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


void BundleTest::testLibraryNameThatIsNoFileNameIsNotInstalled()
{
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();
	CodeCache& cc = testLoader.codeCache();

	const std::string binDir("bin/" + loader.osName() + "/" + loader.osArchitecture() + "/");
	std::string archive = BundleArchive::create({
		{"META-INF/manifest.mf", BundleArchive::manifest("com.appinf.osp.test.libs", "1.0.0")},
		{binDir + "libgood", "good"},
		{binDir + "___evil", "evil"}
	});
	// Poco::Zip refuses to write an entry name with a backslash, which
	// another ZIP tool writes as is.
	const std::string evilName("..\\evil");
	BundleArchive::renameEntry(archive, binDir + "___evil", binDir + evilName);

	Bundle::Ptr pBundle = loader.loadBundle(createTestBundleArchive("com.appinf.osp.test.libs_1.0.0.bndl", archive));
	pBundle->resolve();

	assertTrue (cc.hasLibrary("libgood"));
	assertFalse (cc.hasLibrary(evilName));
}


void BundleTest::testCodeCacheRejectsLibraryNameThatIsNoFileName()
{
	CodeCache cc(codeCachePath());

	File outside(Path(_scratchPath, "outside.lib"));
	std::istringstream istr("library");
	bool rejected = false;
	try
	{
		cc.installLibrary("../outside.lib", istr);
	}
	catch (Poco::InvalidArgumentException&)
	{
		rejected = true;
	}
	const bool writtenOutside = outside.exists();
	if (writtenOutside) outside.remove();

	assertTrue (rejected);
	assertFalse (writtenOutside);

	try
	{
		cc.uninstallLibrary("../outside.lib");
		fail("library name with a path must be rejected");
	}
	catch (Poco::InvalidArgumentException&)
	{
	}
}


void BundleTest::testCopiedFailedEventHasItsOwnException()
{
	BundleEvent original(static_cast<Bundle*>(nullptr), Poco::IOException("start failed"));
	{
		BundleEvent copy(original);
		assertTrue (copy.exception() != nullptr);
		assertTrue (copy.exception() != original.exception());
	}
	{
		BundleEvent assigned(static_cast<Bundle*>(nullptr), BundleEvent::EV_BUNDLE_STARTED);
		assigned = original;
		assertTrue (assigned.exception() != nullptr);
		assertTrue (assigned.exception() != original.exception());
	}
	assertTrue (dynamic_cast<Poco::IOException*>(original.exception()) != nullptr);
}


void BundleTest::testStopAllAfterRequiredBundleUninstalled()
{
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();

	Bundle::Ptr pDep = loader.loadBundle(createTestBundle("com.appinf.osp.test.dep", "1.0.0"));
	Bundle::Ptr pUser = loader.loadBundle(createTestBundle("com.appinf.osp.test.user", "1.0.0", "com.appinf.osp.test.dep"));

	pUser->resolve();
	pUser->start();
	pDep->stop();
	pDep->uninstall();

	loader.stopAllBundles();
	assertTrue (pUser->state() == Bundle::BUNDLE_RESOLVED);

	loader.unloadAllBundles();
	std::vector<Bundle::Ptr> bundles;
	loader.listBundles(bundles);
	assertTrue (bundles.empty());
}


void BundleTest::testResolveModuleWithoutVersion()
{
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();

	Bundle::Ptr pDep = loader.loadBundle(createTestBundle("com.appinf.osp.test.dep", "1.0.0", "", "Provide-Module: com.appinf.osp.test.module;module-version=1.2.0"));
	Bundle::Ptr pUser = loader.loadBundle(createTestBundle("com.appinf.osp.test.user", "1.0.0", "", "Require-Module: com.appinf.osp.test.module"));

	pUser->resolve();

	assertTrue (pUser->isResolved());
	assertTrue (pUser->resolvedDependencies().size() == 1);
	assertTrue (pUser->resolvedDependencies()[0].id == pDep->id());
}


void BundleTest::testDamagedLibraryIsNotInstalled()
{
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();
	CodeCache& cc = testLoader.codeCache();

	const std::string content("library content");
	std::string archive = BundleArchive::create({
		{"META-INF/manifest.mf", BundleArchive::manifest("com.appinf.osp.test.damaged", "1.0.0")},
		{"bin/" + loader.osName() + "/" + loader.osArchitecture() + "/libdamaged", content}
	});
	// the entry is stored uncompressed: changing a byte breaks its checksum
	const std::string::size_type pos = archive.find(content);
	assertTrue (pos != std::string::npos);
	archive[pos] = 'L';

	Bundle::Ptr pBundle = loader.loadBundle(createTestBundleArchive("com.appinf.osp.test.damaged_1.0.0.bndl", archive));
	try
	{
		pBundle->resolve();
		fail("damaged library - must throw");
	}
	catch (Poco::FileException&)
	{
	}

	assertTrue (pBundle->state() == Bundle::BUNDLE_INSTALLED);
	std::vector<std::string> files;
	File(cc.pathFor("", false)).list(files);
	assertTrue (files.empty());
}


void BundleTest::testStoppedBundleRemovesOnlyItsOwnExtensionPoints()
{
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();
	Poco::OSP::ExtensionPointService::Ptr pXPS(new Poco::OSP::ExtensionPointService(loader.events()));

	Bundle::Ptr pBundleA = loader.loadBundle(createTestBundle("com.appinf.osp.test.a", "1.0.0"));
	Bundle::Ptr pBundleB = loader.loadBundle(createTestBundle("com.appinf.osp.test.b", "1.0.0"));
	pBundleA->resolve();
	pBundleA->start();
	pBundleB->resolve();
	pBundleB->start();

	pXPS->registerExtensionPoint(pBundleA, "test.point", new NullExtensionPoint);
	pXPS->unregisterExtensionPoint("test.point");
	pXPS->registerExtensionPoint(pBundleB, "test.point", new NullExtensionPoint);

	pBundleA->stop();

	// the point is registered by bundle B now
	pXPS->unregisterExtensionPoint("test.point");

	pBundleA->start();
	pXPS->registerExtensionPoint(pBundleA, "test.point.a", new NullExtensionPoint);

	pBundleA->stop();

	try
	{
		pXPS->unregisterExtensionPoint("test.point.a");
		fail("extension point of a stopped bundle - must be gone");
	}
	catch (Poco::NotFoundException&)
	{
	}
}


void BundleTest::testStartWhileRequiredBundleIsStarting()
{
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();

	Bundle::Ptr pDep = loader.loadBundle(createTestBundle("com.appinf.osp.test.dep", "1.0.0"));
	Bundle::Ptr pUser = loader.loadBundle(createTestBundle("com.appinf.osp.test.user", "1.0.0", "com.appinf.osp.test.dep"));
	pUser->resolve();

	// Holds the start of the required bundle in its bundleStarting event
	// until the gate is opened, and reports the start of the user.
	Poco::Event depHeld;
	Poco::Event gateOpened;
	Poco::Event userStarting;
	CallbackDelegate gateDelegate([&](BundleEvent& event)
	{
		if (event.bundle() == pDep)
		{
			depHeld.set();
			if (!gateOpened.tryWait(WAIT_MS)) throw Poco::TimeoutException("the gate is not opened");
		}
		else userStarting.set();
	});
	loader.events().bundleStarting += gateDelegate;

	std::string depError;
	std::thread depThread([&]()
	{
		try
		{
			pDep->start();
		}
		catch (Poco::Exception& exc)
		{
			depError = exc.displayText();
		}
	});
	const bool depWasHeld = depHeld.tryWait(WAIT_MS);

	std::string userError;
	Poco::Event userDone;
	std::thread userThread([&]()
	{
		try
		{
			pUser->start();
		}
		catch (Poco::Exception& exc)
		{
			userError = exc.displayText();
		}
		userDone.set();
	});
	const bool userWasStarting = userStarting.tryWait(WAIT_MS);

	// The user's start must wait for the required bundle; give it the time
	// to fail if it does not, then let the required bundle finish starting.
	const bool userDoneEarly = userDone.tryWait(USER_START_WAIT_MS);
	gateOpened.set();
	depThread.join();
	userThread.join();
	loader.events().bundleStarting -= gateDelegate;

	assertTrue (depWasHeld);
	assertTrue (userWasStarting);
	assertFalse (userDoneEarly);
	assertTrue (depError.empty());
	assertTrue (userError.empty());
	assertTrue (pDep->isActive());
	assertTrue (pUser->isActive());
}


void BundleTest::testBundlesRequiringOneBundleAreResolvedConcurrently()
{
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();

	Bundle::Ptr pDep = loader.loadBundle(createTestBundle("com.appinf.osp.test.dep", "1.0.0"));
	Bundle::Ptr pFirst = loader.loadBundle(
		createTestBundle("com.appinf.osp.test.first", "1.0.0", "com.appinf.osp.test.dep"));
	Bundle::Ptr pSecond = loader.loadBundle(
		createTestBundle("com.appinf.osp.test.second", "1.0.0", "com.appinf.osp.test.dep"));

	// Holds the resolution of the required bundle in its bundleResolving
	// event until the gate is opened, and reports the resolution of the
	// second bundle.
	Poco::Event depHeld;
	Poco::Event gateOpened;
	Poco::Event secondResolving;
	CallbackDelegate gateDelegate([&](BundleEvent& event)
	{
		if (event.bundle() == pDep)
		{
			depHeld.set();
			if (!gateOpened.tryWait(WAIT_MS)) throw Poco::TimeoutException("the gate is not opened");
		}
		else if (event.bundle() == pSecond) secondResolving.set();
	});
	loader.events().bundleResolving += gateDelegate;

	std::string firstError;
	std::thread firstThread([&]()
	{
		try
		{
			pFirst->resolve();
		}
		catch (Poco::Exception& exc)
		{
			firstError = exc.displayText();
		}
	});
	const bool depWasHeld = depHeld.tryWait(WAIT_MS);

	std::string secondError;
	Poco::Event secondDone;
	std::thread secondThread([&]()
	{
		try
		{
			pSecond->resolve();
		}
		catch (Poco::Exception& exc)
		{
			secondError = exc.displayText();
		}
		secondDone.set();
	});
	const bool secondWasResolving = secondResolving.tryWait(WAIT_MS);

	// If it does not wait for the required bundle, the second resolution completes within this time.
	const bool secondDoneEarly = secondDone.tryWait(USER_START_WAIT_MS);
	gateOpened.set();
	firstThread.join();
	secondThread.join();
	loader.events().bundleResolving -= gateDelegate;

	assertTrue (depWasHeld);
	assertTrue (secondWasResolving);
	assertFalse (secondDoneEarly);
	assertEqual ("", firstError);
	assertEqual ("", secondError);
	assertTrue (pDep->isResolved());
	assertTrue (pFirst->isResolved());
	assertTrue (pSecond->isResolved());
}


void BundleTest::testCircularDependencyResolvedInTwoThreadsThrows()
{
	// Shared with the threads, which are left running if they block, so that
	// the test fails instead of blocking the runner.
	auto pTestLoader = std::make_shared<TestLoader>(codeCachePath());
	BundleLoader& loader = pTestLoader->loader();

	Bundle::Ptr pFirst = loader.loadBundle(
		createTestBundle("com.appinf.osp.test.first", "1.0.0", "com.appinf.osp.test.second"));
	Bundle::Ptr pSecond = loader.loadBundle(
		createTestBundle("com.appinf.osp.test.second", "1.0.0", "com.appinf.osp.test.first"));
	auto pFirstResolution = std::make_shared<Resolution>();
	auto pSecondResolution = std::make_shared<Resolution>();

	// Each resolution waits in its bundleResolving event, which is fired with
	// the lifecycle mutex of its bundle held, until the other one has started.
	CallbackDelegate barrier([pFirst, pFirstResolution, pSecondResolution](BundleEvent& event)
	{
		const bool first = event.bundle() == pFirst;
		Resolution& own = first ? *pFirstResolution : *pSecondResolution;
		Resolution& other = first ? *pSecondResolution : *pFirstResolution;
		own.resolving.set();
		if (!other.resolving.tryWait(WAIT_MS)) throw Poco::TimeoutException("the other resolution has not started");
	});
	loader.events().bundleResolving += barrier;

	const auto resolveInThread = [pTestLoader](Bundle::Ptr pBundle, std::shared_ptr<Resolution> pResolution)
	{
		return std::thread([pTestLoader, pBundle = std::move(pBundle), pResolution = std::move(pResolution)]() mutable
		{
			try
			{
				pBundle->resolve();
			}
			catch (Poco::OSP::BundleResolveException&)
			{
				pResolution->circular = true;
			}
			catch (Poco::Exception& exc)
			{
				pResolution->error = exc.displayText();
			}
			pResolution->done.set();
		});
	};
	std::thread firstThread = resolveInThread(pFirst, pFirstResolution);
	std::thread secondThread = resolveInThread(pSecond, pSecondResolution);
	const bool firstDone = pFirstResolution->done.tryWait(WAIT_MS);
	const bool secondDone = pSecondResolution->done.tryWait(WAIT_MS);
	if (!firstDone || !secondDone)
	{
		firstThread.detach();
		secondThread.detach();
		fail("the resolutions of two bundles that require each other wait for each other");
	}
	firstThread.join();
	secondThread.join();
	loader.events().bundleResolving -= barrier;

	assertEqual ("", pFirstResolution->error);
	assertEqual ("", pSecondResolution->error);
	assertTrue (pFirstResolution->circular);
	assertTrue (pSecondResolution->circular);
	assertTrue (pFirst->state() == Bundle::BUNDLE_INSTALLED);
	assertTrue (pSecond->state() == Bundle::BUNDLE_INSTALLED);
}


void BundleTest::testUninstallWaitsForResolution()
{
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();

	Bundle::Ptr pBundle = loader.loadBundle(createTestBundle("com.appinf.osp.test.resolving", "1.0.0"));

	// Another thread uninstalls the bundle while it is being resolved.
	std::string uninstallError;
	Poco::Event uninstallDone;
	std::thread uninstallThread;
	bool uninstallDoneEarly = false;
	CallbackDelegate uninstallWhileResolving([&](BundleEvent& event)
	{
		if (event.bundle() != pBundle) return;
		uninstallThread = std::thread([&]()
		{
			try
			{
				pBundle->uninstall();
			}
			catch (Poco::Exception& exc)
			{
				uninstallError = exc.displayText();
			}
			uninstallDone.set();
		});
		uninstallDoneEarly = uninstallDone.tryWait(USER_START_WAIT_MS);
	});
	loader.events().bundleResolving += uninstallWhileResolving;
	try
	{
		pBundle->resolve();
	}
	catch (...)
	{
		if (uninstallThread.joinable()) uninstallThread.join();
		throw;
	}
	if (uninstallThread.joinable()) uninstallThread.join();
	loader.events().bundleResolving -= uninstallWhileResolving;

	assertFalse (uninstallDoneEarly);
	assertEqual ("", uninstallError);
	assertTrue (pBundle->state() == Bundle::BUNDLE_UNINSTALLED);
	assertTrue (loader.findBundle("com.appinf.osp.test.resolving") == nullptr);
}


void BundleTest::testStopAllContinuesAfterFailingBundle()
{
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();

	Bundle::Ptr pFailing = loader.loadBundle(createTestBundle("com.appinf.osp.test.failing", "1.0.0"));
	Bundle::Ptr pOther = loader.loadBundle(createTestBundle("com.appinf.osp.test.other", "1.0.0"));
	pFailing->resolve();
	pFailing->start();
	pOther->resolve();
	pOther->start();

	loader.events().bundleStopping += Delegate<BundleTest, BundleEvent>(this, &BundleTest::handleStoppingEventFails);
	loader.bundleError += Delegate<BundleTest, const BundleLoader::BundleError>(this, &BundleTest::handleBundleError);
	_bundleErrors.clear();

	loader.stopAllBundles();

	loader.events().bundleStopping -= Delegate<BundleTest, BundleEvent>(this, &BundleTest::handleStoppingEventFails);
	loader.bundleError -= Delegate<BundleTest, const BundleLoader::BundleError>(this, &BundleTest::handleBundleError);

	assertTrue (pFailing->isActive());
	assertTrue (pOther->state() == Bundle::BUNDLE_RESOLVED);
	assertTrue (_bundleErrors == std::vector<std::string>({"com.appinf.osp.test.failing"}));

	pFailing->stop();
	loader.unloadAllBundles();
}


void BundleTest::testStopAllContinuesAfterUnknownException()
{
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();

	Bundle::Ptr pFailing = loader.loadBundle(createTestBundle("com.appinf.osp.test.failing", "1.0.0"));
	Bundle::Ptr pOther = loader.loadBundle(createTestBundle("com.appinf.osp.test.other", "1.0.0"));
	pFailing->resolve();
	pFailing->start();
	pOther->resolve();
	pOther->start();

	CallbackDelegate stoppingFails([&](BundleEvent& event)
	{
		if (event.bundle() == pFailing) throw NotAnException();
	});
	loader.events().bundleStopping += stoppingFails;
	loader.bundleError += Delegate<BundleTest, const BundleLoader::BundleError>(this, &BundleTest::handleBundleError);
	_bundleErrors.clear();

	loader.stopAllBundles();

	loader.events().bundleStopping -= stoppingFails;
	loader.bundleError -= Delegate<BundleTest, const BundleLoader::BundleError>(this, &BundleTest::handleBundleError);

	assertTrue (pFailing->isActive());
	assertTrue (pOther->state() == Bundle::BUNDLE_RESOLVED);
	assertTrue (_bundleErrors == std::vector<std::string>({"com.appinf.osp.test.failing"}));

	pFailing->stop();
	loader.unloadAllBundles();
}


void BundleTest::testUnloadAllKeepsBundleThatFailsToStop()
{
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();

	Bundle::Ptr pFailing = loader.loadBundle(createTestBundle("com.appinf.osp.test.failing", "1.0.0"));
	Bundle::Ptr pOther = loader.loadBundle(createTestBundle("com.appinf.osp.test.other", "1.0.0"));
	pFailing->resolve();
	pFailing->start();
	pOther->resolve();
	pOther->start();

	loader.events().bundleStopping += Delegate<BundleTest, BundleEvent>(this, &BundleTest::handleStoppingEventFails);
	loader.unloadAllBundles();
	loader.events().bundleStopping -= Delegate<BundleTest, BundleEvent>(this, &BundleTest::handleStoppingEventFails);

	std::vector<Bundle::Ptr> bundles;
	loader.listBundles(bundles);
	assertTrue (bundles == std::vector<Bundle::Ptr>({pFailing}));
	assertTrue (pFailing->isActive());

	pFailing->stop();
	loader.unloadAllBundles();
}


void BundleTest::testEventsAreFiredWithoutLoaderLock()
{
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();

	LoaderProbe loaderProbe(loader);
	CallbackDelegate probe([&loaderProbe](BundleEvent& /*event*/)
	{
		loaderProbe.probe();
	});
	loader.events().bundleLoaded += probe;
	loader.events().bundleResolving += probe;
	loader.events().bundleResolved += probe;
	loader.events().bundleUnloaded += probe;

	Bundle::Ptr pDep = loader.loadBundle(createTestBundle("com.appinf.osp.test.dep", "1.0.0"));
	Bundle::Ptr pUser = loader.loadBundle(createTestBundle("com.appinf.osp.test.user", "1.0.0", "com.appinf.osp.test.dep"));
	// First in the order of symbolic names, it is resolved first, and
	// resolves the bundles it requires on the way.
	Bundle::Ptr pApp = loader.loadBundle(createTestBundle("com.appinf.osp.test.app", "1.0.0", "com.appinf.osp.test.user"));
	loader.resolveAllBundles();
	loader.unloadBundle(pApp);
	loader.unloadBundle(pUser);
	pDep->uninstall();

	loader.events().bundleLoaded -= probe;
	loader.events().bundleResolving -= probe;
	loader.events().bundleResolved -= probe;
	loader.events().bundleUnloaded -= probe;

	// loaded, resolving, resolved and unloaded for each of the three bundles
	assertEqual (12, loaderProbe.probes());
	assertEqual (0, loaderProbe.blocked());
}


void BundleTest::testResolveEventsOfRequiredBundlesKeepTheirOrder()
{
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();

	Bundle::Ptr pDep = loader.loadBundle(createTestBundle("com.appinf.osp.test.dep", "1.0.0"));
	Bundle::Ptr pUser = loader.loadBundle(createTestBundle("com.appinf.osp.test.user", "1.0.0", "com.appinf.osp.test.dep"));

	_events.clear();
	loader.events().bundleResolving += Delegate<BundleTest, BundleEvent>(this, &BundleTest::handleEvent);
	loader.events().bundleResolved += Delegate<BundleTest, BundleEvent>(this, &BundleTest::handleEvent);
	pUser->resolve();
	loader.events().bundleResolving -= Delegate<BundleTest, BundleEvent>(this, &BundleTest::handleEvent);
	loader.events().bundleResolved -= Delegate<BundleTest, BundleEvent>(this, &BundleTest::handleEvent);

	assertEqual (4, static_cast<int>(_events.size()));
	assertTrue (_events[0].bundle() == pUser && _events[0].what() == BundleEvent::EV_BUNDLE_RESOLVING);
	assertTrue (_events[1].bundle() == pDep && _events[1].what() == BundleEvent::EV_BUNDLE_RESOLVING);
	assertTrue (_events[2].bundle() == pDep && _events[2].what() == BundleEvent::EV_BUNDLE_RESOLVED);
	assertTrue (_events[3].bundle() == pUser && _events[3].what() == BundleEvent::EV_BUNDLE_RESOLVED);
}


void BundleTest::testResolveEventsAreFiredWhenResolutionFails()
{
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();

	Bundle::Ptr pDep = loader.loadBundle(createTestBundle("com.appinf.osp.test.dep", "1.0.0", "com.appinf.osp.test.missing"));
	Bundle::Ptr pUser = loader.loadBundle(createTestBundle("com.appinf.osp.test.user", "1.0.0", "com.appinf.osp.test.dep"));

	_events.clear();
	LoaderProbe loaderProbe(loader);
	const Delegate<BundleTest, BundleEvent> record(this, &BundleTest::handleEvent);
	CallbackDelegate probe([&loaderProbe](BundleEvent& /*event*/)
	{
		loaderProbe.probe();
	});
	loader.events().bundleResolving += record;
	loader.events().bundleResolving += probe;
	try
	{
		pUser->resolve();
		fail("a required bundle is missing - must throw");
	}
	catch (Poco::OSP::BundleResolveException&)
	{
	}
	loader.events().bundleResolving -= record;
	loader.events().bundleResolving -= probe;

	assertEqual (2, static_cast<int>(_events.size()));
	assertTrue (_events[0].bundle() == pUser && _events[0].what() == BundleEvent::EV_BUNDLE_RESOLVING);
	assertTrue (_events[1].bundle() == pDep && _events[1].what() == BundleEvent::EV_BUNDLE_RESOLVING);
	assertEqual (2, loaderProbe.probes());
	assertEqual (0, loaderProbe.blocked());
}


void BundleTest::testBundleResolvingDelegateRefusesRequiredAndExtendedBundle()
{
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();

	// app and ext come first by symbolic name and resolve the refused bundle on the way.
	Bundle::Ptr pRefused = loader.loadBundle(createTestBundle("com.appinf.osp.test.refused", "1.0.0"));
	Bundle::Ptr pApp = loader.loadBundle(createTestBundle("com.appinf.osp.test.app", "1.0.0",
		"com.appinf.osp.test.refused"));
	Bundle::Ptr pExt = loader.loadBundle(createTestBundle("com.appinf.osp.test.ext", "1.0.0",
		"", "Extends-Bundle: com.appinf.osp.test.refused"));

	std::vector<Bundle::State> refusedStates;
	CallbackDelegate refuse([&](BundleEvent& event)
	{
		if (event.bundle() == pRefused)
		{
			refusedStates.push_back(pRefused->state());
			throw Poco::OSP::BundleResolveException("refused", pRefused->symbolicName());
		}
	});
	loader.events().bundleResolving += refuse;
	loader.resolveAllBundles();
	loader.startAllBundles();
	loader.events().bundleResolving -= refuse;

	// app, ext, and the refused bundle itself
	assertTrue (refusedStates == std::vector<Bundle::State>(3, Bundle::BUNDLE_INSTALLED));
	assertTrue (pRefused->state() == Bundle::BUNDLE_INSTALLED);
	assertTrue (pApp->state() == Bundle::BUNDLE_INSTALLED);
	assertTrue (pExt->state() == Bundle::BUNDLE_INSTALLED);
}


void BundleTest::testActivatorIsResetWhenBundleIsUnloaded()
{
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();

	Bundle::Ptr pBundle9 = loader.createBundle(OSPBundleTestSuite::findBundle("com.appinf.osp.bundle9_1.0.0"));
	loader.loadBundle(pBundle9);

	pBundle9->resolve();
	pBundle9->start();
	pBundle9->stop();
	assertTrue (pBundle9->activator() != nullptr);

	loader.unloadBundle(pBundle9);
	assertTrue (pBundle9->activator() == nullptr);
}


void BundleTest::testActivatorIsResetWhenBundlesAreUnloadedWithoutLibraries()
{
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();

	Bundle::Ptr pBundle9 = loader.createBundle(OSPBundleTestSuite::findBundle("com.appinf.osp.bundle9_1.0.0"));
	loader.loadBundle(pBundle9);

	pBundle9->resolve();
	pBundle9->start();
	assertTrue (pBundle9->activator() != nullptr);

	loader.unloadAllBundles(false);
	assertTrue (pBundle9->activator() == nullptr);
}


void BundleTest::testStartAllContinuesAfterUnknownException()
{
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();

	Bundle::Ptr pFailing = loader.loadBundle(createTestBundle("com.appinf.osp.test.failing", "1.0.0"));
	Bundle::Ptr pOther = loader.loadBundle(createTestBundle("com.appinf.osp.test.other", "1.0.0"));
	loader.resolveAllBundles();

	loader.events().bundleStarting += Delegate<BundleTest, BundleEvent>(this, &BundleTest::handleStartingEventThrowsUnknown);
	loader.bundleError += Delegate<BundleTest, const BundleLoader::BundleError>(this, &BundleTest::handleBundleError);
	_bundleErrors.clear();

	loader.startAllBundles();

	loader.events().bundleStarting -= Delegate<BundleTest, BundleEvent>(this, &BundleTest::handleStartingEventThrowsUnknown);
	loader.bundleError -= Delegate<BundleTest, const BundleLoader::BundleError>(this, &BundleTest::handleBundleError);

	assertTrue (pFailing->state() == Bundle::BUNDLE_RESOLVED);
	assertTrue (pOther->isActive());
	assertTrue (_bundleErrors == std::vector<std::string>({"com.appinf.osp.test.failing"}));

	loader.unloadAllBundles();
}


void BundleTest::testStartErrorNamesBundleThatFailsInStartingEvent()
{
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();

	Bundle::Ptr pFailing = loader.loadBundle(createTestBundle("com.appinf.osp.test.failing", "1.0.0"));
	Bundle::Ptr pOther = loader.loadBundle(createTestBundle("com.appinf.osp.test.other", "1.0.0"));
	loader.resolveAllBundles();
	pOther->start();

	std::ostringstream log;
	Logger& loaderLogger = Logger::get("osp.core.BundleLoader");
	loaderLogger.setChannel(new Poco::StreamChannel(log));
	loader.events().bundleStarting += Delegate<BundleTest, BundleEvent>(this, &BundleTest::handleStartingEventThrowsUnknown);

	loader.startAllBundles();

	loader.events().bundleStarting -= Delegate<BundleTest, BundleEvent>(this, &BundleTest::handleStartingEventThrowsUnknown);
	loaderLogger.setChannel(Logger::root().getChannel());

	// The start of the failing bundle fails before its activator is loaded,
	// so the bundle started last before it must not be named.
	assertTrue (log.str().find("Failed to start bundle com.appinf.osp.test.failing: ") != std::string::npos);
	assertTrue (log.str().find("required by") == std::string::npos);

	loader.unloadAllBundles();
}


void BundleTest::testStartErrorNamesBundleThatFailsWhileAnotherThreadStartsBundle()
{
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();

	Bundle::Ptr pFailing = loader.loadBundle(createTestBundle("com.appinf.osp.test.failing", "1.0.0"));
	// not started by startAllBundles(), but by the other thread
	Bundle::Ptr pOther = loader.loadBundle(createTestBundle("com.appinf.osp.test.other", "1.0.0", "", "Bundle-LazyStart: true"));
	loader.resolveAllBundles();

	// The other bundle begins to start after the failing one, and its start
	// is still in progress when startAllBundles() logs the failure.
	Poco::Event otherMayStart;
	Poco::Event otherStarting;
	Poco::Event startAllDone;
	std::atomic<bool> otherStarted(false);
	std::atomic<bool> otherHeld(false);
	CallbackDelegate startingDelegate([&](BundleEvent& event)
	{
		if (event.bundle() == pFailing)
		{
			otherMayStart.set();
			if (otherStarting.tryWait(WAIT_MS)) otherStarted = true;
			throw Poco::IllegalStateException("bundle cannot be started", event.bundle()->symbolicName());
		}
		else if (event.bundle() == pOther)
		{
			otherStarting.set();
			if (startAllDone.tryWait(WAIT_MS)) otherHeld = true;
		}
	});
	std::ostringstream log;
	Logger& loaderLogger = Logger::get("osp.core.BundleLoader");
	loaderLogger.setChannel(new Poco::StreamChannel(log));
	loader.events().bundleStarting += startingDelegate;

	std::atomic<bool> otherMayStartSeen(false);
	std::thread otherThread([&]()
	{
		if (otherMayStart.tryWait(WAIT_MS))
		{
			otherMayStartSeen = true;
			pOther->start();
		}
	});
	try
	{
		loader.startAllBundles();
	}
	catch (...)
	{
		startAllDone.set();
		otherThread.join();
		throw;
	}
	startAllDone.set();
	otherThread.join();

	loader.events().bundleStarting -= startingDelegate;
	loaderLogger.setChannel(Logger::root().getChannel());

	assertTrue (otherMayStartSeen);
	assertTrue (otherStarted);
	assertTrue (otherHeld);
	assertTrue (pOther->isActive());
	assertTrue (log.str().find("Failed to start bundle com.appinf.osp.test.failing: ") != std::string::npos);
	assertTrue (log.str().find("required by") == std::string::npos);

	loader.unloadAllBundles();
}


void BundleTest::testUninstallRemovesBundleUnlessPrevented()
{
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();

	Bundle::Ptr pRemovable = loader.loadBundle(createTestBundle("com.appinf.osp.test.removable", "1.0.0"));
	Bundle::Ptr pKept = loader.loadBundle(createTestBundle("com.appinf.osp.test.kept", "1.0.0", "", "Bundle-PreventUninstall: true"));

	pRemovable->uninstall();
	assertTrue (pRemovable->state() == Bundle::BUNDLE_UNINSTALLED);
	assertEqual ("uninstalled", pRemovable->stateString());
	assertTrue (loader.findBundle("com.appinf.osp.test.removable") == nullptr);
	// not in the primary path of a bundle repository
	assertTrue (File(pRemovable->path()).exists());

	try
	{
		pKept->uninstall();
		fail("manifest prevents uninstall - must throw");
	}
	catch (Poco::OSP::BundleUninstallException&)
	{
	}
	assertTrue (pKept->state() == Bundle::BUNDLE_INSTALLED);
	assertTrue (loader.findBundle("com.appinf.osp.test.kept") == pKept);
}


void BundleTest::testUninstallOfUnresolvedBundleWithLibrary()
{
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();

	// The required bundle is missing: the bundle is never resolved, and its
	// library never reaches the code cache.
	const std::string bundlePath(createTestBundle("com.appinf.osp.test.unresolved", "1.0.0",
		"com.appinf.osp.test.missing"));
	Path libraryPath(Path::forDirectory(bundlePath));
	libraryPath.pushDirectory("bin");
	libraryPath.pushDirectory(loader.osName());
	libraryPath.pushDirectory(loader.osArchitecture());
	File(libraryPath).createDirectories();
	libraryPath.setFileName("libunresolved");
	Poco::FileOutputStream(libraryPath.toString()) << "library";
	Bundle::Ptr pBundle = loader.loadBundle(bundlePath);
	loader.resolveAllBundles();
	assertTrue (pBundle->state() == Bundle::BUNDLE_INSTALLED);

	_events.clear();
	loader.events().bundleUninstalled += Delegate<BundleTest, BundleEvent>(this, &BundleTest::handleEvent);
	pBundle->uninstall();
	loader.events().bundleUninstalled -= Delegate<BundleTest, BundleEvent>(this, &BundleTest::handleEvent);

	assertTrue (pBundle->state() == Bundle::BUNDLE_UNINSTALLED);
	assertTrue (loader.findBundle("com.appinf.osp.test.unresolved") == nullptr);
	assertEqual (1, static_cast<int>(_events.size()));
	assertTrue (_events[0].bundle() == pBundle && _events[0].what() == BundleEvent::EV_BUNDLE_UNINSTALLED);
}


void BundleTest::testTemporaryDirectoryThatCannotBeRemovedIsLogged()
{
#if defined(POCO_OS_FAMILY_UNIX)
	// declared before the loader, which logs to it until tearDown() restores the channel
	std::ostringstream log;
	CodeCache cc(codeCachePath());
	ServiceRegistry reg;
	Poco::OSP::SystemEvents systemEvents;
	BundleLoader loader(cc,
		BundleFactory::Ptr(new BundleFactory(LanguageTag("en", "US"))),
		BundleContextFactory::Ptr(new BundleContextFactory(reg, systemEvents, Path(_scratchPath, "data"))));

	Bundle::Ptr pBundle = loader.loadBundle(createTestBundle("com.appinf.osp.test.temporary", "1.0.0"));
	Path lockedDir(loader.contextForBundle(pBundle)->temporaryDirectory());
	lockedDir.pushDirectory("locked");
	File(lockedDir).createDirectories();
	Poco::FileOutputStream(Path(lockedDir, "file").toString()) << "data";
	// Without write permission, the file in the directory cannot be removed.
	File(lockedDir).setWriteable(false);

	Logger::get("osp.bundle.com.appinf.osp.test.temporary").setChannel(new Poco::StreamChannel(log));
	loader.unloadBundle(pBundle);
	File(lockedDir).setWriteable(true);

	assertTrue (log.str().find("The temporary directory " + Path(lockedDir).parent().toString()) == 0);
#endif
}


void BundleTest::testUninstallCompletesIfBundleDirectoryCannotBeDeleted()
{
#if defined(POCO_OS_FAMILY_UNIX)
	// declared before the loader, which logs to it until tearDown() restores the channel
	std::ostringstream log;
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();

	const std::string bundlePath(createTestBundle("com.appinf.osp.test.undeletable", "1.0.0"));
	Bundle::Ptr pBundle = loader.loadBundle(bundlePath);
	loader.setDeleteOnUninstall(pBundle, true);
	loader.events().bundleUninstalled += Poco::delegate(this, &BundleTest::handleEvent);
	// Without write permission, the manifest in the directory cannot be deleted.
	Path metaPath(Path::forDirectory(bundlePath));
	metaPath.pushDirectory("META-INF");
	File(metaPath).setWriteable(false);

	Logger::get("osp.core.BundleLoader").setChannel(Poco::makeAuto<Poco::StreamChannel>(log));
	try
	{
		pBundle->uninstall();
	}
	catch (...)
	{
		File(metaPath).setWriteable(true);
		throw;
	}
	File(metaPath).setWriteable(true);
	loader.events().bundleUninstalled -= Poco::delegate(this, &BundleTest::handleEvent);

	assertTrue (pBundle->state() == Bundle::BUNDLE_UNINSTALLED);
	assertTrue (loader.findBundle("com.appinf.osp.test.undeletable") == nullptr);
	assertEqual (1, static_cast<int>(_events.size()));
	assertTrue (_events.front().bundle() == pBundle && _events.front().what() == BundleEvent::EV_BUNDLE_UNINSTALLED);
	assertTrue (log.str().find("Bundle com.appinf.osp.test.undeletable is uninstalled, but its file or directory '"
		+ bundlePath + "' cannot be deleted") != std::string::npos);
	assertTrue (File(bundlePath).exists());
#endif
}


void BundleTest::testUninstallKeepsLibraryThatAnotherBundleShips()
{
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();
	CodeCache& cc = testLoader.codeCache();

	std::vector<Bundle::Ptr> bundles;
	for (const char* symbolicName: {"com.appinf.osp.test.first", "com.appinf.osp.test.second"})
	{
		const std::string bundlePath(createTestBundle(symbolicName, "1.0.0"));
		Path libraryPath(Path::forDirectory(bundlePath));
		libraryPath.pushDirectory("bin");
		libraryPath.pushDirectory(loader.osName());
		libraryPath.pushDirectory(loader.osArchitecture());
		File(libraryPath).createDirectories();
		libraryPath.setFileName("libshared");
		Poco::FileOutputStream(libraryPath.toString()) << "library";
		bundles.push_back(loader.loadBundle(bundlePath));
	}
	Bundle::Ptr pFirst = bundles.front();
	Bundle::Ptr pSecond = bundles.back();
	pFirst->resolve();
	assertTrue (cc.hasLibrary("libshared"));

	// The second bundle is resolved, relying on the library in the code cache,
	// between the decision of the first bundle's uninstall and the removal.
	Poco::AutoPtr<LibraryDecisionGate> pGate(new LibraryDecisionGate);
	Logger& loaderLogger = Logger::get("osp.core.BundleLoader");
	loaderLogger.setChannel(pGate);
	loaderLogger.setLevel(Poco::Message::PRIO_DEBUG);
	std::thread uninstaller([&pFirst]() { pFirst->uninstall(); });
	const bool decided = pGate->decided.tryWait(WAIT_MS);
	try
	{
		if (decided) pSecond->resolve();
	}
	catch (...)
	{
		pGate->removalAllowed.set();
		uninstaller.join();
		throw;
	}
	pGate->removalAllowed.set();
	uninstaller.join();
	loaderLogger.setChannel(Logger::root().getChannel());

	assertTrue (decided);
	assertTrue (pSecond->isResolved());
	assertTrue (cc.hasLibrary("libshared"));

	pSecond->uninstall();
	assertFalse (cc.hasLibrary("libshared"));
}


void BundleTest::testListRequiringBundlesInStopOrder()
{
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();

	Bundle::Ptr pDep = loader.loadBundle(createTestBundle("com.appinf.osp.test.dep", "1.0.0",
		"", "Provide-Module: com.appinf.osp.test.module;module-version=1.0.0"));
	Bundle::Ptr pUser = loader.loadBundle(createTestBundle("com.appinf.osp.test.user", "1.0.0",
		"com.appinf.osp.test.dep"));
	Bundle::Ptr pApp = loader.loadBundle(createTestBundle("com.appinf.osp.test.app", "1.0.0",
		"com.appinf.osp.test.user"));
	Bundle::Ptr pModuleUser = loader.loadBundle(createTestBundle("com.appinf.osp.test.moduleuser", "1.0.0",
		"", "Require-Module: com.appinf.osp.test.module"));
	Bundle::Ptr pOther = loader.loadBundle(createTestBundle("com.appinf.osp.test.other", "1.0.0"));
	loader.resolveAllBundles();

	std::vector<Bundle::Ptr> requiring;
	loader.contextForBundle(pOther)->listRequiringBundles(pDep, requiring);

	assertEqual (3, static_cast<int>(requiring.size()));
	const auto itApp = std::find(requiring.begin(), requiring.end(), pApp);
	const auto itUser = std::find(requiring.begin(), requiring.end(), pUser);
	assertTrue (itApp != requiring.end());
	assertTrue (itUser != requiring.end());
	assertTrue (itApp < itUser);
	assertTrue (std::find(requiring.begin(), requiring.end(), pModuleUser) != requiring.end());
}


void BundleTest::testStartAllStartsByRunLevelAndSkipsLazyStart()
{
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();

	loader.loadBundle(createTestBundle("com.appinf.osp.test.late", "1.0.0", "", "Bundle-RunLevel: 300"));
	loader.loadBundle(createTestBundle("com.appinf.osp.test.early", "1.0.0", "", "Bundle-RunLevel: 100"));
	Bundle::Ptr pLazy = loader.loadBundle(createTestBundle("com.appinf.osp.test.lazy", "1.0.0", "", "Bundle-LazyStart: true"));
	loader.loadBundle(createTestBundle("com.appinf.osp.test.middle", "1.0.0", "", "Bundle-RunLevel: 200"));
	loader.resolveAllBundles();

	loader.events().bundleStarted += Delegate<BundleTest, BundleEvent>(this, &BundleTest::handleEvent);
	_events.clear();
	loader.startAllBundles();
	loader.events().bundleStarted -= Delegate<BundleTest, BundleEvent>(this, &BundleTest::handleEvent);

	std::vector<std::string> started;
	started.reserve(_events.size());
	for (const auto& event: _events)
	{
		started.push_back(event.bundle()->symbolicName());
	}
	assertTrue (started == std::vector<std::string>({"com.appinf.osp.test.early", "com.appinf.osp.test.middle", "com.appinf.osp.test.late"}));
	assertTrue (pLazy->state() == Bundle::BUNDLE_RESOLVED);
	assertEqual ("resolved", pLazy->stateString());

	loader.unloadAllBundles();
}


void BundleTest::testFindListUnloadAndContextOfBundles()
{
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();

	Bundle::Ptr pA = loader.loadBundle(createTestBundle("com.appinf.osp.test.a", "1.0.0"));
	Bundle::Ptr pB = loader.loadBundle(createTestBundle("com.appinf.osp.test.b", "1.0.0"));
	Bundle::Ptr pNotLoaded = loader.createBundle(createTestBundle("com.appinf.osp.test.c", "1.0.0"));

	assertTrue (loader.findBundle(pA->id()) == pA);
	assertTrue (loader.findBundle(pNotLoaded->id()) == nullptr);

	std::vector<Bundle::Ptr> bundles;
	loader.listBundles(bundles, new SymbolicNameFilter("com.appinf.osp.test.b"));
	assertTrue (bundles == std::vector<Bundle::Ptr>({pB}));

	try
	{
		loader.contextForBundle(pNotLoaded);
		fail("bundle not loaded - must throw");
	}
	catch (Poco::OSP::BundleException&)
	{
	}

	pA->resolve();
	pA->start();
	try
	{
		loader.unloadBundle(pA);
		fail("active bundle - must throw");
	}
	catch (Poco::OSP::BundleStateException&)
	{
	}
	assertTrue (loader.findBundle("com.appinf.osp.test.a") == pA);
	assertTrue (pA->isActive());

	loader.unloadAllBundles();
}


void BundleTest::testActivatorStartThatThrowsLeavesBundleResolved()
{
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();

	Bundle::Ptr pBundle = loader.loadBundle(createActivatorBundle("com.appinf.osp.test.failing", "FailingBundleActivator;library=TestBundle"));
	pBundle->resolve();

	loader.events().bundleFailed += Delegate<BundleTest, BundleEvent>(this, &BundleTest::handleEvent);
	_events.clear();
	try
	{
		pBundle->start();
		fail("activator start fails - must throw");
	}
	catch (Poco::IllegalStateException&)
	{
	}
	loader.events().bundleFailed -= Delegate<BundleTest, BundleEvent>(this, &BundleTest::handleEvent);

	assertTrue (pBundle->state() == Bundle::BUNDLE_RESOLVED);
	assertTrue (_events.size() == 1);
	assertTrue (_events[0].what() == BundleEvent::EV_BUNDLE_FAILED);
	assertTrue (_events[0].bundle() == pBundle);
	assertTrue (dynamic_cast<Poco::IllegalStateException*>(_events[0].exception()) != nullptr);

	loader.unloadAllBundles();
}


void BundleTest::testActivatorThatCannotBeLoadedFailsEveryStart()
{
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();
	CodeCache& cc = testLoader.codeCache();

	Bundle::Ptr pMissingClass = loader.loadBundle(createActivatorBundle("com.appinf.osp.test.missingclass", "MissingBundleActivator;library=TestBundle"));
	Bundle::Ptr pMissingLibrary = loader.loadBundle(createActivatorBundle("com.appinf.osp.test.missinglibrary", "MyBundleActivator;library=MissingLibrary"));
	pMissingClass->resolve();
	pMissingLibrary->resolve();

	for (int attempt = 0; attempt < 2; ++attempt)
	{
		try
		{
			pMissingClass->start();
			fail("activator class missing from the library - must throw");
		}
		catch (Poco::NotFoundException&)
		{
		}
		assertTrue (pMissingClass->state() == Bundle::BUNDLE_RESOLVED);

		try
		{
			pMissingLibrary->start();
			fail("activator library missing - must throw");
		}
		catch (Poco::LibraryLoadException&)
		{
		}
		assertTrue (pMissingLibrary->state() == Bundle::BUNDLE_RESOLVED);
	}

	// Loading the library here does not initialize it; the count is that of the
	// class loaders that still hold it after the failed starts.
	using InitializationCountFunc = int (*)();
	Poco::SharedLibrary library(cc.pathFor("TestBundle"));
	const auto initializationCount = reinterpret_cast<InitializationCountFunc>(library.getSymbol("testBundleInitializationCount"));
	assertEqual (0, initializationCount());
	library.unload();

	loader.unloadAllBundles();
}


void BundleTest::testCodeCacheInstallsAndUninstallsLibrary()
{
	CodeCache cc(codeCachePath());
	assertFalse (cc.hasLibrary("libtest"));

	std::istringstream istr("library content");
	cc.installLibrary("libtest", istr);
	assertTrue (cc.hasLibrary("libtest"));
	const std::string libraryPath(Path(Path::forDirectory(codeCachePath()), "libtest").toString());
	assertEqual (libraryPath, cc.pathFor("libtest", false));
	assertEqual (libraryPath + Poco::SharedLibrary::suffix(), cc.pathFor("libtest"));
	assertTrue (cc.libraryTimestamp("libtest") == File(libraryPath).getLastModified());
	Poco::FileInputStream libraryStream(libraryPath);
	std::string content;
	Poco::StreamCopier::copyToString(libraryStream, content);
	// Windows does not remove a file that is open.
	libraryStream.close();
	assertEqual ("library content", content);

	cc.uninstallLibrary("libtest");
	assertFalse (cc.hasLibrary("libtest"));

	{
		SharedCodeCache shared(codeCachePath());
		CodeCache::Lock lock(shared);
		std::istringstream sharedStream("shared library content");
		shared.installLibrary("libshared", sharedStream);
	}
	assertTrue (cc.hasLibrary("libshared"));
	assertFalse (SharedCodeCache::hasMutexName(codeCachePath()));
}


void BundleTest::testLibraryEntryOutsideCodeDirectoryIsNotInstalled()
{
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();
	CodeCache& cc = testLoader.codeCache();

	const std::string binDir("bin/" + loader.osName() + "/" + loader.osArchitecture() + "/");
	std::string archive = BundleArchive::create({
		{"META-INF/manifest.mf", BundleArchive::manifest("com.appinf.osp.test.outside", "1.0.0")},
		{binDir + "___x", "outside"}
	});
	// Poco::Zip refuses to write an entry name with a parent reference, which
	// another ZIP tool writes as is.
	BundleArchive::renameEntry(archive, binDir + "___x", binDir + "../x");

	Bundle::Ptr pBundle = loader.loadBundle(createTestBundleArchive("com.appinf.osp.test.outside_1.0.0.bndl", archive));
	pBundle->resolve();

	assertFalse (File(Path(_scratchPath, "x")).exists());
	std::vector<std::string> files;
	File(cc.pathFor("", false)).list(files);
	assertTrue (files.empty());
}


void BundleTest::testExtensionsAreHandledOnStartAndRemovedOnStop()
{
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();
	Poco::OSP::ExtensionPointService::Ptr pXPS(new Poco::OSP::ExtensionPointService(loader.events()));

	Bundle::Ptr pProvider = loader.loadBundle(createTestBundle("com.appinf.osp.test.provider", "1.0.0"));
	pProvider->resolve();
	pProvider->start();
	std::vector<std::string> calls;
	pXPS->registerExtensionPoint(pProvider, "test.point", new RecordingExtensionPoint(calls));

	const std::string userPath(createTestBundle("com.appinf.osp.test.user", "1.0.0"));
	{
		Poco::FileOutputStream ostr(Path(userPath, "extensions.xml").toString());
		ostr
			<< "<extensions>\n"
			<< "  <extension point=\"test.point\" value=\"1\"/>\n"
			<< "  <extension point=\"unknown.point\" value=\"2\"/>\n"
			<< "  <extension point=\"test.point\" value=\"3\"/>\n"
			<< "</extensions>\n";
	}
	const std::string brokenPath(createTestBundle("com.appinf.osp.test.broken", "1.0.0"));
	{
		Poco::FileOutputStream ostr(Path(brokenPath, "extensions.xml").toString());
		ostr << "<extensions><extension point=\"test.point\"";
	}

	Bundle::Ptr pUser = loader.loadBundle(userPath);
	pUser->resolve();
	pUser->start();
	assertTrue (calls == std::vector<std::string>({"handled:1", "handled:3"}));

	pUser->stop();
	assertTrue (calls == std::vector<std::string>({"handled:1", "handled:3", "removed:3", "removed:1"}));

	// malformed extensions are logged; the bundle starts
	Bundle::Ptr pBroken = loader.loadBundle(brokenPath);
	pBroken->resolve();
	pBroken->start();
	assertTrue (pBroken->isActive());
	assertTrue (calls.size() == 4);

	loader.unloadAllBundles();
}


void BundleTest::testSubsystemStartsBundlesOfConfiguredRepository()
{
	const std::string repositoryPath(Path(_scratchPath, "repository").toString());
	File(repositoryPath).createDirectories();
	{
		Poco::FileOutputStream ostr(Path(repositoryPath, "com.appinf.osp.test.system_1.0.0.bndl").toString());
		ostr << BundleArchive::create({{"META-INF/manifest.mf", BundleArchive::manifest("com.appinf.osp.test.system", "1.0.0")}});
	}

	// the application replaces the channel of every logger, which tearDown() restores
	{
		Poco::AutoPtr<Poco::Util::Application> pApp(new Poco::Util::Application);
		Poco::Util::Application& app = *pApp;
		app.config().setString("osp.codeCache", codeCachePath());
		app.config().setString("osp.bundleRepository", repositoryPath);
		app.config().setString("osp.data", Path(_scratchPath, "data").toString());
		app.config().setString("osp.language", "de-AT");

		Poco::AutoPtr<RecordingSubsystem> pSubsystem(new RecordingSubsystem("com.appinf.osp.test.system"));
		pSubsystem->initialize(app);

		Bundle::ConstPtr pBundle = pSubsystem->bundleLoader().findBundle("com.appinf.osp.test.system");
		assertTrue (pBundle != nullptr);
		assertTrue (pBundle->isActive());
		assertEqual ("de-AT", pBundle->language().toString());
		assertTrue (File(codeCachePath()).isDirectory());
		assertEqual (pSubsystem->bundleLoader().osName(), app.config().getString("osp.osname"));
		ServiceRegistry& reg = pSubsystem->serviceRegistry();
		assertTrue (reg.findByName(Poco::OSP::ExtensionPointService::SERVICE_NAME) != nullptr);
		assertTrue (reg.findByName(Poco::OSP::PreferencesService::SERVICE_NAME) != nullptr);
		assertTrue (reg.findByName(Poco::OSP::BundleInstallerService::SERVICE_NAME) != nullptr);

		pSubsystem->uninitialize();
		assertTrue (pSubsystem->events() == std::vector<std::string>({"started:active", "shutting down:active"}));
		assertTrue (reg.findByName(Poco::OSP::PreferencesService::SERVICE_NAME) == nullptr);
		assertTrue (pSubsystem->bundleLoader().findBundle("com.appinf.osp.test.system") == nullptr);
	}
}


void BundleTest::testFailedSubsystemInitializationUnregistersStreamsAndServices()
{
	const std::string repositoryPath(Path(_scratchPath, "repository").toString());
	File(repositoryPath).createDirectories();

	// the application replaces the channel of every logger, which tearDown() restores
	Poco::AutoPtr<Poco::Util::Application> pApp(new Poco::Util::Application);
	Poco::Util::Application& app = *pApp;
	app.config().setString("osp.codeCache", codeCachePath());
	app.config().setString("osp.bundleRepository", repositoryPath);
	app.config().setString("osp.data", Path(_scratchPath, "data").toString());

	// starts the bundles only if the repository holds this bundle
	Poco::AutoPtr<RecordingSubsystem> pSubsystem(new RecordingSubsystem("com.appinf.osp.test.missing"));
	try
	{
		pSubsystem->initialize(app);
		fail("the bundle is missing - must throw");
	}
	catch (Poco::NotFoundException&)
	{
	}
	assertFalse (Poco::URIStreamOpener::defaultOpener().supportsScheme("bndl"));
	ServiceRegistry& reg = pSubsystem->serviceRegistry();
	assertTrue (reg.findByName(Poco::OSP::ExtensionPointService::SERVICE_NAME) == nullptr);
	assertTrue (reg.findByName(Poco::OSP::PreferencesService::SERVICE_NAME) == nullptr);
	assertTrue (reg.findByName(Poco::OSP::BundleInstallerService::SERVICE_NAME) == nullptr);
}


void BundleTest::testConcurrentStartsAndStopsOfDependencyChain()
{
	// Released only when both threads have finished: threads that block each
	// other are left running.
	auto pTestLoader = std::make_unique<TestLoader>(codeCachePath());
	BundleLoader& loader = pTestLoader->loader();

	Bundle::Ptr pDep = loader.loadBundle(createTestBundle("com.appinf.osp.test.dep", "1.0.0"));
	Bundle::Ptr pUser = loader.loadBundle(createTestBundle("com.appinf.osp.test.user", "1.0.0",
		"com.appinf.osp.test.dep"));
	Bundle::Ptr pApp = loader.loadBundle(createTestBundle("com.appinf.osp.test.app", "1.0.0",
		"com.appinf.osp.test.user"));
	pApp->resolve();

	auto pContextsMissing = std::make_unique<std::atomic<int>>(0);
	CallbackDelegate probe([&loader, &contextsMissing = *pContextsMissing](BundleEvent& event)
	{
		if (loader.contextForBundle(event.bundle()) == nullptr) ++contextsMissing;
	});
	loader.events().bundleStarting += probe;

	auto pUnexpected = std::make_unique<std::atomic<int>>(0);
	auto pFinished = std::make_unique<std::atomic<int>>(0);
	auto pDone = std::make_unique<Poco::Event>();
	auto cycle = [&unexpected = *pUnexpected, &finished = *pFinished, &done = *pDone](Bundle::Ptr pBundle)
	{
		for (int i = 0; i < START_STOP_CYCLES; ++i)
		{
			try
			{
				pBundle->start();
				pBundle->stop();
			}
			catch (Poco::OSP::BundleStateException&)
			{
				// the other thread has started or stopped the bundle
			}
			catch (...)
			{
				++unexpected;
			}
		}
		if (++finished == 2) done.set();
	};
	std::thread appThread(cycle, pApp);
	std::thread userThread(cycle, pUser);
	const bool completed = pDone->tryWait(WAIT_MS);
	if (!completed)
	{
		appThread.detach();
		userThread.detach();
		static std::vector<const void*> blockedObjects;
		blockedObjects.push_back(pDone.release());
		blockedObjects.push_back(pFinished.release());
		blockedObjects.push_back(pUnexpected.release());
		blockedObjects.push_back(pContextsMissing.release());
		blockedObjects.push_back(pTestLoader.release());
		fail("concurrent starts and stops did not complete");
	}
	appThread.join();
	userThread.join();
	loader.events().bundleStarting -= probe;

	assertEqual (0, pUnexpected->load());
	assertEqual (0, pContextsMissing->load());

	loader.unloadAllBundles();
}


void BundleTest::setUp()
{
	_pRootChannel = Logger::root().getChannel();
	_rootLevel = Logger::root().getLevel();
	_scratchPath = Poco::TemporaryFile::tempName();
	File(_scratchPath).createDirectories();
}


void BundleTest::tearDown()
{
	Logger::setChannel("", _pRootChannel);
	Logger::setLevel("", _rootLevel);
	try
	{
		File(_scratchPath).remove(true);
	}
	catch (Poco::Exception&)
	{
	}
}


std::string BundleTest::codeCachePath() const
{
	return Path(_scratchPath, "codeCache").toString();
}


std::string BundleTest::createTestBundle(const std::string& symbolicName, const std::string& version, const std::string& requireBundle, const std::string& extraHeader)
{
	Path bundlePath(Path::forDirectory(_scratchPath), symbolicName + "_" + version);
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


std::string BundleTest::createActivatorBundle(const std::string& symbolicName, const std::string& activator)
{
	const std::string bundlePath(createTestBundle(symbolicName, "1.0.0", "", "Bundle-Activator: " + activator));
	File(Path(OSPBundleTestSuite::findBundle("com.appinf.osp.bundle9_1.0.0"), "bin").toString()).copyTo(bundlePath);
	return bundlePath;
}


std::string BundleTest::createTestBundleArchive(const std::string& fileName, const std::string& archive)
{
	Path bundlePath(Path::forDirectory(_scratchPath), fileName);
	Poco::FileOutputStream ostr(bundlePath.toString());
	ostr << archive;
	ostr.close();
	return bundlePath.toString();
}


void BundleTest::handleEvent(const void* /*sender*/, Poco::OSP::BundleEvent& event)
{
	_events.push_back(event);
}


void BundleTest::handleStartingEvent(const void* /*sender*/, Poco::OSP::BundleEvent& event)
{
	_events.push_back(event);
	assert (event.bundle()->state() == Bundle::BUNDLE_STARTING);
}


void BundleTest::handleStartedEvent(const void* /*sender*/, Poco::OSP::BundleEvent& event)
{
	_events.push_back(event);
	assert (event.bundle()->state() == Bundle::BUNDLE_ACTIVE);
}


void BundleTest::handleStoppingEvent(const void* /*sender*/, Poco::OSP::BundleEvent& event)
{
	_events.push_back(event);
	assert (event.bundle()->state() == Bundle::BUNDLE_STOPPING);
}


void BundleTest::handleStoppedEvent(const void* /*sender*/, Poco::OSP::BundleEvent& event)
{
	_events.push_back(event);
	assert (event.bundle()->state() == Bundle::BUNDLE_RESOLVED);
}


void BundleTest::handleStoppingEventFails(const void* /*sender*/, Poco::OSP::BundleEvent& event)
{
	if (event.bundle()->symbolicName() == "com.appinf.osp.test.failing")
		throw Poco::IllegalStateException("bundle cannot be stopped", event.bundle()->symbolicName());
}


void BundleTest::handleStartingEventThrowsUnknown(const void* /*sender*/, Poco::OSP::BundleEvent& event)
{
	if (event.bundle()->symbolicName() == "com.appinf.osp.test.failing")
		throw NotAnException();
}


void BundleTest::handleBundleError(const void* /*sender*/, const Poco::OSP::BundleLoader::BundleError& error)
{
	_bundleErrors.push_back(error.pBundle->symbolicName());
}


CppUnit::Test* BundleTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("BundleTest");

	CppUnit_addTest(pSuite, BundleTest, testLoadOne);
	CppUnit_addTest(pSuite, BundleTest, testVersionConflict);
	CppUnit_addTest(pSuite, BundleTest, testManifestPlaceholdersAreReplacedByProperties);
	CppUnit_addTest(pSuite, BundleTest, testLocalizedPropertiesExactLanguage);
	CppUnit_addTest(pSuite, BundleTest, testLocalizedPropertiesFallBackToLanguage);
	CppUnit_addTest(pSuite, BundleTest, testBundleFile);
	CppUnit_addTest(pSuite, BundleTest, testResolve);
	CppUnit_addTest(pSuite, BundleTest, testResolveCircularDependencyThrows);
	CppUnit_addTest(pSuite, BundleTest, testResolveModules);
	CppUnit_addTest(pSuite, BundleTest, testStart);
	CppUnit_addTest(pSuite, BundleTest, testActivatorStartAndStopAreCalled);
	CppUnit_addTest(pSuite, BundleTest, testStopAll);
	CppUnit_addTest(pSuite, BundleTest, testResolveStartStopUnloadAll);
	CppUnit_addTest(pSuite, BundleTest, testExtensionBundle);
	CppUnit_addTest(pSuite, BundleTest, testExtensionBundleReplacesResource);
	CppUnit_addTest(pSuite, BundleTest, testExtensionBundleAfterUnloadingUnresolvedExtension);
	CppUnit_addTest(pSuite, BundleTest, testSealedBundle);
	CppUnit_addTest(pSuite, BundleTest, testStartAfterDependencyReplaced);
	CppUnit_addTest(pSuite, BundleTest, testStartAfterDependencyReplacedSameVersion);
	CppUnit_addTest(pSuite, BundleTest, testStartAfterDependencyReplacedOutOfRange);
	CppUnit_addTest(pSuite, BundleTest, testStartAfterDependencyReplacedResolved);
	CppUnit_addTest(pSuite, BundleTest, testStartAfterDependencyReplacedUnresolvable);
	CppUnit_addTest(pSuite, BundleTest, testLibraryNameThatIsNoFileNameIsNotInstalled);
	CppUnit_addTest(pSuite, BundleTest, testCodeCacheRejectsLibraryNameThatIsNoFileName);
	CppUnit_addTest(pSuite, BundleTest, testCopiedFailedEventHasItsOwnException);
	CppUnit_addTest(pSuite, BundleTest, testStopAllAfterRequiredBundleUninstalled);
	CppUnit_addTest(pSuite, BundleTest, testResolveModuleWithoutVersion);
	CppUnit_addTest(pSuite, BundleTest, testDamagedLibraryIsNotInstalled);
	CppUnit_addTest(pSuite, BundleTest, testStoppedBundleRemovesOnlyItsOwnExtensionPoints);
	CppUnit_addTest(pSuite, BundleTest, testStartWhileRequiredBundleIsStarting);
	CppUnit_addTest(pSuite, BundleTest, testBundlesRequiringOneBundleAreResolvedConcurrently);
	CppUnit_addTest(pSuite, BundleTest, testCircularDependencyResolvedInTwoThreadsThrows);
	CppUnit_addTest(pSuite, BundleTest, testUninstallWaitsForResolution);
	CppUnit_addTest(pSuite, BundleTest, testStopAllContinuesAfterFailingBundle);
	CppUnit_addTest(pSuite, BundleTest, testStopAllContinuesAfterUnknownException);
	CppUnit_addTest(pSuite, BundleTest, testUnloadAllKeepsBundleThatFailsToStop);
	CppUnit_addTest(pSuite, BundleTest, testEventsAreFiredWithoutLoaderLock);
	CppUnit_addTest(pSuite, BundleTest, testResolveEventsOfRequiredBundlesKeepTheirOrder);
	CppUnit_addTest(pSuite, BundleTest, testResolveEventsAreFiredWhenResolutionFails);
	CppUnit_addTest(pSuite, BundleTest, testBundleResolvingDelegateRefusesRequiredAndExtendedBundle);
	CppUnit_addTest(pSuite, BundleTest, testActivatorIsResetWhenBundleIsUnloaded);
	CppUnit_addTest(pSuite, BundleTest, testActivatorIsResetWhenBundlesAreUnloadedWithoutLibraries);
	CppUnit_addTest(pSuite, BundleTest, testStartAllContinuesAfterUnknownException);
	CppUnit_addTest(pSuite, BundleTest, testStartErrorNamesBundleThatFailsInStartingEvent);
	CppUnit_addTest(pSuite, BundleTest, testStartErrorNamesBundleThatFailsWhileAnotherThreadStartsBundle);
	CppUnit_addTest(pSuite, BundleTest, testUninstallRemovesBundleUnlessPrevented);
	CppUnit_addTest(pSuite, BundleTest, testUninstallOfUnresolvedBundleWithLibrary);
	CppUnit_addTest(pSuite, BundleTest, testTemporaryDirectoryThatCannotBeRemovedIsLogged);
	CppUnit_addTest(pSuite, BundleTest, testUninstallCompletesIfBundleDirectoryCannotBeDeleted);
	CppUnit_addTest(pSuite, BundleTest, testUninstallKeepsLibraryThatAnotherBundleShips);
	CppUnit_addTest(pSuite, BundleTest, testListRequiringBundlesInStopOrder);
	CppUnit_addTest(pSuite, BundleTest, testStartAllStartsByRunLevelAndSkipsLazyStart);
	CppUnit_addTest(pSuite, BundleTest, testFindListUnloadAndContextOfBundles);
	CppUnit_addTest(pSuite, BundleTest, testActivatorStartThatThrowsLeavesBundleResolved);
	CppUnit_addTest(pSuite, BundleTest, testActivatorThatCannotBeLoadedFailsEveryStart);
	CppUnit_addTest(pSuite, BundleTest, testCodeCacheInstallsAndUninstallsLibrary);
	CppUnit_addTest(pSuite, BundleTest, testLibraryEntryOutsideCodeDirectoryIsNotInstalled);
	CppUnit_addTest(pSuite, BundleTest, testExtensionsAreHandledOnStartAndRemovedOnStop);
	CppUnit_addTest(pSuite, BundleTest, testSubsystemStartsBundlesOfConfiguredRepository);
	CppUnit_addTest(pSuite, BundleTest, testFailedSubsystemInitializationUnregistersStreamsAndServices);
	CppUnit_addTest(pSuite, BundleTest, testConcurrentStartsAndStopsOfDependencyChain);

	return pSuite;
}
