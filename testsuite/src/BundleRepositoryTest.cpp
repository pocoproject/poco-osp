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
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "Poco/OSP/BundleRepository.h"
#include "Poco/OSP/Bundle.h"
#include "Poco/OSP/BundleFactory.h"
#include "Poco/OSP/BundleContextFactory.h"
#include "Poco/OSP/BundleLoader.h"
#include "Poco/OSP/BundleFilter.h"
#include "Poco/OSP/CodeCache.h"
#include "Poco/OSP/ServiceRegistry.h"
#include "Poco/OSP/LanguageTag.h"
#include "Poco/OSP/Version.h"
#include "Poco/OSP/OSPException.h"
#include "Poco/OSP/SystemEvents.h"
#include "Poco/OSP/BundleEvents.h"
#include "Poco/OSP/BundleInstallerService.h"
#include "Poco/Path.h"
#include "Poco/File.h"
#include "Poco/FileStream.h"
#include "Poco/StreamCopier.h"
#include "Poco/TemporaryFile.h"
#include "Poco/Logger.h"
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
	CodeCache cc(codeCachePath());
	ServiceRegistry reg;
	LanguageTag lang("en", "US");
	BundleFactory::Ptr pBundleFactory(new BundleFactory(lang));
	Poco::OSP::SystemEvents systemEvents;
	BundleContextFactory::Ptr pBundleContextFactory(new BundleContextFactory(reg, systemEvents));
	BundleLoader loader(cc, pBundleFactory, pBundleContextFactory);
	BundleRepository repo(findBundleRepository(), loader);
	
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
	CodeCache cc(codeCachePath());
	ServiceRegistry reg;
	LanguageTag lang("en", "US");
	BundleFactory::Ptr pBundleFactory(new BundleFactory(lang));
	Poco::OSP::SystemEvents systemEvents;
	BundleContextFactory::Ptr pBundleContextFactory(new BundleContextFactory(reg, systemEvents));
	BundleLoader loader(cc, pBundleFactory, pBundleContextFactory);

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


void BundleRepositoryTest::testLoadChoosesBundleOfSameVersionByName()
{
	CodeCache cc(codeCachePath());
	ServiceRegistry reg;
	LanguageTag lang("en", "US");
	BundleFactory::Ptr pBundleFactory(new BundleFactory(lang));
	Poco::OSP::SystemEvents systemEvents;
	BundleContextFactory::Ptr pBundleContextFactory(new BundleContextFactory(reg, systemEvents));
	BundleLoader loader(cc, pBundleFactory, pBundleContextFactory);

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

	CodeCache cc(codeCachePath());
	ServiceRegistry reg;
	LanguageTag lang("en", "US");
	BundleFactory::Ptr pBundleFactory(new BundleFactory(lang));
	Poco::OSP::SystemEvents systemEvents;
	BundleContextFactory::Ptr pBundleContextFactory(new BundleContextFactory(reg, systemEvents));
	BundleLoader loader(cc, pBundleFactory, pBundleContextFactory);
	BundleRepository repo(_installPath + Path::pathSeparator() + findBundleRepository(), loader);

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
	CodeCache cc(codeCachePath());
	ServiceRegistry reg;
	LanguageTag lang("en", "US");
	BundleFactory::Ptr pBundleFactory(new BundleFactory(lang));
	Poco::OSP::SystemEvents systemEvents;
	BundleContextFactory::Ptr pBundleContextFactory(new BundleContextFactory(reg, systemEvents));
	BundleLoader loader(cc, pBundleFactory, pBundleContextFactory);
	Poco::AutoPtr<CountingFilter> pFilter(new CountingFilter(true));
	BundleRepository repo(findBundleRepository(), loader, pFilter);
	
	repo.loadBundles();
	
	assertEqual (TEST_BUNDLE_COUNT, pFilter->calls());
	assertTrue (loadedBundleNames(loader) == TEST_BUNDLE_NAMES);
}


