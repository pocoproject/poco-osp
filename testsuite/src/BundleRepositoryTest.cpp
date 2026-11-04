//
// BundleRepositoryTest.cpp
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#include "BundleRepositoryTest.h"
#include "OSPBundleTestSuite.h"
#include "BundleArchive.h"
#include "CallbackDelegate.h"
#include "TestLoader.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "Poco/OSP/BundleRepository.h"
#include "Poco/OSP/Bundle.h"
#include "Poco/OSP/BundleLoader.h"
#include "Poco/OSP/BundleFilter.h"
#include "Poco/OSP/Version.h"
#include "Poco/OSP/OSPException.h"
#include "Poco/OSP/BundleEvents.h"
#include "Poco/OSP/BundleInstallerService.h"
#include "Poco/Path.h"
#include "Poco/File.h"
#include "Poco/FileStream.h"
#include "Poco/StreamCopier.h"
#include "Poco/TemporaryFile.h"
#include "Poco/Logger.h"
#include "Poco/Message.h"
#include "Poco/StreamChannel.h"
#include "Poco/Delegate.h"
#include "Poco/Exception.h"
#include "Poco/Event.h"
#include "Poco/URI.h"
#include "Poco/AutoPtr.h"
#include <algorithm>
#include <atomic>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>


using Poco::OSP::BundleRepository;
using Poco::OSP::Bundle;
using Poco::OSP::BundleLoader;
using Poco::OSP::Version;
using Poco::OSP::BundleEvent;
using Poco::Path;
using Poco::File;
using Poco::Logger;


class CountingFilter: public Poco::OSP::BundleFilter
	/// Accepts all bundles or none, and counts the
	/// bundles it is asked for.
{
public:
	CountingFilter() = delete;
	explicit CountingFilter(bool acceptAll):
		_acceptAll(acceptAll)
	{
	}

	~CountingFilter() override = default;

	[[nodiscard]] bool accept(Bundle::Ptr /*pBundle*/) override
	{
		++_calls;
		return _acceptAll;
	}

	[[nodiscard]] int calls() const
	{
		return _calls;
	}

private:
	bool _acceptAll;
	int _calls = 0;
};


class AcceptOneFilter: public Poco::OSP::BundleFilter
{
public:
	[[nodiscard]] bool accept(Bundle::Ptr pBundle) override
	{
		return pBundle->symbolicName() == "com.appinf.osp.bundle5";
	}
};


const std::vector<std::string> BundleRepositoryTest::TEST_BUNDLE_NAMES({
	"com.appinf.osp.bundle1", "com.appinf.osp.bundle2", "com.appinf.osp.bundle3",
	"com.appinf.osp.bundle4", "com.appinf.osp.bundle5", "com.appinf.osp.bundle6",
	"com.appinf.osp.bundle7", "com.appinf.osp.bundle8", "com.appinf.osp.bundle9",
	"com.appinf.osp.bundleA", "com.appinf.osp.bundleB", "com.appinf.osp.bundleC",
	"com.appinf.osp.bundleD", "com.appinf.osp.bundleE", "com.appinf.osp.bundleF",
	"com.appinf.osp.bundleG", "com.appinf.osp.bundleX", "com.appinf.osp.bundleY"
});


BundleRepositoryTest::BundleRepositoryTest(const std::string& name): CppUnit::TestCase(name)
{
}


BundleRepositoryTest::~BundleRepositoryTest() = default;


void BundleRepositoryTest::testLoadFindsNewestVersionOfEachBundle()
{
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();
	BundleRepository repo(createBundleRepository(), loader);
	
	repo.loadBundles();
	
	assertTrue (loadedBundleNames(loader) == TEST_BUNDLE_NAMES);

	Bundle::Ptr pBundle1(loader.findBundle("com.appinf.osp.bundle1"));
	assertTrue (pBundle1->version() == Version(1, 0, 0));

	// bundle2 is both a directory and an archive of the same version
	Bundle::Ptr pBundle2(loader.findBundle("com.appinf.osp.bundle2"));
	assertTrue (Path(pBundle2->path()).getFileName() == "com.appinf.osp.bundle2_1.0.0.bndl");

	// an extension of another bundle each, with names of their own
	Bundle::Ptr pBundleX(loader.findBundle("com.appinf.osp.bundleX"));
	Bundle::Ptr pBundleY(loader.findBundle("com.appinf.osp.bundleY"));
	assertTrue (pBundleX->manifest().extendedBundle() == "com.appinf.osp.bundle2");
	assertTrue (pBundleY->manifest().extendedBundle() == "com.appinf.osp.bundleG");
}


void BundleRepositoryTest::testLoadKeepsNewerVersionFoundFirst()
{
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();

	// the entries are loaded in the order of their names, so version 10.0.0 is found before 2.0.0
	const std::string symbolicName("com.appinf.osp.test.newer");
	for (const char* version: {"10.0.0", "2.0.0"})
	{
		Poco::FileOutputStream ostr(Path(_developmentPath, symbolicName + "_" + version + ".bndl").toString());
		ostr << BundleArchive::create({{"META-INF/manifest.mf", BundleArchive::manifest(symbolicName, version)}});
	}

	BundleRepository repo(_developmentPath, loader);
	repo.loadBundles();

	Bundle::Ptr pBundle(loader.findBundle(symbolicName));
	assertTrue (pBundle != nullptr);
	assertTrue (pBundle->version() == Version(10, 0, 0));
}


void BundleRepositoryTest::testLoadContinuesAfterBundleThatCannotBeLoaded()
{
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();

	// Loaded before, version 2.0.0 of the first bundle makes version 1.0.0 conflict.
	const std::string conflictingPath(Path(_scratchPath, "com.appinf.osp.test.first_2.0.0.bndl").toString());
	{
		Poco::FileOutputStream ostr(conflictingPath);
		ostr << BundleArchive::create({{"META-INF/manifest.mf",
			BundleArchive::manifest("com.appinf.osp.test.first", "2.0.0")}});
	}
	loader.loadBundle(conflictingPath);
	for (const char* symbolicName: {"com.appinf.osp.test.first", "com.appinf.osp.test.second"})
	{
		Poco::FileOutputStream ostr(Path(_developmentPath, std::string(symbolicName) + "_1.0.0.bndl").toString());
		ostr << BundleArchive::create({{"META-INF/manifest.mf", BundleArchive::manifest(symbolicName, "1.0.0")}});
	}

	BundleRepository repo(_developmentPath, loader);
	repo.loadBundles();

	assertTrue (loader.findBundle("com.appinf.osp.test.first")->version() == Version(2, 0, 0));
	assertTrue (loader.findBundle("com.appinf.osp.test.second") != nullptr);
}


