//
// WebServerDispatcherTest.cpp
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#include "WebServerDispatcherTest.h"
#include "Poco/OSP/Web/WebServerDispatcher.h"
#include "Poco/OSP/Web/MediaTypeMapper.h"
#include "Poco/OSP/Bundle.h"
#include "Poco/OSP/BundleFactory.h"
#include "Poco/OSP/BundleContextFactory.h"
#include "Poco/OSP/BundleLoader.h"
#include "Poco/OSP/CodeCache.h"
#include "Poco/OSP/ServiceRegistry.h"
#include "Poco/OSP/LanguageTag.h"
#include "Poco/OSP/Version.h"
#include "Poco/OSP/SystemEvents.h"
#include "Poco/Path.h"
#include "Poco/File.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include <map>
#include <fstream>


using namespace Poco::OSP::Web;
using namespace Poco::OSP;
using Poco::Path;
using Poco::File;


WebServerDispatcherTest::WebServerDispatcherTest(const std::string& name): CppUnit::TestCase(name)
{
}


WebServerDispatcherTest::~WebServerDispatcherTest()
{
}


void WebServerDispatcherTest::testVirtualDirectoryRoot()
{
	CodeCache cc("codeCache");
	ServiceRegistry reg;
	LanguageTag lang("en", "US");

	BundleFactory::Ptr pBundleFactory(new BundleFactory(lang));
	Poco::OSP::SystemEvents systemEvents;
	BundleContextFactory::Ptr pBundleContextFactory(new BundleContextFactory(reg, systemEvents));
	BundleLoader loader(cc, pBundleFactory, pBundleContextFactory);
	BundleEvents events;

	Bundle::Ptr pBundle = loader.createBundle("testBundle.zip");
	WebServerDispatcher::Config config;
	config.pContext = pBundleContextFactory->createBundleContext(loader, pBundle, events);
	WebServerDispatcher disp(config);
	WebServerDispatcher::VirtualPath vPath;
	vPath.path = "/";
	vPath.resource = "/res";
	vPath.security.mode = WebServerDispatcher::SM_NONE;
	vPath.pOwnerBundle = pBundle;
	disp.addVirtualPath(vPath);
	try
	{
		disp.addVirtualPath(vPath);
		fail("must fail");
	}
	catch(Poco::Exception&)
	{
	}
}


void WebServerDispatcherTest::testVirtualDirectoryAllow()
{
	CodeCache cc("codeCache");
	ServiceRegistry reg;
	LanguageTag lang("en", "US");

	BundleFactory::Ptr pBundleFactory(new BundleFactory(lang));
	Poco::OSP::SystemEvents systemEvents;
	BundleContextFactory::Ptr pBundleContextFactory(new BundleContextFactory(reg, systemEvents));
	BundleLoader loader(cc, pBundleFactory, pBundleContextFactory);
	BundleEvents events;

	Bundle::Ptr pBundle = loader.createBundle("testBundle.zip");
	WebServerDispatcher::Config config;
	config.pContext = pBundleContextFactory->createBundleContext(loader, pBundle, events);
	WebServerDispatcher disp(config);
	WebServerDispatcher::VirtualPath vPath;
	vPath.path = "/";
	vPath.resource = "res";
	vPath.security.mode = WebServerDispatcher::SM_ALL;
	vPath.pOwnerBundle = pBundle;
	disp.addVirtualPath(vPath);

	vPath.path = "/sub";
	vPath.resource = "res2";
	vPath.security.mode = WebServerDispatcher::SM_OWNER;
	disp.addVirtualPath(vPath);

	vPath.path = "/sub/subsub";
	vPath.resource = "res3";
	vPath.security.mode = WebServerDispatcher::SM_NONE;
	disp.addVirtualPath(vPath);

	try
	{
		vPath.path = "/sub/subsub/subsubsub";
		vPath.resource = "res4";
		vPath.security.mode = WebServerDispatcher::SM_NONE;
		disp.addVirtualPath(vPath);

		fail("must fail");
	}
	catch(Poco::Exception&)
	{
	}
}