void BundleRepositoryTest::testFilterThatRejectsAllLoadsNoBundle()
{
	CodeCache cc(codeCachePath());
	ServiceRegistry reg;
	LanguageTag lang("en", "US");
	BundleFactory::Ptr pBundleFactory(new BundleFactory(lang));
	Poco::OSP::SystemEvents systemEvents;
	BundleContextFactory::Ptr pBundleContextFactory(new BundleContextFactory(reg, systemEvents));
	BundleLoader loader(cc, pBundleFactory, pBundleContextFactory);
	Poco::AutoPtr<CountingFilter> pFilter(new CountingFilter(false));
	BundleRepository repo(findBundleRepository(), loader, pFilter);
	
	repo.loadBundles();
	
	assertEqual (TEST_BUNDLE_COUNT, pFilter->calls());
	assertTrue (loadedBundleNames(loader).empty());
}


void BundleRepositoryTest::testFilterThatAcceptsOneLoadsOnlyThatBundle()
{
	CodeCache cc(codeCachePath());
	ServiceRegistry reg;
	LanguageTag lang("en", "US");
	BundleFactory::Ptr pBundleFactory(new BundleFactory(lang));
	Poco::OSP::SystemEvents systemEvents;
	BundleContextFactory::Ptr pBundleContextFactory(new BundleContextFactory(reg, systemEvents));
	BundleLoader loader(cc, pBundleFactory, pBundleContextFactory);
	BundleRepository repo(findBundleRepository(), loader, new AcceptOneFilter);
	
	repo.loadBundles();
	
	assertTrue (loadedBundleNames(loader) == std::vector<std::string>({"com.appinf.osp.bundle5"}));
}


void BundleRepositoryTest::testInstallRejectsSymbolicNameThatIsNoFileName()
{
	CodeCache cc(codeCachePath());
	ServiceRegistry reg;
	LanguageTag lang("en", "US");
	BundleFactory::Ptr pBundleFactory(new BundleFactory(lang));
	Poco::OSP::SystemEvents systemEvents;
	BundleContextFactory::Ptr pBundleContextFactory(new BundleContextFactory(reg, systemEvents));
	BundleLoader loader(cc, pBundleFactory, pBundleContextFactory);
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
	CodeCache cc(codeCachePath());
	ServiceRegistry reg;
	LanguageTag lang("en", "US");
	BundleFactory::Ptr pBundleFactory(new BundleFactory(lang));
	Poco::OSP::SystemEvents systemEvents;
	BundleContextFactory::Ptr pBundleContextFactory(new BundleContextFactory(reg, systemEvents));
	BundleLoader loader(cc, pBundleFactory, pBundleContextFactory);
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
	CodeCache cc(codeCachePath());
	ServiceRegistry reg;
	LanguageTag lang("en", "US");
	BundleFactory::Ptr pBundleFactory(new BundleFactory(lang));
	Poco::OSP::SystemEvents systemEvents;
	BundleContextFactory::Ptr pBundleContextFactory(new BundleContextFactory(reg, systemEvents));
	BundleLoader loader(cc, pBundleFactory, pBundleContextFactory);
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
	CodeCache cc(codeCachePath());
	ServiceRegistry reg;
	LanguageTag lang("en", "US");
	BundleFactory::Ptr pBundleFactory(new BundleFactory(lang));
	Poco::OSP::SystemEvents systemEvents;
	BundleContextFactory::Ptr pBundleContextFactory(new BundleContextFactory(reg, systemEvents));
	BundleLoader loader(cc, pBundleFactory, pBundleContextFactory);
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
	CodeCache cc(codeCachePath());
	ServiceRegistry reg;
	LanguageTag lang("en", "US");
	BundleFactory::Ptr pBundleFactory(new BundleFactory(lang));
	Poco::OSP::SystemEvents systemEvents;
	BundleContextFactory::Ptr pBundleContextFactory(new BundleContextFactory(reg, systemEvents));
	BundleLoader loader(cc, pBundleFactory, pBundleContextFactory);
	BundleRepository repo(_installPath, loader);

	assertTrue (Logger::has("osp.core.BundleRepository") != nullptr);

	_installedBundles.clear();
	loader.events().bundleInstalled += Poco::delegate(this, &BundleRepositoryTest::handleInstalled);
	std::istringstream istr(BundleArchive::create({{"META-INF/manifest.mf", BundleArchive::manifest("com.appinf.osp.test.installed", "1.0.0")}}));
	Bundle::Ptr pBundle = repo.installBundle(istr);
	loader.events().bundleInstalled -= Poco::delegate(this, &BundleRepositoryTest::handleInstalled);

	assertTrue (_installedBundles == std::vector<std::string>({"com.appinf.osp.test.installed"}));
}