void BundleRepositoryTest::testLoadUnloadsBundleWhoseLoadedDelegateFails()
{
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();
	for (const char* symbolicName: {"com.appinf.osp.test.first", "com.appinf.osp.test.second"})
	{
		Poco::FileOutputStream ostr(Path(_installPath, std::string(symbolicName) + "_1.0.0.bndl").toString());
		ostr << BundleArchive::create({{"META-INF/manifest.mf", BundleArchive::manifest(symbolicName, "1.0.0")}});
	}

	// bundleLoaded is fired after the loader has added the bundle. The exception does not derive from Poco::Exception.
	CallbackDelegate loadedFails([](BundleEvent& event)
	{
		if (event.bundle()->symbolicName() == "com.appinf.osp.test.first")
			throw std::runtime_error("bundle cannot be loaded");
	});
	loader.events().bundleLoaded += loadedFails;
	BundleRepository repo(_installPath, loader);
	repo.loadBundles();
	loader.events().bundleLoaded -= loadedFails;

	assertTrue (loader.findBundle("com.appinf.osp.test.first") == nullptr);
	assertTrue (loader.findBundle("com.appinf.osp.test.second") != nullptr);
}


void BundleRepositoryTest::testLoadReportsWhetherFailedBundleIsUnloaded()
{
	// declared before the repository, which logs to it until tearDown() restores the channel
	std::ostringstream log;
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();
	for (const char* symbolicName: {"com.appinf.osp.test.first", "com.appinf.osp.test.second"})
	{
		Poco::FileOutputStream ostr(Path(_installPath, std::string(symbolicName) + "_1.0.0.bndl").toString());
		ostr << BundleArchive::create({{"META-INF/manifest.mf", BundleArchive::manifest(symbolicName, "1.0.0")}});
	}

	// The first bundle is unloaded, and a handler of the unload event fails.
	// The second bundle is started, so it cannot be unloaded.
	CallbackDelegate loadedFails([](BundleEvent& event)
	{
		Bundle::Ptr pBundle = event.bundle();
		if (pBundle->symbolicName() == "com.appinf.osp.test.first")
			throw Poco::IOException("bundle cannot be loaded", pBundle->symbolicName());
		if (pBundle->symbolicName() == "com.appinf.osp.test.second")
		{
			pBundle->resolve();
			pBundle->start();
			throw Poco::IOException("bundle cannot be loaded", pBundle->symbolicName());
		}
	});
	CallbackDelegate unloadedFails([](BundleEvent& event)
	{
		if (event.bundle()->symbolicName() == "com.appinf.osp.test.first")
			throw Poco::IllegalStateException("unload handler fails", event.bundle()->symbolicName());
	});
	loader.events().bundleLoaded += loadedFails;
	loader.events().bundleUnloaded += unloadedFails;
	Logger::get("osp.core.BundleRepository").setChannel(Poco::makeAuto<Poco::StreamChannel>(log));
	BundleRepository repo(_installPath, loader);
	repo.loadBundles();
	loader.events().bundleLoaded -= loadedFails;
	loader.events().bundleUnloaded -= unloadedFails;

	assertTrue (loader.findBundle("com.appinf.osp.test.first") == nullptr);
	assertTrue (log.str().find("Bundle com.appinf.osp.test.first, whose loading has failed, is unloaded, ")
		!= std::string::npos);
	Bundle::Ptr pSecond = loader.findBundle("com.appinf.osp.test.second");
	assertTrue (pSecond != nullptr);
	assertTrue (pSecond->isActive());
	assertTrue (log.str().find("Failed to unload bundle com.appinf.osp.test.second, whose loading has failed; "
		"it stays loaded: ") != std::string::npos);
}


void BundleRepositoryTest::testLoadChoosesBundleOfSameVersionByName()
{
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();

	// enough pairs that the order of a directory listing that is not
	// sorted by name differs for at least one of them
	constexpr int BUNDLE_PAIRS = 16;
	for (int i = 0; i < BUNDLE_PAIRS; ++i)
	{
		const std::string symbolicName("com.appinf.osp.test.same" + std::to_string(i));
		const std::string manifest(BundleArchive::manifest(symbolicName, "1.0.0"));
		Path metaPath(Path::forDirectory(_developmentPath));
		metaPath.pushDirectory(symbolicName + "_1.0.0");
		metaPath.pushDirectory("META-INF");
		File(metaPath).createDirectories();
		{
			Poco::FileOutputStream ostr(Path(metaPath, "manifest.mf").toString());
			ostr << manifest;
		}
		{
			Poco::FileOutputStream ostr(Path(_developmentPath, symbolicName + "_1.0.0.bndl").toString());
			ostr << BundleArchive::create({{"META-INF/manifest.mf", manifest}});
		}
	}

	BundleRepository repo(_developmentPath, loader);
	repo.loadBundles();

	for (int i = 0; i < BUNDLE_PAIRS; ++i)
	{
		const std::string symbolicName("com.appinf.osp.test.same" + std::to_string(i));
		Bundle::Ptr pBundle(loader.findBundle(symbolicName));
		assertTrue (pBundle != nullptr);
		assertEqual (symbolicName + "_1.0.0.bndl", Path(pBundle->path()).getFileName());
	}
}