void WebServerDispatcherTest::testVirtualDirectoryFail()
{
	// add code for second test here
	CodeCache cc("codeCache");
	ServiceRegistry reg;
	LanguageTag lang("en", "US");

	BundleFactory::Ptr pBundleFactory(new BundleFactory(lang));
	Poco::OSP::SystemEvents systemEvents;
	BundleContextFactory::Ptr pBundleContextFactory(new BundleContextFactory(reg, systemEvents));
	BundleLoader loader(cc, pBundleFactory, pBundleContextFactory);
	BundleEvents events;

	Bundle::Ptr pBundle = loader.createBundle("testBundle.zip");
	WebServerDispatcher::Config config;
	config.pContext = pBundleContextFactory->createBundleContext(loader, pBundle, events);
	WebServerDispatcher disp(config);

	WebServerDispatcher::VirtualPath vPath;
	vPath.path = "/sub/subsub";
	vPath.resource = "res";
	vPath.security.mode = WebServerDispatcher::SM_OWNER;
	vPath.pOwnerBundle = pBundle;
	disp.addVirtualPath(vPath);

	vPath.path = "/sub";
	vPath.resource = "res2";
	vPath.security.mode = WebServerDispatcher::SM_ALL;
	disp.addVirtualPath(vPath);

	try
	{
		vPath.path = "/";
		vPath.resource = "res3";
		vPath.security.mode = WebServerDispatcher::SM_OWNER;
		disp.addVirtualPath(vPath);

		// must fail because we have a child with a less stricter restriction SM_ALL
		fail("must fail");
	}
	catch(Poco::Exception&)
	{
	}
}


void WebServerDispatcherTest::testRemoveDir()
{
	// add code for second test here
	CodeCache cc("codeCache");
	ServiceRegistry reg;
	LanguageTag lang("en", "US");

	BundleFactory::Ptr pBundleFactory(new BundleFactory(lang));
	Poco::OSP::SystemEvents systemEvents;
	BundleContextFactory::Ptr pBundleContextFactory(new BundleContextFactory(reg, systemEvents));
	BundleLoader loader(cc, pBundleFactory, pBundleContextFactory);
	BundleEvents events;

	Bundle::Ptr pBundle = loader.createBundle("testBundle.zip");
	WebServerDispatcher::Config config;
	config.pContext = pBundleContextFactory->createBundleContext(loader, pBundle, events);
	WebServerDispatcher disp(config);

	WebServerDispatcher::VirtualPath vPath;
	vPath.path = "/sub/";
	vPath.resource = "res";
	vPath.security.mode = WebServerDispatcher::SM_OWNER;
	vPath.pOwnerBundle = pBundle;
	disp.addVirtualPath(vPath);

	vPath.path = "/sub/subsub";
	vPath.resource = "res2";
	vPath.security.mode = WebServerDispatcher::SM_NONE;
	disp.addVirtualPath(vPath);

	disp.removeVirtualPath("/sub/subsub");

	disp.addVirtualPath(vPath);
}


void WebServerDispatcherTest::testParseRange()
{
	CodeCache cc("codeCache");
	ServiceRegistry reg;
	LanguageTag lang("en", "US");

	BundleFactory::Ptr pBundleFactory(new BundleFactory(lang));
	Poco::OSP::SystemEvents systemEvents;
	BundleContextFactory::Ptr pBundleContextFactory(new BundleContextFactory(reg, systemEvents));
	BundleLoader loader(cc, pBundleFactory, pBundleContextFactory);
	BundleEvents events;

	Bundle::Ptr pBundle = loader.createBundle("testBundle.zip");
	WebServerDispatcher::Config config;
	config.pContext = pBundleContextFactory->createBundleContext(loader, pBundle, events);
	WebServerDispatcher disp(config);

	Poco::File::FileSize fileSize = 1024;
	Poco::File::FileSize rangeStart, rangeLength;

	// valid range header specifying the entire file (0-1023)
	std::string validRange = "bytes=0-1023";
	bool result = disp.parseRange(validRange, fileSize, rangeStart, rangeLength);
	assertTrue(result);
	assertEqual(static_cast<Poco::File::FileSize>(0), rangeStart);
	assertEqual(static_cast<Poco::File::FileSize>(1024), rangeLength);

	// valid range header specifying the middle of the file (512-1023)
	validRange = "bytes=512-1023";
	result = disp.parseRange(validRange, fileSize, rangeStart, rangeLength);
	assertTrue(result);
	assertEqual(static_cast<Poco::File::FileSize>(512), rangeStart);
	assertEqual(static_cast<Poco::File::FileSize>(512), rangeLength);

	// out of range header
	std::string outOfRangeRange = "bytes=2048-4096";
	result = disp.parseRange(outOfRangeRange, fileSize, rangeStart, rangeLength);
	assertFalse(!result);
	assertEqual(static_cast<Poco::File::FileSize>(2048), rangeStart);
	assertEqual(static_cast<Poco::File::FileSize>(2049), rangeLength);
}