void BundleRepositoryTest::testUninstallKeepsBundleOutsideInstallRepository()
{
	CodeCache cc(codeCachePath());
	ServiceRegistry reg;
	LanguageTag lang("en", "US");
	BundleFactory::Ptr pBundleFactory(new BundleFactory(lang));
	Poco::OSP::SystemEvents systemEvents;
	BundleContextFactory::Ptr pBundleContextFactory(new BundleContextFactory(reg, systemEvents));
	BundleLoader loader(cc, pBundleFactory, pBundleContextFactory);

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
	CodeCache cc(codeCachePath());
	ServiceRegistry reg;
	LanguageTag lang("en", "US");
	BundleFactory::Ptr pBundleFactory(new BundleFactory(lang));
	Poco::OSP::SystemEvents systemEvents;
	BundleContextFactory::Ptr pBundleContextFactory(new BundleContextFactory(reg, systemEvents));
	BundleLoader loader(cc, pBundleFactory, pBundleContextFactory);
	BundleRepository repo(_installPath + Path::pathSeparator() + _developmentPath, loader);

	std::istringstream istr(BundleArchive::create({{"META-INF/manifest.mf", BundleArchive::manifest("com.appinf.osp.test.secondary", "1.0.0")}}));
	Bundle::Ptr pBundle = repo.installBundle(_developmentPath, istr);
	const File bundleFile(pBundle->path());
	pBundle->uninstall();

	assertTrue (bundleFile.exists());
}