void BundleRepositoryTest::testInstallWritesArchiveIntoFirstPathOnce()
{
	// The following is a ZIP file containing the
	// manifest of bundle1.
	static const unsigned char TEST_BUNDLE_ZIP[] = 
	{
		0x50, 0x4b, 0x03, 0x04, 0x14, 0x00, 0x00, 0x00, 0x08, 0x00, 0x36, 0x46, 0x85, 0x36, 0x2a, 0x71,
		0x30, 0xee, 0x9d, 0x00, 0x00, 0x00, 0xec, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x4d, 0x45,
		0x54, 0x41, 0x2d, 0x49, 0x4e, 0x46, 0x2f, 0x6d, 0x61, 0x6e, 0x69, 0x66, 0x65, 0x73, 0x74, 0x2e,
		0x6d, 0x66, 0x6d, 0xcd, 0x3d, 0x0b, 0xc2, 0x30, 0x14, 0x85, 0xe1, 0x5d, 0xe8, 0x7f, 0xb8, 0xa3,
		0x82, 0x2d, 0xad, 0x8b, 0x90, 0x4d, 0x45, 0xd4, 0xc1, 0x0f, 0x08, 0x74, 0x4f, 0xd3, 0xdb, 0x7a,
		0x21, 0xb9, 0x09, 0x49, 0x44, 0xfa, 0xef, 0x05, 0x0b, 0xb5, 0x83, 0xeb, 0xe1, 0x39, 0xbc, 0x57,
		0xc5, 0xd4, 0x61, 0x4c, 0x79, 0x8d, 0x21, 0x92, 0x63, 0x01, 0x55, 0x51, 0x66, 0x8b, 0xfd, 0x8b,
		0x5b, 0x83, 0xf9, 0x4d, 0x59, 0x14, 0x70, 0x97, 0x0f, 0x90, 0xca, 0x7a, 0x83, 0x30, 0xee, 0x50,
		0x4d, 0x42, 0x0e, 0xb6, 0x71, 0x86, 0xf4, 0x28, 0xb5, 0xb3, 0x85, 0xf2, 0x9e, 0xb8, 0x2b, 0x5c,
		0xf4, 0x45, 0xf3, 0x35, 0x3f, 0x3c, 0x6f, 0xcc, 0x2a, 0x35, 0x72, 0xeb, 0x82, 0x80, 0x9d, 0xf7,
		0x86, 0xb0, 0x85, 0x0b, 0x77, 0x2e, 0x58, 0x95, 0x48, 0xc7, 0xc9, 0x1c, 0x9c, 0x1f, 0x02, 0xf5,
		0xcf, 0x24, 0x60, 0xa9, 0x57, 0xb0, 0x29, 0xcb, 0xed, 0xfa, 0xdf, 0x01, 0xa4, 0xeb, 0xd2, 0x5b,
		0x05, 0x84, 0x23, 0xf7, 0xc4, 0x88, 0x81, 0xb8, 0x87, 0x93, 0x6d, 0xce, 0xd9, 0xe2, 0x03, 0x50,
		0x4b, 0x03, 0x04, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf7, 0x45, 0x85, 0x36, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x4d, 0x45, 0x54,
		0x41, 0x2d, 0x49, 0x4e, 0x46, 0x2f, 0x50, 0x4b, 0x01, 0x02, 0x14, 0x00, 0x14, 0x00, 0x00, 0x00,
		0x08, 0x00, 0x36, 0x46, 0x85, 0x36, 0x2a, 0x71, 0x30, 0xee, 0x9d, 0x00, 0x00, 0x00, 0xec, 0x00,
		0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x21, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x4d, 0x45, 0x54, 0x41, 0x2d, 0x49, 0x4e, 0x46, 0x2f, 0x6d, 0x61, 0x6e,
		0x69, 0x66, 0x65, 0x73, 0x74, 0x2e, 0x6d, 0x66, 0x50, 0x4b, 0x01, 0x02, 0x14, 0x00, 0x0a, 0x00,
		0x00, 0x00, 0x00, 0x00, 0xf7, 0x45, 0x85, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00,
		0x00, 0x00, 0xcf, 0x00, 0x00, 0x00, 0x4d, 0x45, 0x54, 0x41, 0x2d, 0x49, 0x4e, 0x46, 0x2f, 0x50,
		0x4b, 0x05, 0x06, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x02, 0x00, 0x79, 0x00, 0x00, 0x00, 0xf6,
		0x00, 0x00, 0x00, 0x00, 0x00
	};

	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();
	BundleRepository repo(_installPath + Path::pathSeparator() + createBundleRepository(), loader);

	std::string bundleData(reinterpret_cast<const char*>(&TEST_BUNDLE_ZIP[0]), sizeof(TEST_BUNDLE_ZIP));
	std::istringstream istr(bundleData);
	
	Bundle::Ptr pBundle(repo.installBundle(istr));
	
	assert (!pBundle.isNull());
	assert (pBundle->state() == Bundle::BUNDLE_INSTALLED);
	assertTrue (pBundle->symbolicName() == "com.appinf.osp.bundle1");
	const std::vector<std::string> installedFiles({"com.appinf.osp.bundle1_1.0.0.bndl"});
	std::vector<std::string> files;
	File(_installPath).list(files);
	assertTrue (files == installedFiles);

	std::istringstream istr2(bundleData);
	try
	{
		repo.installBundle(istr2);
		fail("bundle already installed - must throw");
	}
	catch (Poco::OSP::BundleInstallException&)
	{
	}
	File(_installPath).list(files);
	assertTrue (files == installedFiles);
}


void BundleRepositoryTest::testFilterThatAcceptsAllLoadsEveryBundle()
{
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();
	Poco::AutoPtr<CountingFilter> pFilter(new CountingFilter(true));
	BundleRepository repo(createBundleRepository(), loader, pFilter);
	
	repo.loadBundles();
	
	assertEqual (TEST_BUNDLE_COUNT, pFilter->calls());
	assertTrue (loadedBundleNames(loader) == TEST_BUNDLE_NAMES);
}


void BundleRepositoryTest::testFilterThatRejectsAllLoadsNoBundle()
{
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();
	Poco::AutoPtr<CountingFilter> pFilter(new CountingFilter(false));
	BundleRepository repo(createBundleRepository(), loader, pFilter);
	
	repo.loadBundles();
	
	assertEqual (TEST_BUNDLE_COUNT, pFilter->calls());
	assertTrue (loadedBundleNames(loader).empty());
}


void BundleRepositoryTest::testFilterThatAcceptsOneLoadsOnlyThatBundle()
{
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();
	BundleRepository repo(createBundleRepository(), loader, new AcceptOneFilter);
	
	repo.loadBundles();
	
	assertTrue (loadedBundleNames(loader) == std::vector<std::string>({"com.appinf.osp.bundle5"}));
}


void BundleRepositoryTest::testInstallRejectsSymbolicNameThatIsNoFileName()
{
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();
	BundleRepository repo(_installPath, loader);

	File outside(Path(_scratchPath, "outside_1.0.0.bndl"));
	std::istringstream istr(BundleArchive::create({{"META-INF/manifest.mf", BundleArchive::manifest("../outside", "1.0.0")}}));
	bool rejected = false;
	try
	{
		repo.installBundle(istr);
	}
	catch (Poco::OSP::ManifestException&)
	{
		rejected = true;
	}
	const bool writtenOutside = outside.exists();
	if (writtenOutside) outside.remove();

	assertTrue (rejected);
	assertFalse (writtenOutside);
	std::vector<std::string> files;
	File(_installPath).list(files);
	assertTrue (files.empty());
}


void BundleRepositoryTest::testInstallRejectsBundleThatFilterRejects()
{
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();
	BundleRepository repo(_installPath, loader, new CountingFilter(false));

	std::istringstream istr(BundleArchive::create({{"META-INF/manifest.mf", BundleArchive::manifest("com.appinf.osp.test.filtered", "1.0.0")}}));
	try
	{
		repo.installBundle(istr);
		fail("bundle rejected by the filter must not be installed");
	}
	catch (Poco::OSP::BundleInstallException&)
	{
	}

	assertTrue (loader.findBundle("com.appinf.osp.test.filtered").isNull());
	std::vector<std::string> files;
	File(_installPath).list(files);
	assertTrue (files.empty());
}


void BundleRepositoryTest::testTemporaryInstallFileIsNotLoaded()
{
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();
	BundleRepository repo(_installPath, loader);

	// left over by an install that did not complete
	{
		Poco::FileOutputStream ostr(Path(_installPath, "tmp.bundle-7." + BundleRepository::TEMP_BUNDLE_EXTENSION).toString());
		ostr << BundleArchive::create({{"META-INF/manifest.mf", BundleArchive::manifest("com.appinf.osp.test.partial", "1.0.0")}});
	}
	repo.loadBundles();

	assertTrue (loader.findBundle("com.appinf.osp.test.partial").isNull());
}