void WebServerDispatcherTest::testCanonicalPathWithQuery()
{
	// No query in original — canonical path returned unchanged.
	assertEqual(std::string("/foo/"), WebServerDispatcher::canonicalPathWithQuery("/foo/", "/foo"));

	// Query preserved verbatim across the trailing-slash redirect.
	assertEqual(std::string("/foo/?bar=1"), WebServerDispatcher::canonicalPathWithQuery("/foo/", "/foo?bar=1"));

	// '+' in the source query value survives intact (regression: it must
	// not be re-encoded or stripped during canonicalization).
	assertEqual(std::string("/foo/?q=hello+world"), WebServerDispatcher::canonicalPathWithQuery("/foo/", "/foo?q=hello+world"));

	// Multi-parameter query preserved.
	assertEqual(std::string("/foo/?a=1&b=2"), WebServerDispatcher::canonicalPathWithQuery("/foo/", "/foo?a=1&b=2"));
}


void WebServerDispatcherTest::testRedirectLocationOn401()
{
	// Plain case: original path appended as percent-encoded redirect= parameter.
	{
		std::string loc = WebServerDispatcher::redirectLocationOn401("/login", "/protected/page");
		assertEqual(std::string("/login?redirect=%2Fprotected%2Fpage"), loc);
	}

	// '+' in the request query MUST be percent-encoded (HTMLForm decoders
	// treat raw '+' as space, which would corrupt the value on round-trip).
	{
		std::string loc = WebServerDispatcher::redirectLocationOn401("/login", "/search?q=hello+world");
		assertTrue(loc.find("%2B") != std::string::npos);
		assertTrue(loc.find("hello+world") == std::string::npos);
	}

	// Loop guard: request already targets the login path → no redirect= appended,
	// even when the request carries an unrelated query.
	{
		std::string loc = WebServerDispatcher::redirectLocationOn401("/login", "/login");
		assertEqual(std::string("/login"), loc);
	}
	{
		std::string loc = WebServerDispatcher::redirectLocationOn401("/login", "/login?other=foo");
		assertEqual(std::string("/login"), loc);
	}

	// Loop guard: request already carries its own redirect= → don't nest another.
	{
		std::string loc = WebServerDispatcher::redirectLocationOn401("/login", "/login?redirect=%2Ffoo");
		assertEqual(std::string("/login"), loc);
	}
}


void WebServerDispatcherTest::setUp()
{
	// The following is a ZIP file containing the same
	// directory hierarchy as is used in BundleDirectoryTest.
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

	std::ofstream ostr("testBundle.zip", std::ios::binary);
	ostr.write(reinterpret_cast<const char*>(&TEST_BUNDLE_ZIP[0]), sizeof(TEST_BUNDLE_ZIP));
}


void WebServerDispatcherTest::tearDown()
{
}


CppUnit::Test* WebServerDispatcherTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("WebServerDispatcherTest");

	CppUnit_addTest(pSuite, WebServerDispatcherTest, testVirtualDirectoryRoot);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testVirtualDirectoryAllow);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testVirtualDirectoryFail);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testRemoveDir);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testParseRange);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testCanonicalPathWithQuery);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testRedirectLocationOn401);

	return pSuite;
}