void BundleRepositoryTest::testInstallKeepsLeftoverTemporaryFile()
{
	CodeCache cc(codeCachePath());
	ServiceRegistry reg;
	LanguageTag lang("en", "US");
	BundleFactory::Ptr pBundleFactory(new BundleFactory(lang));
	Poco::OSP::SystemEvents systemEvents;
	BundleContextFactory::Ptr pBundleContextFactory(new BundleContextFactory(reg, systemEvents));
	BundleLoader loader(cc, pBundleFactory, pBundleContextFactory);
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
	CodeCache cc(codeCachePath());
	ServiceRegistry reg;
	LanguageTag lang("en", "US");
	BundleFactory::Ptr pBundleFactory(new BundleFactory(lang));
	Poco::OSP::SystemEvents systemEvents;
	BundleContextFactory::Ptr pBundleContextFactory(new BundleContextFactory(reg, systemEvents));
	BundleLoader loader(cc, pBundleFactory, pBundleContextFactory);
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
	CodeCache cc(codeCachePath());
	ServiceRegistry reg;
	LanguageTag lang("en", "US");
	BundleFactory::Ptr pBundleFactory(new BundleFactory(lang));
	Poco::OSP::SystemEvents systemEvents;
	BundleContextFactory::Ptr pBundleContextFactory(new BundleContextFactory(reg, systemEvents));
	BundleLoader loader(cc, pBundleFactory, pBundleContextFactory);
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
	CodeCache cc(codeCachePath());
	ServiceRegistry reg;
	LanguageTag lang("en", "US");
	BundleFactory::Ptr pBundleFactory(new BundleFactory(lang));
	Poco::OSP::SystemEvents systemEvents;
	BundleContextFactory::Ptr pBundleContextFactory(new BundleContextFactory(reg, systemEvents));
	BundleLoader loader(cc, pBundleFactory, pBundleContextFactory);
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
	CodeCache cc(codeCachePath());
	ServiceRegistry reg;
	LanguageTag lang("en", "US");
	BundleFactory::Ptr pBundleFactory(new BundleFactory(lang));
	Poco::OSP::SystemEvents systemEvents;
	BundleContextFactory::Ptr pBundleContextFactory(new BundleContextFactory(reg, systemEvents));
	BundleLoader loader(cc, pBundleFactory, pBundleContextFactory);
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
	CodeCache cc(codeCachePath());
	ServiceRegistry reg;
	LanguageTag lang("en", "US");
	BundleFactory::Ptr pBundleFactory(new BundleFactory(lang));
	Poco::OSP::SystemEvents systemEvents;
	BundleContextFactory::Ptr pBundleContextFactory(new BundleContextFactory(reg, systemEvents));
	BundleLoader loader(cc, pBundleFactory, pBundleContextFactory);
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
	CodeCache cc(codeCachePath());
	ServiceRegistry reg;
	LanguageTag lang("en", "US");
	BundleFactory::Ptr pBundleFactory(new BundleFactory(lang));
	Poco::OSP::SystemEvents systemEvents;
	BundleContextFactory::Ptr pBundleContextFactory(new BundleContextFactory(reg, systemEvents));
	BundleLoader loader(cc, pBundleFactory, pBundleContextFactory);
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
	// Released only when both threads have finished: threads that block each
	// other are left running.
	auto pCodeCache = std::make_unique<CodeCache>(codeCachePath());
	auto pRegistry = std::make_unique<ServiceRegistry>();
	auto pSystemEvents = std::make_unique<Poco::OSP::SystemEvents>();
	LanguageTag lang("en", "US");
	BundleFactory::Ptr pBundleFactory(new BundleFactory(lang));
	BundleContextFactory::Ptr pBundleContextFactory(new BundleContextFactory(*pRegistry, *pSystemEvents));
	auto pLoader = std::make_unique<BundleLoader>(*pCodeCache, pBundleFactory, pBundleContextFactory);
	auto pRepo = std::make_unique<BundleRepository>(_installPath, *pLoader);

	const std::string replacedName("com.appinf.osp.test.replaced");
	std::istringstream oldArchive(BundleArchive::create({{"META-INF/manifest.mf", BundleArchive::manifest(replacedName, "1.0.0")}}));
	pRepo->installBundle(oldArchive);
	std::istringstream userArchive(BundleArchive::create({{"META-INF/manifest.mf",
		BundleArchive::manifest("com.appinf.osp.test.user", "1.0.0") + "Require-Bundle: " + replacedName + "\n"}}));
	Bundle::Ptr pUser = pRepo->installBundle(userArchive);
	pUser->resolve();
	pUser->start();

	// The replacing thread waits in bundleInstalled until the starting thread
	// starts the new bundle; then it starts the user, which requires the new
	// bundle, while the starting thread installs bundles from bundleStarting,
	// as an activator of the new bundle would.
	Poco::Event newBundleInstalled;
	Poco::Event newBundleStarting;
	CallbackDelegate installedDelegate([&](BundleEvent& event)
	{
		if (event.bundle()->symbolicName() == replacedName && event.bundle()->version() == Version(2, 0, 0))
		{
			newBundleInstalled.set();
			if (!newBundleStarting.tryWait(WAIT_MS)) throw Poco::TimeoutException("the new bundle is not started");
		}
	});
	std::atomic<bool> secondReplacementRejected(false);
	std::atomic<bool> otherInstalled(false);
	CallbackDelegate startingDelegate([&](BundleEvent& event)
	{
		if (event.bundle()->symbolicName() == replacedName && event.bundle()->version() == Version(2, 0, 0))
		{
			newBundleStarting.set();
			std::istringstream secondArchive(BundleArchive::create({{"META-INF/manifest.mf", BundleArchive::manifest(replacedName, "3.0.0")}}));
			try
			{
				pRepo->installBundle(secondArchive, replacedName);
			}
			catch (Poco::OSP::BundleInstallException&)
			{
				secondReplacementRejected = true;
			}
			std::istringstream otherArchive(BundleArchive::create({{"META-INF/manifest.mf", BundleArchive::manifest("com.appinf.osp.test.other", "1.0.0")}}));
			pRepo->installBundle(otherArchive);
			otherInstalled = true;
		}
	});
	pLoader->events().bundleInstalled += installedDelegate;
	pLoader->events().bundleStarting += startingDelegate;

	std::string replacingError;
	Poco::Event replacingDone;
	std::thread replacing([&]()
	{
		try
		{
			std::istringstream newArchive(BundleArchive::create({{"META-INF/manifest.mf", BundleArchive::manifest(replacedName, "2.0.0")}}));
			pRepo->installBundle(newArchive, replacedName);
		}
		catch (Poco::Exception& exc)
		{
			replacingError = exc.displayText();
		}
		replacingDone.set();
	});
	std::string startingError;
	Poco::Event startingDone;
	std::thread starting([&]()
	{
		try
		{
			if (!newBundleInstalled.tryWait(WAIT_MS)) throw Poco::TimeoutException("the new bundle is not installed");
			Bundle::Ptr pNew = pLoader->findBundle(replacedName);
			pNew->resolve();
			pNew->start();
		}
		catch (Poco::Exception& exc)
		{
			startingError = exc.displayText();
		}
		startingDone.set();
	});

	const bool replacingFinished = replacingDone.tryWait(WAIT_MS);
	const bool startingFinished = startingDone.tryWait(WAIT_MS);
	if (!replacingFinished || !startingFinished)
	{
		replacing.detach();
		starting.detach();
		static std::vector<const void*> blockedObjects;
		blockedObjects.push_back(pRepo.release());
		blockedObjects.push_back(pLoader.release());
		blockedObjects.push_back(pSystemEvents.release());
		blockedObjects.push_back(pRegistry.release());
		blockedObjects.push_back(pCodeCache.release());
		fail("the replacement and the installs from bundleStarting block each other");
	}
	replacing.join();
	starting.join();
	pLoader->events().bundleInstalled -= installedDelegate;
	pLoader->events().bundleStarting -= startingDelegate;

	assertEqual ("", replacingError);
	assertEqual ("", startingError);
	assertTrue (secondReplacementRejected);
	assertTrue (otherInstalled);
	Bundle::Ptr pNew = pLoader->findBundle(replacedName);
	assertTrue (pNew->version() == Version(2, 0, 0));
	assertTrue (pNew->isActive());
	assertTrue (pUser->isActive());

	pLoader->unloadAllBundles();
}