void BundleRepositoryTest::testConcurrentInstallsOfSameBundle()
{
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();
	BundleRepository repo(_installPath, loader);

	const std::string archive(BundleArchive::create({{"META-INF/manifest.mf", BundleArchive::manifest("com.appinf.osp.test.twice", "1.0.0")}}));
	std::atomic<int> installed(0);
	std::atomic<int> rejected(0);
	std::atomic<int> failed(0);
	auto install = [&]()
	{
		std::istringstream istr(archive);
		try
		{
			repo.installBundle(istr);
			++installed;
		}
		catch (Poco::OSP::BundleInstallException&)
		{
			++rejected;
		}
		catch (Poco::Exception&)
		{
			++failed;
		}
	};
	std::thread first(install);
	std::thread second(install);
	first.join();
	second.join();

	assertEqual (1, installed.load());
	assertEqual (1, rejected.load());
	assertEqual (0, failed.load());
	std::vector<std::string> files;
	File(_installPath).list(files);
	assertTrue (files == std::vector<std::string>({"com.appinf.osp.test.twice_1.0.0.bndl"}));
}


void BundleRepositoryTest::testInstallFiresBundleInstalled()
{
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();
	BundleRepository repo(_installPath, loader);

	_installedBundles.clear();
	loader.events().bundleInstalled += Poco::delegate(this, &BundleRepositoryTest::handleInstalled);
	std::istringstream istr(BundleArchive::create({{"META-INF/manifest.mf", BundleArchive::manifest("com.appinf.osp.test.installed", "1.0.0")}}));
	repo.installBundle(istr);
	loader.events().bundleInstalled -= Poco::delegate(this, &BundleRepositoryTest::handleInstalled);

	assertTrue (_installedBundles == std::vector<std::string>({"com.appinf.osp.test.installed"}));
}


void BundleRepositoryTest::testUninstallKeepsBundleOutsideInstallRepository()
{
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();

	const File installedFile(Path(_installPath, "com.appinf.osp.test.installed_1.0.0.bndl"));
	const File developmentFile(Path(_developmentPath, "com.appinf.osp.test.development_1.0.0.bndl"));
	{
		Poco::FileOutputStream ostr(installedFile.path());
		ostr << BundleArchive::create({{"META-INF/manifest.mf", BundleArchive::manifest("com.appinf.osp.test.installed", "1.0.0")}});
	}
	{
		Poco::FileOutputStream ostr(developmentFile.path());
		ostr << BundleArchive::create({{"META-INF/manifest.mf", BundleArchive::manifest("com.appinf.osp.test.development", "1.0.0")}});
	}
	BundleRepository repo(_installPath + Path::pathSeparator() + _developmentPath, loader);
	repo.loadBundles();

	Bundle::Ptr pInstalled = loader.findBundle("com.appinf.osp.test.installed");
	Bundle::Ptr pDevelopment = loader.findBundle("com.appinf.osp.test.development");
	assertTrue (!pInstalled.isNull());
	assertTrue (!pDevelopment.isNull());
	pInstalled->uninstall();
	pDevelopment->uninstall();

	assertFalse (installedFile.exists());
	assertTrue (developmentFile.exists());
}


void BundleRepositoryTest::testUninstallKeepsBundleInstalledIntoSecondaryPath()
{
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();
	BundleRepository repo(_installPath + Path::pathSeparator() + _developmentPath, loader);

	std::istringstream istr(BundleArchive::create({{"META-INF/manifest.mf", BundleArchive::manifest("com.appinf.osp.test.secondary", "1.0.0")}}));
	Bundle::Ptr pBundle = repo.installBundle(_developmentPath, istr);
	const File bundleFile(pBundle->path());
	pBundle->uninstall();

	assertTrue (bundleFile.exists());
}


void BundleRepositoryTest::testInstallKeepsLeftoverTemporaryFile()
{
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();
	BundleRepository repo(_installPath, loader);

	// left over by an earlier process, under the first temporary name
	const std::string leftoverPath(Path(_installPath, "tmp.bundle-0." + BundleRepository::TEMP_BUNDLE_EXTENSION).toString());
	{
		Poco::FileOutputStream ostr(leftoverPath);
		ostr << "left over";
	}
	std::istringstream istr(BundleArchive::create({{"META-INF/manifest.mf", BundleArchive::manifest("com.appinf.osp.test.installed", "1.0.0")}}));
	repo.installBundle(istr);

	std::string content;
	Poco::FileInputStream leftover(leftoverPath);
	Poco::StreamCopier::copyToString(leftover, content);
	assertEqual ("left over", content);
}


void BundleRepositoryTest::testInstallKeepsLeftoverTemporaryDirectory()
{
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();
	BundleRepository repo(_installPath, loader);

	// a backup of a bundle directory, left over by an earlier process under the first temporary name
	File leftover(Path(_installPath, "tmp.bundle-0." + BundleRepository::TEMP_BUNDLE_EXTENSION));
	leftover.createDirectories();
	std::istringstream istr(BundleArchive::create({{"META-INF/manifest.mf", BundleArchive::manifest("com.appinf.osp.test.installed", "1.0.0")}}));
	Bundle::Ptr pBundle = repo.installBundle(istr);

	assertTrue (pBundle->state() == Bundle::BUNDLE_INSTALLED);
	assertTrue (leftover.isDirectory());
}


void BundleRepositoryTest::testInstallKeepsFileOfSameNameThatIsNoLoadedBundle()
{
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();
	BundleRepository repo(_installPath, loader);

	// not loaded, for example because the bundle filter rejected it
	const std::string takenPath(Path(_installPath, "com.appinf.osp.test.taken_1.0.0.bndl").toString());
	{
		Poco::FileOutputStream ostr(takenPath);
		ostr << "taken";
	}
	std::istringstream istr(BundleArchive::create({{"META-INF/manifest.mf", BundleArchive::manifest("com.appinf.osp.test.taken", "1.0.0")}}));
	try
	{
		repo.installBundle(istr);
		fail("the target name is taken - must throw");
	}
	catch (Poco::OSP::BundleInstallException&)
	{
	}

	std::string content;
	Poco::FileInputStream taken(takenPath);
	Poco::StreamCopier::copyToString(taken, content);
	assertEqual ("taken", content);
	std::vector<std::string> files;
	File(_installPath).list(files);
	assertTrue (files == std::vector<std::string>({"com.appinf.osp.test.taken_1.0.0.bndl"}));
}


void BundleRepositoryTest::testReplacementWithSameVersion()
{
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();
	BundleRepository repo(_installPath, loader);

	const std::string manifest(BundleArchive::manifest("com.appinf.osp.test.replaced", "1.0.0"));
	std::istringstream oldArchive(BundleArchive::create({{"META-INF/manifest.mf", manifest}}));
	repo.installBundle(oldArchive);
	std::istringstream newArchive(BundleArchive::create({{"META-INF/manifest.mf", manifest + "Bundle-Vendor: New\n"}}));
	Bundle::Ptr pNew = repo.installBundle(newArchive, "com.appinf.osp.test.replaced");

	assertTrue (pNew->vendor() == "New");
	assertTrue (loader.findBundle("com.appinf.osp.test.replaced") == pNew);
	std::vector<std::string> files;
	File(_installPath).list(files);
	assertTrue (files == std::vector<std::string>({"com.appinf.osp.test.replaced_1.0.0.bndl"}));
}


