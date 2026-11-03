//
// BundleStreamFactoryTest.cpp
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#include "BundleStreamFactoryTest.h"
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
#include "Poco/OSP/BundleStreamFactory.h"
#include "Poco/OSP/SystemEvents.h"
#include "Poco/URIStreamOpener.h"
#include "Poco/Path.h"
#include "Poco/File.h"
#include "Poco/TemporaryFile.h"
#include "Poco/Exception.h"
#include <memory>
#include <string>


using Poco::OSP::Bundle;
using Poco::OSP::BundleFactory;
using Poco::OSP::BundleContextFactory;
using Poco::OSP::BundleLoader;
using Poco::OSP::CodeCache;
using Poco::OSP::ServiceRegistry;
using Poco::OSP::LanguageTag;
using Poco::OSP::BundleStreamFactory;
using Poco::Path;
using Poco::File;


namespace
{
	class TestLoader
		/// A BundleLoader with its code cache in a temporary directory,
		/// and the bundle given to the constructor loaded.
	{
	public:
		TestLoader() = delete;

		explicit TestLoader(const std::string& bundlePath):
			_codeCache(_codeCacheDir.path()),
			_loader(_codeCache,
				BundleFactory::Ptr(new BundleFactory(LanguageTag("en", "US"))),
				BundleContextFactory::Ptr(new BundleContextFactory(_registry, _systemEvents)))
		{
			_loader.loadBundle(bundlePath);
		}

		~TestLoader() = default;

		[[nodiscard]] BundleLoader& loader()
		{
			return _loader;
		}

	private:
		Poco::TemporaryFile _codeCacheDir;
		CodeCache _codeCache;
		ServiceRegistry _registry;
		Poco::OSP::SystemEvents _systemEvents;
		BundleLoader _loader;
	};
}


BundleStreamFactoryTest::BundleStreamFactoryTest(const std::string& name): CppUnit::TestCase(name)
{
}


BundleStreamFactoryTest::~BundleStreamFactoryTest() = default;


void BundleStreamFactoryTest::testOpenReadsBundleResource()
{
	TestLoader testLoader(findBundle("com.appinf.osp.bundle2_1.0.0"));
	BundleLoader& loader = testLoader.loader();

	Poco::URIStreamOpener opener;
	opener.registerStreamFactory("bndl", new BundleStreamFactory(loader));

	std::unique_ptr<std::istream> pStream(opener.open("bndl://com.appinf.osp.bundle2/lang.txt"));
	assert (pStream != nullptr);
	std::string s;
	*pStream >> s;
	assert (s == "en-US");
}


void BundleStreamFactoryTest::testOpenOfMissingResourceOrBundleThrowsNotFound()
{
	TestLoader testLoader(findBundle("com.appinf.osp.bundle2_1.0.0"));
	BundleLoader& loader = testLoader.loader();

	Poco::URIStreamOpener opener;
	opener.registerStreamFactory("bndl", new BundleStreamFactory(loader));

	try
	{
		std::unique_ptr<std::istream> pStream(opener.open("bndl://com.appinf.osp.bundle2/notfound.txt"));
		fail("resource not found - must throw");
	}
	catch (Poco::NotFoundException&)
	{
	}

	try
	{
		std::unique_ptr<std::istream> pStream(opener.open("bndl://com.appinf.osp.bundle99/notfound.txt"));
		fail("bundle not found - must throw");
	}
	catch (Poco::NotFoundException&)
	{
	}
}


void BundleStreamFactoryTest::testUriWithoutPathIsRejected()
{
	TestLoader testLoader(findBundle("com.appinf.osp.bundle2_1.0.0"));
	BundleLoader& loader = testLoader.loader();

	Poco::URIStreamOpener opener;
	opener.registerStreamFactory("bndl", new BundleStreamFactory(loader));

	try
	{
		std::unique_ptr<std::istream> pStream(opener.open("bndl://com.appinf.osp.bundle2"));
		fail("URI without a path - must throw");
	}
	catch (Poco::InvalidArgumentException&)
	{
	}
}


void BundleStreamFactoryTest::testDefaultOpenerUsesRegisteredFactory()
{
	TestLoader testLoader(findBundle("com.appinf.osp.bundle2_1.0.0"));
	BundleLoader& loader = testLoader.loader();

	BundleStreamFactory::registerFactory(loader);
	std::unique_ptr<std::istream> pStream(Poco::URIStreamOpener::defaultOpener().open("bndl://com.appinf.osp.bundle2/lang.txt"));
	assertTrue (pStream != nullptr);
	std::string s;
	*pStream >> s;
	assertTrue (s == "en-US");

	BundleStreamFactory::unregisterFactory();
	assertFalse (Poco::URIStreamOpener::defaultOpener().supportsScheme("bndl"));
}


void BundleStreamFactoryTest::setUp()
{
}


void BundleStreamFactoryTest::tearDown()
{
	// the factory refers to the loader of the test that registered it
	if (Poco::URIStreamOpener::defaultOpener().supportsScheme("bndl"))
	{
		BundleStreamFactory::unregisterFactory();
	}
}


std::string BundleStreamFactoryTest::findBundle(const std::string& name)
{
	std::string path = OSPBundleTestSuite::findBundle(name);
	assert(!path.empty());
	return path;
}


CppUnit::Test* BundleStreamFactoryTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("BundleStreamFactoryTest");

	CppUnit_addTest(pSuite, BundleStreamFactoryTest, testOpenReadsBundleResource);
	CppUnit_addTest(pSuite, BundleStreamFactoryTest, testOpenOfMissingResourceOrBundleThrowsNotFound);
	CppUnit_addTest(pSuite, BundleStreamFactoryTest, testUriWithoutPathIsRejected);
	CppUnit_addTest(pSuite, BundleStreamFactoryTest, testDefaultOpenerUsesRegisteredFactory);

	return pSuite;
}
