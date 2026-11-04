//
// WebServerExtensionPointTest.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "WebServerExtensionPointTest.h"
#include "TemporaryBundle.h"
#include "ServerRequestStub.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "CppUnit/TestCase.h"
#include "CppUnit/Test.h"
#include "Poco/OSP/Web/WebServerDispatcher.h"
#include "Poco/OSP/Web/WebServerExtensionPoint.h"
#include "Poco/OSP/OSPException.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/Bundle.h"
#include "Poco/Net/HTTPRequestHandler.h"
#include "Poco/DOM/Document.h"
#include "Poco/DOM/Element.h"
#include "Poco/AutoPtr.h"
#include <map>
#include <string>
#include <memory>
#include <tuple>


using Poco::OSP::Web::WebServerDispatcher;
using Poco::OSP::Web::WebServerExtensionPoint;
using Poco::OSP::BundleContext;
using Poco::OSP::Bundle;
using Poco::XML::Document;
using Poco::XML::Element;


namespace
{
	class TestExtensionPoint: public WebServerExtensionPoint
		/// Makes handleCommon() accessible to the test.
	{
	public:
		TestExtensionPoint(BundleContext::Ptr pContext, WebServerDispatcher* pDispatcher):
			WebServerExtensionPoint(pContext, pDispatcher)
		{
		}

		[[nodiscard]] WebServerDispatcher::VirtualPath parse(Bundle::ConstPtr pBundle, Element* pElem)
		{
			WebServerDispatcher::VirtualPath vPath;
			handleCommon(pBundle, pElem, vPath);
			return vPath;
		}
	};

	using Attributes = std::map<std::string, std::string>;

	WebServerDispatcher::VirtualPath parseCommon(TemporaryBundle& bundle, const Attributes& attributes)
		/// Returns the virtual path that WebServerExtensionPoint::handleCommon()
		/// creates from an extension element with the given attributes.
	{
		WebServerDispatcher::Config config;
		config.pContext = bundle.context();
		WebServerDispatcher dispatcher(config);
		Poco::AutoPtr<TestExtensionPoint> pExtensionPoint(new TestExtensionPoint(bundle.context(), &dispatcher));
		Poco::AutoPtr<Document> pDoc(new Document);
		Poco::AutoPtr<Element> pElem(pDoc->createElement("extension"));
		for (const auto& [name, value]: attributes)
		{
			pElem->setAttribute(name, value);
		}
		return pExtensionPoint->parse(bundle.bundle(), pElem);
	}
}


WebServerExtensionPointTest::WebServerExtensionPointTest(const std::string& name): CppUnit::TestCase(name)
{
}


WebServerExtensionPointTest::~WebServerExtensionPointTest() = default;


void WebServerExtensionPointTest::setUp()
{
	_pBundle = std::make_unique<TemporaryBundle>("web.path = /app\n");
}


void WebServerExtensionPointTest::tearDown()
{
	_pBundle.reset();
}


void WebServerExtensionPointTest::testCommonAttributes()
{
	WebServerDispatcher::VirtualPath vPath = parseCommon(*_pBundle, {
		{"path", "${web.path}/api"},
		{"allowSpecialization", "Owner"},
		{"responseFormat", "JSON"},
		{"csrfProtection", "true"},
		{"secure", "true"},
		{"cache", "false"}});

	assertEqual (std::string("/app/api"), vPath.path);
	assertTrue (vPath.security.mode == WebServerDispatcher::SM_OWNER);
	assertTrue (vPath.responseFormat == WebServerDispatcher::RESPONSE_FORMAT_JSON);
	assertTrue (vPath.security.csrfProtection);
	assertEqual (std::string("X-XSRF-TOKEN"), vPath.security.csrfTokenHeader);
	assertTrue (vPath.security.secure);
	assertFalse (vPath.cache);
}