void BundleRepositoryTest::testFailedReplacementKeepsReplacedBundle()
{
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();
	BundleRepository repo(_installPath, loader);

	std::istringstream oldArchive(BundleArchive::create({{"META-INF/manifest.mf", BundleArchive::manifest("com.appinf.osp.test.replaced", "1.0.0")}}));
	repo.installBundle(oldArchive);

	loader.events().bundleLoaded += Poco::delegate(this, &BundleRepositoryTest::handleLoadedFailsForVersion2);
	std::istringstream newArchive(BundleArchive::create({{"META-INF/manifest.mf", BundleArchive::manifest("com.appinf.osp.test.replaced", "2.0.0")}}));
	try
	{
		repo.installBundle(newArchive, "com.appinf.osp.test.replaced");
		fail("the new bundle cannot be loaded - must throw");
	}
	catch (Poco::IOException&)
	{
	}
	loader.events().bundleLoaded -= Poco::delegate(this, &BundleRepositoryTest::handleLoadedFailsForVersion2);

	Bundle::Ptr pBundle = loader.findBundle("com.appinf.osp.test.replaced");
	assertTrue (!pBundle.isNull());
	assertTrue (pBundle->version() == Version(1, 0, 0));
	assertTrue (pBundle->state() == Bundle::BUNDLE_INSTALLED);
	std::vector<std::string> files;
	File(_installPath).list(files);
	assertTrue (files == std::vector<std::string>({"com.appinf.osp.test.replaced_1.0.0.bndl"}));

	pBundle->resolve();
	assertTrue (pBundle->isResolved());
}


void BundleRepositoryTest::testFailedReplacementIntoSecondaryPathKeepsReplacedBundle()
{
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();
	BundleRepository repo(_installPath + Path::pathSeparator() + _developmentPath, loader);

	std::istringstream oldArchive(BundleArchive::create({{"META-INF/manifest.mf", BundleArchive::manifest("com.appinf.osp.test.replaced", "1.0.0")}}));
	repo.installBundle(oldArchive);

	loader.events().bundleLoaded += Poco::delegate(this, &BundleRepositoryTest::handleLoadedFailsForVersion2);
	std::istringstream newArchive(BundleArchive::create({{"META-INF/manifest.mf", BundleArchive::manifest("com.appinf.osp.test.replaced", "2.0.0")}}));
	try
	{
		repo.installBundle(_developmentPath, newArchive, "com.appinf.osp.test.replaced");
		fail("the new bundle cannot be loaded - must throw");
	}
	catch (Poco::IOException&)
	{
	}
	loader.events().bundleLoaded -= Poco::delegate(this, &BundleRepositoryTest::handleLoadedFailsForVersion2);

	Bundle::Ptr pBundle = loader.findBundle("com.appinf.osp.test.replaced");
	assertTrue (!pBundle.isNull());
	assertTrue (pBundle->version() == Version(1, 0, 0));
	std::vector<std::string> files;
	File(_installPath).list(files);
	assertTrue (files == std::vector<std::string>({"com.appinf.osp.test.replaced_1.0.0.bndl"}));
}


void BundleRepositoryTest::testFailedReplacementStartsRequiringBundleAgain()
{
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();
	BundleRepository repo(_installPath, loader);

	std::istringstream oldArchive(BundleArchive::create({{"META-INF/manifest.mf", BundleArchive::manifest("com.appinf.osp.test.replaced", "1.0.0")}}));
	repo.installBundle(oldArchive);
	std::istringstream userArchive(BundleArchive::create({{"META-INF/manifest.mf",
		BundleArchive::manifest("com.appinf.osp.test.user", "1.0.0") + "Require-Bundle: com.appinf.osp.test.replaced\n"}}));
	Bundle::Ptr pUser = repo.installBundle(userArchive);
	pUser->resolve();
	pUser->start();

	// The exception does not derive from Poco::Exception.
	loader.events().bundleLoaded += Poco::delegate(this, &BundleRepositoryTest::handleLoadedFailsWithStdExceptionForVersion2);
	std::istringstream newArchive(BundleArchive::create({{"META-INF/manifest.mf", BundleArchive::manifest("com.appinf.osp.test.replaced", "2.0.0")}}));
	try
	{
		repo.installBundle(newArchive, "com.appinf.osp.test.replaced");
		fail("the new bundle cannot be loaded - must throw");
	}
	catch (std::runtime_error&)
	{
	}
	loader.events().bundleLoaded -= Poco::delegate(this, &BundleRepositoryTest::handleLoadedFailsWithStdExceptionForVersion2);

	Bundle::Ptr pBundle = loader.findBundle("com.appinf.osp.test.replaced");
	assertTrue (!pBundle.isNull());
	assertTrue (pBundle->version() == Version(1, 0, 0));
	assertTrue (pBundle->isActive());
	assertTrue (pUser->isActive());
	std::vector<std::string> files;
	File(_installPath).list(files);
	std::sort(files.begin(), files.end());
	assertTrue (files == std::vector<std::string>({"com.appinf.osp.test.replaced_1.0.0.bndl", "com.appinf.osp.test.user_1.0.0.bndl"}));

	loader.unloadAllBundles();
}