void BundleRepositoryTest::testReplacementDoesNotReportReplacedFileAsKept()
{
	// declared before the loader, which logs to it until tearDown() restores the channel
	std::ostringstream log;
	CodeCache cc(codeCachePath());
	ServiceRegistry reg;
	LanguageTag lang("en", "US");
	BundleFactory::Ptr pBundleFactory(new BundleFactory(lang));
	Poco::OSP::SystemEvents systemEvents;
	BundleContextFactory::Ptr pBundleContextFactory(new BundleContextFactory(reg, systemEvents));
	BundleLoader loader(cc, pBundleFactory, pBundleContextFactory);
	BundleRepository repo(_installPath + Path::pathSeparator() + _developmentPath, loader);

	// in a secondary path, where an uninstall keeps the file and says so
	std::istringstream oldArchive(BundleArchive::create({{"META-INF/manifest.mf", BundleArchive::manifest("com.appinf.osp.test.replaced", "1.0.0")}}));
	repo.installBundle(_developmentPath, oldArchive);

	Logger& loaderLogger = Logger::get("osp.core.BundleLoader");
	loaderLogger.setChannel(new Poco::StreamChannel(log));
	std::istringstream newArchive(BundleArchive::create({{"META-INF/manifest.mf", BundleArchive::manifest("com.appinf.osp.test.replaced", "2.0.0")}}));
	repo.installBundle(_developmentPath, newArchive, "com.appinf.osp.test.replaced");

	assertTrue (log.str().find("is kept") == std::string::npos);
	std::vector<std::string> files;
	File(_developmentPath).list(files);
	assertTrue (files == std::vector<std::string>({"com.appinf.osp.test.replaced_2.0.0.bndl"}));
}


