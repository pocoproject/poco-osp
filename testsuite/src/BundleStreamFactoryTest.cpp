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
#include "TestLoader.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "Poco/OSP/Bundle.h"
#include "Poco/OSP/BundleLoader.h"
#include "Poco/OSP/BundleStreamFactory.h"
#include "Poco/URIStreamOpener.h"
#include "Poco/TemporaryFile.h"
#include "Poco/Exception.h"
#include <memory>
#include <string>


using Poco::OSP::Bundle;
using Poco::OSP::BundleLoader;
using Poco::OSP::BundleStreamFactory;


BundleStreamFactoryTest::BundleStreamFactoryTest(const std::string& name): CppUnit::TestCase(name)
{
}


BundleStreamFactoryTest::~BundleStreamFactoryTest() = default;


void BundleStreamFactoryTest::testOpenReadsBundleResource()
{
	Poco::TemporaryFile codeCacheDir;
	TestLoader testLoader(codeCacheDir.path());
	BundleLoader& loader = testLoader.loader();
	loader.loadBundle(OSPBundleTestSuite::findBundle("com.appinf.osp.bundle2_1.0.0"));

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
	Poco::TemporaryFile codeCacheDir;
	TestLoader testLoader(codeCacheDir.path());
	BundleLoader& loader = testLoader.loader();
	loader.loadBundle(OSPBundleTestSuite::findBundle("com.appinf.osp.bundle2_1.0.0"));

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
	Poco::TemporaryFile codeCacheDir;
	TestLoader testLoader(codeCacheDir.path());
	BundleLoader& loader = testLoader.loader();
	loader.loadBundle(OSPBundleTestSuite::findBundle("com.appinf.osp.bundle2_1.0.0"));

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
	Poco::TemporaryFile codeCacheDir;
	TestLoader testLoader(codeCacheDir.path());
	BundleLoader& loader = testLoader.loader();
	loader.loadBundle(OSPBundleTestSuite::findBundle("com.appinf.osp.bundle2_1.0.0"));

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


CppUnit::Test* BundleStreamFactoryTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("BundleStreamFactoryTest");

	CppUnit_addTest(pSuite, BundleStreamFactoryTest, testOpenReadsBundleResource);
	CppUnit_addTest(pSuite, BundleStreamFactoryTest, testOpenOfMissingResourceOrBundleThrowsNotFound);
	CppUnit_addTest(pSuite, BundleStreamFactoryTest, testUriWithoutPathIsRejected);
	CppUnit_addTest(pSuite, BundleStreamFactoryTest, testDefaultOpenerUsesRegisteredFactory);

	return pSuite;
}