void BundleRepositoryTest::testActivatorInstallsBundlesDuringReplacement()
{
	// Shared with the threads and the delegates; threads that block each other
	// are left running.
	struct Shared
	{
		Shared(const std::string& codeCachePath, const std::string& installPath):
			testLoader(codeCachePath),
			repository(installPath, testLoader.loader())
		{
		}

		const std::string replacedName{"com.appinf.osp.test.replaced"};
		TestLoader testLoader;
		BundleRepository repository;
		Poco::Event newBundleInstalled;
		Poco::Event newBundleStarting;
		std::atomic<bool> secondReplacementRejected{false};
		std::atomic<bool> otherInstalled{false};
		std::string replacingError;
		Poco::Event replacingDone;
		std::string startingError;
		Poco::Event startingDone;
	};
	auto pOwnedShared = std::make_unique<Shared>(codeCachePath(), _installPath);
	Shared* pShared = pOwnedShared.get();
	BundleLoader& loader = pShared->testLoader.loader();

	std::istringstream oldArchive(BundleArchive::create({{"META-INF/manifest.mf",
		BundleArchive::manifest(pShared->replacedName, "1.0.0")}}));
	pShared->repository.installBundle(oldArchive);
	std::istringstream userArchive(BundleArchive::create({{"META-INF/manifest.mf",
		BundleArchive::manifest("com.appinf.osp.test.user", "1.0.0")
			+ "Require-Bundle: " + pShared->replacedName + "\n"}}));
	Bundle::Ptr pUser = pShared->repository.installBundle(userArchive);
	pUser->resolve();
	pUser->start();

	// The replacing thread waits in bundleInstalled until the starting thread
	// starts the new bundle; then it starts the user, which requires the new
	// bundle, while the starting thread installs bundles from bundleStarting,
	// as an activator of the new bundle would.
	CallbackDelegate installedDelegate([pShared](BundleEvent& event)
	{
		if (event.bundle()->symbolicName() == pShared->replacedName && event.bundle()->version() == Version(2, 0, 0))
		{
			pShared->newBundleInstalled.set();
			if (!pShared->newBundleStarting.tryWait(WAIT_MS))
				throw Poco::TimeoutException("the new bundle is not started");
		}
	});
	CallbackDelegate startingDelegate([pShared](BundleEvent& event)
	{
		if (event.bundle()->symbolicName() == pShared->replacedName && event.bundle()->version() == Version(2, 0, 0))
		{
			pShared->newBundleStarting.set();
			std::istringstream secondArchive(BundleArchive::create({{"META-INF/manifest.mf",
				BundleArchive::manifest(pShared->replacedName, "3.0.0")}}));
			try
			{
				pShared->repository.installBundle(secondArchive, pShared->replacedName);
			}
			catch (Poco::OSP::BundleInstallException&)
			{
				pShared->secondReplacementRejected = true;
			}
			std::istringstream otherArchive(BundleArchive::create({{"META-INF/manifest.mf", BundleArchive::manifest("com.appinf.osp.test.other", "1.0.0")}}));
			pShared->repository.installBundle(otherArchive);
			pShared->otherInstalled = true;
		}
	});
	loader.events().bundleInstalled += installedDelegate;
	loader.events().bundleStarting += startingDelegate;

	std::thread replacing([pShared]()
	{
		try
		{
			std::istringstream newArchive(BundleArchive::create({{"META-INF/manifest.mf",
				BundleArchive::manifest(pShared->replacedName, "2.0.0")}}));
			pShared->repository.installBundle(newArchive, pShared->replacedName);
		}
		catch (Poco::Exception& exc)
		{
			pShared->replacingError = exc.displayText();
		}
		pShared->replacingDone.set();
	});
	std::thread starting([pShared]()
	{
		try
		{
			if (!pShared->newBundleInstalled.tryWait(WAIT_MS))
				throw Poco::TimeoutException("the new bundle is not installed");
			Bundle::Ptr pNew = pShared->testLoader.loader().findBundle(pShared->replacedName);
			pNew->resolve();
			pNew->start();
		}
		catch (Poco::Exception& exc)
		{
			pShared->startingError = exc.displayText();
		}
		pShared->startingDone.set();
	});

	const bool replacingFinished = pShared->replacingDone.tryWait(WAIT_MS);
	const bool startingFinished = pShared->startingDone.tryWait(WAIT_MS);
	if (!replacingFinished || !startingFinished)
	{
		replacing.detach();
		starting.detach();
		static std::vector<const void*> blockedObjects;
		blockedObjects.push_back(pOwnedShared.release());
		fail("the replacement and the installs from bundleStarting block each other");
	}
	replacing.join();
	starting.join();
	loader.events().bundleInstalled -= installedDelegate;
	loader.events().bundleStarting -= startingDelegate;

	assertEqual ("", pShared->replacingError);
	assertEqual ("", pShared->startingError);
	assertTrue (pShared->secondReplacementRejected);
	assertTrue (pShared->otherInstalled);
	Bundle::Ptr pNew = loader.findBundle(pShared->replacedName);
	assertTrue (pNew->version() == Version(2, 0, 0));
	assertTrue (pNew->isActive());
	assertTrue (pUser->isActive());

	loader.unloadAllBundles();
}


void BundleRepositoryTest::testReplacementDoesNotReportReplacedFileAsKept()
{
	// declared before the loader, which logs to it until tearDown() restores the channel
	std::ostringstream log;
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();
	BundleRepository repo(_installPath + Path::pathSeparator() + _developmentPath, loader);

	// in a secondary path, where an uninstall keeps the file and says so
	std::istringstream oldArchive(BundleArchive::create({{"META-INF/manifest.mf", BundleArchive::manifest("com.appinf.osp.test.replaced", "1.0.0")}}));
	repo.installBundle(_developmentPath, oldArchive);

	Logger& loaderLogger = Logger::get("osp.core.BundleLoader");
	loaderLogger.setChannel(new Poco::StreamChannel(log));
	loaderLogger.setLevel(Poco::Message::PRIO_INFORMATION);
	std::istringstream newArchive(BundleArchive::create({{"META-INF/manifest.mf", BundleArchive::manifest("com.appinf.osp.test.replaced", "2.0.0")}}));
	repo.installBundle(_developmentPath, newArchive, "com.appinf.osp.test.replaced");

	// The information messages of the uninstall are logged, and none says the file is kept.
	assertTrue (log.str().find("uninstalled.") != std::string::npos);
	assertTrue (log.str().find("is kept") == std::string::npos);
	std::vector<std::string> files;
	File(_developmentPath).list(files);
	assertTrue (files == std::vector<std::string>({"com.appinf.osp.test.replaced_2.0.0.bndl"}));
}


void BundleRepositoryTest::testReplacementStartsRequiringBundleAgain()
{
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();
	BundleRepository repo(_installPath, loader);

	std::istringstream oldArchive(BundleArchive::create({{"META-INF/manifest.mf", BundleArchive::manifest("com.appinf.osp.test.replaced", "1.0.0")}}));
	repo.installBundle(oldArchive);
	std::istringstream userArchive(BundleArchive::create({{"META-INF/manifest.mf",
		BundleArchive::manifest("com.appinf.osp.test.user", "1.0.0") + "Require-Bundle: com.appinf.osp.test.replaced\n"}}));
	Bundle::Ptr pUser = repo.installBundle(userArchive);
	pUser->resolve();
	pUser->start();

	std::istringstream newArchive(BundleArchive::create({{"META-INF/manifest.mf", BundleArchive::manifest("com.appinf.osp.test.replaced", "2.0.0")}}));
	Bundle::Ptr pNew = repo.installBundle(newArchive, "com.appinf.osp.test.replaced");

	assertTrue (pUser->isActive());
	assertTrue (pNew->isActive());
	const Bundle::ResolvedDependencies deps = pUser->resolvedDependencies();
	assertEqual (1, static_cast<int>(deps.size()));
	assertEqual (pNew->id(), deps[0].id);

	loader.unloadAllBundles();
}


void BundleRepositoryTest::testReplacementOfUnresolvedBundleWithLibrary()
{
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();
	BundleRepository repo(_installPath, loader);

	// The required bundle is missing: the bundle is never resolved, and its
	// library never reaches the code cache.
	const std::string libraryPath("bin/" + loader.osName() + "/" + loader.osArchitecture() + "/libunresolved");
	std::istringstream oldArchive(BundleArchive::create({
		{"META-INF/manifest.mf", BundleArchive::manifest("com.appinf.osp.test.unresolved", "1.0.0")
			+ "Require-Bundle: com.appinf.osp.test.missing\n"},
		{libraryPath, "library"}}));
	Bundle::Ptr pOld = repo.installBundle(oldArchive);
	loader.resolveAllBundles();
	assertTrue (pOld->state() == Bundle::BUNDLE_INSTALLED);

	std::istringstream newArchive(BundleArchive::create({{"META-INF/manifest.mf",
		BundleArchive::manifest("com.appinf.osp.test.unresolved", "2.0.0")}}));
	Bundle::Ptr pNew = repo.installBundle(newArchive, "com.appinf.osp.test.unresolved");

	assertTrue (pOld->state() == Bundle::BUNDLE_UNINSTALLED);
	assertTrue (loader.findBundle("com.appinf.osp.test.unresolved") == pNew);
	std::vector<std::string> files;
	File(_installPath).list(files);
	assertTrue (files == std::vector<std::string>({"com.appinf.osp.test.unresolved_2.0.0.bndl"}));
}


