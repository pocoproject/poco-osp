//
// WebFilterExtensionPointTest.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "WebFilterExtensionPointTest.h"
#include "TemporaryBundle.h"
#include "ServerRequestStub.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "CppUnit/TestCase.h"
#include "CppUnit/Test.h"
#include "Poco/OSP/Web/WebServerDispatcher.h"
#include "Poco/OSP/Web/WebFilterExtensionPoint.h"
#include "Poco/OSP/Web/WebFilter.h"
#include "Poco/OSP/Web/WebFilterFactory.h"
#include "Poco/OSP/BundleEvent.h"
#include "Poco/OSP/Bundle.h"
#include "Poco/DOM/Document.h"
#include "Poco/DOM/Element.h"
#include "Poco/AutoPtr.h"
#include "Poco/Exception.h"
#include <memory>
#include <sstream>
#include <string>


using Poco::OSP::Web::WebServerDispatcher;
using Poco::OSP::Web::WebFilterExtensionPoint;
using Poco::OSP::Web::WebFilter;
using Poco::OSP::Web::WebFilterFactory;
using Poco::OSP::Bundle;
using Poco::OSP::BundleEvent;
using Poco::XML::Document;
using Poco::XML::Element;


#if defined(OSPWEB_TEST_LIBRARY)


namespace
{
	const std::string MEDIA_TYPE("text/x-test");
	const std::string LIBRARY("osp.web.test.filter");

	class TestDispatcher: public WebServerDispatcher
		/// Gives the tests access to the filters of the dispatcher.
	{
	public:
		using WebServerDispatcher::WebServerDispatcher;
		using WebServerDispatcher::findFilter;
	};

	class NoFilterFactory: public WebFilterFactory
	{
	public:
		[[nodiscard]] WebFilter* createFilter(const WebFilter::Args& /*args*/) override
		{
			return nullptr;
		}
	};

	void addFilterExtension(WebFilterExtensionPoint& extensionPoint, const Bundle::Ptr& pBundle,
		const std::string& mediaType = MEDIA_TYPE, const std::string& className = "TestFilterFactory",
		const std::string& library = LIBRARY)
		/// Adds a filter extension of the given bundle for the media type with the given filter factory of the library.
	{
		Poco::AutoPtr<Document> pDoc(new Document);
		Poco::AutoPtr<Element> pElem(pDoc->createElement("extension"));
		pElem->setAttribute("point", WebFilterExtensionPoint::EXTPOINT_FILTER);
		pElem->setAttribute("mediaType", mediaType);
		pElem->setAttribute("class", className);
		pElem->setAttribute("library", library);
		extensionPoint.handleExtension(pBundle, pElem);
	}
}


#endif // OSPWEB_TEST_LIBRARY


WebFilterExtensionPointTest::WebFilterExtensionPointTest(const std::string& name): CppUnit::TestCase(name)
{
}


WebFilterExtensionPointTest::~WebFilterExtensionPointTest() = default;


void WebFilterExtensionPointTest::setUp()
{
	_pBundle = std::make_unique<TemporaryBundle>("");
}


void WebFilterExtensionPointTest::tearDown()
{
	_pBundle.reset();
}


#if defined(OSPWEB_TEST_LIBRARY)


void WebFilterExtensionPointTest::testSecondFilterExtensionForMediaTypeIsRejected()
{
	_pBundle->installLibrary(LIBRARY, OSPWEB_TEST_LIBRARY);
	WebServerDispatcher::Config config;
	config.pContext = _pBundle->context();
	WebServerDispatcher dispatcher(config);
	Poco::AutoPtr<WebFilterExtensionPoint> pExtensionPoint(new WebFilterExtensionPoint(_pBundle->context(),
		&dispatcher));

	addFilterExtension(*pExtensionPoint, _pBundle->bundle());
	try
	{
		addFilterExtension(*pExtensionPoint, _pBundle->zipBundle());
		fail("a second filter extension for a media type must be rejected");
	}
	catch (Poco::ExistsException&)
	{
	}
}


void WebFilterExtensionPointTest::testFilterOfStoppedBundleIsRemoved()
{
	_pBundle->installLibrary(LIBRARY, OSPWEB_TEST_LIBRARY);
	WebServerDispatcher::Config config;
	config.pContext = _pBundle->context();
	WebServerDispatcher dispatcher(config);
	Poco::AutoPtr<WebFilterExtensionPoint> pExtensionPoint(new WebFilterExtensionPoint(_pBundle->context(),
		&dispatcher));
	addFilterExtension(*pExtensionPoint, _pBundle->bundle());

	BundleEvent stopped(_pBundle->bundle(), BundleEvent::EV_BUNDLE_STOPPED);
	_pBundle->events().bundleStopped(this, stopped);

	dispatcher.addFilter(MEDIA_TYPE, new NoFilterFactory, WebFilter::Args(), _pBundle->bundle());
}


void WebFilterExtensionPointTest::testFilterFactoryComesFromItsBundleLibrary()
{
	// Each bundle has its own copy of the library, which exports a class with the same name.
	_pBundle->installLibrary("osp.web.test.first", OSPWEB_TEST_LIBRARY);
	_pBundle->installLibrary("osp.web.test.second", OSPWEB_TEST_LIBRARY);
	WebServerDispatcher::Config config;
	config.pContext = _pBundle->context();
	TestDispatcher dispatcher(config);
	Poco::AutoPtr<WebFilterExtensionPoint> pExtensionPoint(new WebFilterExtensionPoint(_pBundle->context(),
		&dispatcher));
	addFilterExtension(*pExtensionPoint, _pBundle->bundle(), "text/x-first", "LibraryInstanceFilterFactory",
		"osp.web.test.first");
	addFilterExtension(*pExtensionPoint, _pBundle->zipBundle(), "text/x-second", "LibraryInstanceFilterFactory",
		"osp.web.test.second");
	const auto libraryInstance = [&dispatcher, this](const std::string& mediaType)
		// Returns the answer of a filter for the media type, which identifies the copy of the library.
	{
		ServerRequestStub request("127.0.0.1:50000", false);
		std::istringstream resource;
		dispatcher.findFilter(mediaType)->process(request, request.response(), "/resource", resource,
			_pBundle->bundle());
		return request.response().body();
	};

	assertFalse (libraryInstance("text/x-first").empty());
	assertTrue (libraryInstance("text/x-first") != libraryInstance("text/x-second"));
}


#endif // OSPWEB_TEST_LIBRARY


CppUnit::Test* WebFilterExtensionPointTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("WebFilterExtensionPointTest");

#if defined(OSPWEB_TEST_LIBRARY)
	CppUnit_addTest(pSuite, WebFilterExtensionPointTest, testSecondFilterExtensionForMediaTypeIsRejected);
	CppUnit_addTest(pSuite, WebFilterExtensionPointTest, testFilterOfStoppedBundleIsRemoved);
	CppUnit_addTest(pSuite, WebFilterExtensionPointTest, testFilterFactoryComesFromItsBundleLibrary);
#endif

	return pSuite;
}