void WebServerExtensionPointTest::testCommonAttributeDefaults()
{
	WebServerDispatcher::VirtualPath vPath = parseCommon(*_pBundle, {{"path", "/app"}});

	assertTrue (vPath.security.mode == WebServerDispatcher::SM_NONE);
	assertTrue (vPath.responseFormat == WebServerDispatcher::RESPONSE_FORMAT_HTML);
	assertFalse (vPath.security.csrfProtection);
	assertTrue (vPath.security.csrfTokenHeader.empty());
	assertTrue (vPath.cache);
	assertTrue (vPath.cors.allowCredentials);
	assertTrue (vPath.redirectOn401.empty());
}


void WebServerExtensionPointTest::testInvalidAllowSpecialization()
{
	try
	{
		parseCommon(*_pBundle, {{"path", "/app"}, {"allowSpecialization", "others"}});
		fail("an invalid allowSpecialization value must be rejected");
	}
	catch (Poco::OSP::BundleException&)
	{
	}
}


#if defined(OSPWEB_TEST_LIBRARY)


void WebServerExtensionPointTest::testRequestHandlerFactoryComesFromItsBundleLibrary()
{
	// Each bundle has its own copy of the library, which exports a class with the same name.
	_pBundle->installLibrary("osp.web.test.first", OSPWEB_TEST_LIBRARY);
	_pBundle->installLibrary("osp.web.test.second", OSPWEB_TEST_LIBRARY);
	WebServerDispatcher::Config config;
	config.pContext = _pBundle->context();
	WebServerDispatcher dispatcher(config);
	Poco::AutoPtr<WebServerExtensionPoint> pExtensionPoint(new WebServerExtensionPoint(_pBundle->context(),
		&dispatcher));
	Poco::AutoPtr<Document> pDoc(new Document);
	for (const auto& [path, library, pBundle]: {std::make_tuple("/first", "osp.web.test.first", _pBundle->bundle()),
		std::make_tuple("/second", "osp.web.test.second", _pBundle->zipBundle())})
	{
		Poco::AutoPtr<Element> pElem(pDoc->createElement("extension"));
		pElem->setAttribute("point", WebServerExtensionPoint::EXTPOINT_HANDLER);
		pElem->setAttribute("path", path);
		pElem->setAttribute("class", "LibraryInstanceRequestHandlerFactory");
		pElem->setAttribute("library", library);
		pExtensionPoint->handleExtension(pBundle, pElem);
	}
	WebServerDispatcher::PathMap mappings;
	dispatcher.virtualPathMappings(mappings);
	const auto libraryInstance = [&mappings](const std::string& path)
		// Returns the answer of a handler of the path, which identifies the copy of the library.
	{
		ServerRequestStub request("127.0.0.1:50000", false);
		std::unique_ptr<Poco::Net::HTTPRequestHandler> pHandler(
			mappings.at(path).pFactory->createRequestHandler(request));
		pHandler->handleRequest(request, request.response());
		return request.response().body();
	};

	assertFalse (libraryInstance("/first/").empty());
	assertTrue (libraryInstance("/first/") != libraryInstance("/second/"));
}


#endif // OSPWEB_TEST_LIBRARY


void WebServerExtensionPointTest::testInvalidResponseFormat()
{
	try
	{
		parseCommon(*_pBundle, {{"path", "/app"}, {"responseFormat", "xml"}});
		fail("an invalid responseFormat value must be rejected");
	}
	catch (Poco::OSP::BundleException&)
	{
	}
}


CppUnit::Test* WebServerExtensionPointTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("WebServerExtensionPointTest");

	CppUnit_addTest(pSuite, WebServerExtensionPointTest, testCommonAttributes);
	CppUnit_addTest(pSuite, WebServerExtensionPointTest, testCommonAttributeDefaults);
	CppUnit_addTest(pSuite, WebServerExtensionPointTest, testInvalidAllowSpecialization);
	CppUnit_addTest(pSuite, WebServerExtensionPointTest, testInvalidResponseFormat);
#if defined(OSPWEB_TEST_LIBRARY)
	CppUnit_addTest(pSuite, WebServerExtensionPointTest, testRequestHandlerFactoryComesFromItsBundleLibrary);
#endif

	return pSuite;
}