void BundleRepositoryTest::testFailedUninstallOfReplacedBundleReportsItsError()
{
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();
	BundleRepository repo(_installPath, loader);

	std::istringstream oldArchive(BundleArchive::create({{"META-INF/manifest.mf",
		BundleArchive::manifest("com.appinf.osp.test.replaced", "1.0.0")}}));
	repo.installBundle(oldArchive);

	// bundleUnloaded is fired after the loader has removed the replaced bundle.
	CallbackDelegate unloadedFails([](BundleEvent& event)
	{
		if (event.bundle()->version() == Version(1, 0, 0))
			throw Poco::IOException("bundle cannot be unloaded", event.bundle()->symbolicName());
	});
	loader.events().bundleUnloaded += unloadedFails;
	std::istringstream newArchive(BundleArchive::create({{"META-INF/manifest.mf",
		BundleArchive::manifest("com.appinf.osp.test.replaced", "2.0.0")}}));
	try
	{
		repo.installBundle(newArchive, "com.appinf.osp.test.replaced");
		fail("the replaced bundle cannot be unloaded - must throw");
	}
	catch (Poco::IOException&)
	{
	}
	loader.events().bundleUnloaded -= unloadedFails;
}


void BundleRepositoryTest::testFailedInstallReportsBundleAsUnloaded()
{
	// declared before the repository, which logs to it until tearDown() restores the channel
	std::ostringstream log;
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();
	Logger::get("osp.core.BundleRepository").setChannel(Poco::makeAuto<Poco::StreamChannel>(log));
	BundleRepository repo(_installPath, loader);

	// The bundle is unloaded, and a handler of the unload event fails.
	CallbackDelegate loadedFails([](BundleEvent& event)
	{
		if (event.bundle()->symbolicName() == "com.appinf.osp.test.installed")
			throw Poco::IOException("bundle cannot be loaded", event.bundle()->symbolicName());
	});
	CallbackDelegate unloadedFails([](BundleEvent& event)
	{
		if (event.bundle()->symbolicName() == "com.appinf.osp.test.installed")
			throw Poco::IllegalStateException("unload handler fails", event.bundle()->symbolicName());
	});
	loader.events().bundleLoaded += loadedFails;
	loader.events().bundleUnloaded += unloadedFails;
	std::istringstream archive(BundleArchive::create({{"META-INF/manifest.mf",
		BundleArchive::manifest("com.appinf.osp.test.installed", "1.0.0")}}));
	try
	{
		repo.installBundle(archive);
		fail("the bundle cannot be loaded - must throw");
	}
	catch (Poco::IOException&)
	{
	}
	loader.events().bundleLoaded -= loadedFails;
	loader.events().bundleUnloaded -= unloadedFails;

	assertTrue (loader.findBundle("com.appinf.osp.test.installed") == nullptr);
	assertTrue (log.str().find("Bundle com.appinf.osp.test.installed, whose installation has failed, is unloaded, ")
		!= std::string::npos);
}


void BundleRepositoryTest::testInstallOfCorruptArchiveLeavesNoFile()
{
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();
	BundleRepository repo(_installPath, loader);

	std::istringstream istr("this is not a ZIP archive");
	try
	{
		repo.installBundle(istr);
		fail("corrupt archive - must throw");
	}
	catch (Poco::OSP::BundleLoadException&)
	{
	}
	std::vector<std::string> files;
	File(_installPath).list(files);
	assertTrue (files.empty());
}


void BundleRepositoryTest::testMissingRepositoryPathIsSkipped()
{
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();
	{
		Poco::FileOutputStream ostr(Path(_installPath, "com.appinf.osp.test.present_1.0.0.bndl").toString());
		ostr << BundleArchive::create({{"META-INF/manifest.mf", BundleArchive::manifest("com.appinf.osp.test.present", "1.0.0")}});
	}
	BundleRepository repo(Path(_scratchPath, "missing").toString() + Path::pathSeparator() + _installPath, loader);
	repo.loadBundles();

	assertTrue (loadedBundleNames(loader) == std::vector<std::string>({"com.appinf.osp.test.present"}));
}


void BundleRepositoryTest::testRepositoryPathWithGlobPattern()
{
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();
	for (const std::string name: {"a", "b"})
	{
		const std::string repoPath(Path(_scratchPath, "repo-" + name).toString());
		File(repoPath).createDirectories();
		Poco::FileOutputStream ostr(Path(repoPath, "com.appinf.osp.test." + name + "_1.0.0.bndl").toString());
		ostr << BundleArchive::create({{"META-INF/manifest.mf", BundleArchive::manifest("com.appinf.osp.test." + name, "1.0.0")}});
	}
	BundleRepository repo(Path(_scratchPath, "repo-*").toString(), loader);
	repo.loadBundles();

	assertTrue (loadedBundleNames(loader) == std::vector<std::string>({"com.appinf.osp.test.a", "com.appinf.osp.test.b"}));
}


void BundleRepositoryTest::testInstallerServiceInstallsFromUriAndReplaces()
{
	TestLoader testLoader(codeCachePath());
	BundleLoader& loader = testLoader.loader();
	BundleRepository repo(_installPath, loader);
	Poco::AutoPtr<Poco::OSP::BundleInstallerService> pInstaller(new Poco::OSP::BundleInstallerService(repo));

	const std::string archivePath(Path(_scratchPath, "download.bndl").toString());
	{
		Poco::FileOutputStream ostr(archivePath);
		ostr << BundleArchive::create({{"META-INF/manifest.mf", BundleArchive::manifest("com.appinf.osp.test.installed", "1.0.0")}});
	}
	Bundle::Ptr pBundle = pInstaller->installBundle(Poco::URI(Path(archivePath)).toString());
	assertTrue (pBundle->symbolicName() == "com.appinf.osp.test.installed");

	std::istringstream istr(BundleArchive::create({{"META-INF/manifest.mf", BundleArchive::manifest("com.appinf.osp.test.installed", "2.0.0")}}));
	Bundle::Ptr pReplacement = pInstaller->replaceBundle("com.appinf.osp.test.installed", istr);
	assertTrue (pReplacement->version() == Version(2, 0, 0));
	assertTrue (loader.findBundle("com.appinf.osp.test.installed") == pReplacement);
	std::vector<std::string> files;
	File(_installPath).list(files);
	assertTrue (files == std::vector<std::string>({"com.appinf.osp.test.installed_2.0.0.bndl"}));
}