void BundleRepositoryTest::testReplacementStartsRequiringBundleAgain()
{
	CodeCache cc(codeCachePath());
	ServiceRegistry reg;
	LanguageTag lang("en", "US");
	BundleFactory::Ptr pBundleFactory(new BundleFactory(lang));
	Poco::OSP::SystemEvents systemEvents;
	BundleContextFactory::Ptr pBundleContextFactory(new BundleContextFactory(reg, systemEvents));
	BundleLoader loader(cc, pBundleFactory, pBundleContextFactory);
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


void BundleRepositoryTest::testInstallOfCorruptArchiveLeavesNoFile()
{
	CodeCache cc(codeCachePath());
	ServiceRegistry reg;
	LanguageTag lang("en", "US");
	BundleFactory::Ptr pBundleFactory(new BundleFactory(lang));
	Poco::OSP::SystemEvents systemEvents;
	BundleContextFactory::Ptr pBundleContextFactory(new BundleContextFactory(reg, systemEvents));
	BundleLoader loader(cc, pBundleFactory, pBundleContextFactory);
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
	CodeCache cc(codeCachePath());
	ServiceRegistry reg;
	LanguageTag lang("en", "US");
	BundleFactory::Ptr pBundleFactory(new BundleFactory(lang));
	Poco::OSP::SystemEvents systemEvents;
	BundleContextFactory::Ptr pBundleContextFactory(new BundleContextFactory(reg, systemEvents));
	BundleLoader loader(cc, pBundleFactory, pBundleContextFactory);
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
	CodeCache cc(codeCachePath());
	ServiceRegistry reg;
	LanguageTag lang("en", "US");
	BundleFactory::Ptr pBundleFactory(new BundleFactory(lang));
	Poco::OSP::SystemEvents systemEvents;
	BundleContextFactory::Ptr pBundleContextFactory(new BundleContextFactory(reg, systemEvents));
	BundleLoader loader(cc, pBundleFactory, pBundleContextFactory);
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
	CodeCache cc(codeCachePath());
	ServiceRegistry reg;
	LanguageTag lang("en", "US");
	BundleFactory::Ptr pBundleFactory(new BundleFactory(lang));
	Poco::OSP::SystemEvents systemEvents;
	BundleContextFactory::Ptr pBundleContextFactory(new BundleContextFactory(reg, systemEvents));
	BundleLoader loader(cc, pBundleFactory, pBundleContextFactory);
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


std::string BundleRepositoryTest::findBundleRepository()
{
	std::string path = OSPBundleTestSuite::findBundlesPath();
	assert(!path.empty());
	return path;
}


CppUnit::Test* BundleRepositoryTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("BundleRepositoryTest");

	CppUnit_addTest(pSuite, BundleRepositoryTest, testLoadFindsNewestVersionOfEachBundle);
	CppUnit_addTest(pSuite, BundleRepositoryTest, testLoadKeepsNewerVersionFoundFirst);
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
	CppUnit_addTest(pSuite, BundleRepositoryTest, testInstallOfCorruptArchiveLeavesNoFile);
	CppUnit_addTest(pSuite, BundleRepositoryTest, testMissingRepositoryPathIsSkipped);
	CppUnit_addTest(pSuite, BundleRepositoryTest, testRepositoryPathWithGlobPattern);
	CppUnit_addTest(pSuite, BundleRepositoryTest, testInstallerServiceInstallsFromUriAndReplaces);

	return pSuite;
}