void BundleRepositoryTest::handleLoadedFailsForVersion2(const void* /*sender*/, Poco::OSP::BundleEvent& event)
{
	if (event.bundle()->version() == Version(2, 0, 0))
		throw Poco::IOException("bundle cannot be loaded", event.bundle()->symbolicName());
}


void BundleRepositoryTest::handleLoadedFailsWithStdExceptionForVersion2(const void* /*sender*/,
	Poco::OSP::BundleEvent& event)
{
	if (event.bundle()->version() == Version(2, 0, 0))
		throw std::runtime_error("bundle cannot be loaded");
}


void BundleRepositoryTest::handleInstalled(const void* /*sender*/, Poco::OSP::BundleEvent& event)
{
	_installedBundles.push_back(event.bundle()->symbolicName());
}


void BundleRepositoryTest::setUp()
{
	_pRootChannel = Logger::root().getChannel();
	_rootLevel = Logger::root().getLevel();
	_scratchPath = Poco::TemporaryFile::tempName();
	_installPath = Path(_scratchPath, "installed-bundles").toString();
	_developmentPath = Path(_scratchPath, "development-bundles").toString();
	File(_installPath).createDirectories();
	File(_developmentPath).createDirectories();
}


void BundleRepositoryTest::tearDown()
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


std::string BundleRepositoryTest::codeCachePath() const
{
	return Path(_scratchPath, "codeCache").toString();
}


std::vector<std::string> BundleRepositoryTest::loadedBundleNames(const BundleLoader& loader)
{
	std::vector<Bundle::Ptr> bundles;
	loader.listBundles(bundles);
	std::vector<std::string> names;
	names.reserve(bundles.size());
	for (const auto& pBundle: bundles)
	{
		names.push_back(pBundle->symbolicName());
	}
	std::sort(names.begin(), names.end());
	return names;
}


std::string BundleRepositoryTest::createBundleRepository()
{
	Path repositoryPath(Path::forDirectory(_scratchPath));
	repositoryPath.pushDirectory("bundles");
	File(repositoryPath).createDirectories();
	const std::string testBundlesPath(OSPBundleTestSuite::testBundlesPath());
	std::vector<std::string> names;
	File(testBundlesPath).list(names);
	for (const auto& name: names)
	{
		File(Path(Path::forDirectory(testBundlesPath), name)).copyTo(repositoryPath.toString());
	}

	Poco::FileOutputStream ostr(Path(repositoryPath, "com.appinf.osp.bundle2_1.0.0.bndl").toString());
	ostr << BundleArchive::createFromDirectory(OSPBundleTestSuite::findBundle("com.appinf.osp.bundle2_1.0.0"));
	ostr.close();

	return repositoryPath.toString();
}


CppUnit::Test* BundleRepositoryTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("BundleRepositoryTest");

	CppUnit_addTest(pSuite, BundleRepositoryTest, testLoadFindsNewestVersionOfEachBundle);
	CppUnit_addTest(pSuite, BundleRepositoryTest, testLoadKeepsNewerVersionFoundFirst);
	CppUnit_addTest(pSuite, BundleRepositoryTest, testLoadContinuesAfterBundleThatCannotBeLoaded);
	CppUnit_addTest(pSuite, BundleRepositoryTest, testLoadUnloadsBundleWhoseLoadedDelegateFails);
	CppUnit_addTest(pSuite, BundleRepositoryTest, testLoadReportsWhetherFailedBundleIsUnloaded);
	CppUnit_addTest(pSuite, BundleRepositoryTest, testLoadChoosesBundleOfSameVersionByName);
	CppUnit_addTest(pSuite, BundleRepositoryTest, testInstallWritesArchiveIntoFirstPathOnce);
	CppUnit_addTest(pSuite, BundleRepositoryTest, testFilterThatAcceptsAllLoadsEveryBundle);
	CppUnit_addTest(pSuite, BundleRepositoryTest, testFilterThatRejectsAllLoadsNoBundle);
	CppUnit_addTest(pSuite, BundleRepositoryTest, testFilterThatAcceptsOneLoadsOnlyThatBundle);
	CppUnit_addTest(pSuite, BundleRepositoryTest, testInstallRejectsSymbolicNameThatIsNoFileName);
	CppUnit_addTest(pSuite, BundleRepositoryTest, testInstallRejectsBundleThatFilterRejects);
	CppUnit_addTest(pSuite, BundleRepositoryTest, testTemporaryInstallFileIsNotLoaded);
	CppUnit_addTest(pSuite, BundleRepositoryTest, testConcurrentInstallsOfSameBundle);
	CppUnit_addTest(pSuite, BundleRepositoryTest, testInstallFiresBundleInstalled);
	CppUnit_addTest(pSuite, BundleRepositoryTest, testUninstallKeepsBundleOutsideInstallRepository);
	CppUnit_addTest(pSuite, BundleRepositoryTest, testUninstallKeepsBundleInstalledIntoSecondaryPath);
	CppUnit_addTest(pSuite, BundleRepositoryTest, testInstallKeepsLeftoverTemporaryFile);
	CppUnit_addTest(pSuite, BundleRepositoryTest, testInstallKeepsLeftoverTemporaryDirectory);
	CppUnit_addTest(pSuite, BundleRepositoryTest, testInstallKeepsFileOfSameNameThatIsNoLoadedBundle);
	CppUnit_addTest(pSuite, BundleRepositoryTest, testReplacementWithSameVersion);
	CppUnit_addTest(pSuite, BundleRepositoryTest, testFailedReplacementKeepsReplacedBundle);
	CppUnit_addTest(pSuite, BundleRepositoryTest, testFailedReplacementIntoSecondaryPathKeepsReplacedBundle);
	CppUnit_addTest(pSuite, BundleRepositoryTest, testFailedReplacementStartsRequiringBundleAgain);
	CppUnit_addTest(pSuite, BundleRepositoryTest, testReplacementDoesNotReportReplacedFileAsKept);
	CppUnit_addTest(pSuite, BundleRepositoryTest, testReplacementStartsRequiringBundleAgain);
	CppUnit_addTest(pSuite, BundleRepositoryTest, testActivatorInstallsBundlesDuringReplacement);
	CppUnit_addTest(pSuite, BundleRepositoryTest, testReplacementOfUnresolvedBundleWithLibrary);
	CppUnit_addTest(pSuite, BundleRepositoryTest, testFailedUninstallOfReplacedBundleReportsItsError);
	CppUnit_addTest(pSuite, BundleRepositoryTest, testFailedInstallReportsBundleAsUnloaded);
	CppUnit_addTest(pSuite, BundleRepositoryTest, testInstallOfCorruptArchiveLeavesNoFile);
	CppUnit_addTest(pSuite, BundleRepositoryTest, testMissingRepositoryPathIsSkipped);
	CppUnit_addTest(pSuite, BundleRepositoryTest, testRepositoryPathWithGlobPattern);
	CppUnit_addTest(pSuite, BundleRepositoryTest, testInstallerServiceInstallsFromUriAndReplaces);

	return pSuite;
}
