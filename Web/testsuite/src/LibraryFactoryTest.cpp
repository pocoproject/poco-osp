//
// LibraryFactoryTest.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "LibraryFactoryTest.h"
#include "ServerRequestStub.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "CppUnit/TestCase.h"
#include "CppUnit/Test.h"
#include "Poco/OSP/Web/LibraryRequestHandlerFactory.h"
#include "Poco/OSP/Web/LibraryWebFilterFactory.h"
#include "Poco/OSP/Web/WebFilter.h"
#include "Poco/OSP/Web/WebFilterFactory.h"
#include "Poco/OSP/Bundle.h"
#include "Poco/Net/HTTPRequestHandler.h"
#include "Poco/Net/HTTPRequestHandlerFactory.h"
#if defined(OSPWEB_TEST_LIBRARY)
#include "LibraryReference.h"
#endif
#include <memory>
#include <sstream>
#include <string>
#include <istream>


using Poco::OSP::Web::LibraryRequestHandlerFactory;
using Poco::OSP::Web::LibraryWebFilterFactory;
using Poco::OSP::Web::WebFilter;
using Poco::OSP::Web::WebFilterFactory;
using Poco::Net::HTTPRequestHandler;
using Poco::Net::HTTPRequestHandlerFactory;
using Poco::Net::HTTPServerRequest;
using Poco::Net::HTTPServerResponse;


namespace
{
	class StatusRequestHandler: public HTTPRequestHandler
	{
	public:
		void handleRequest(HTTPServerRequest& request, HTTPServerResponse& response) override
		{
			response.setStatusAndReason(HTTPServerResponse::HTTP_ACCEPTED);
		}
	};

	class StatusRequestHandlerFactory: public HTTPRequestHandlerFactory
		/// Creates a StatusRequestHandler, or none if created with createHandlers false.
	{
	public:
		explicit StatusRequestHandlerFactory(bool createHandlers):
			_createHandlers(createHandlers)
		{
		}

		HTTPRequestHandler* createRequestHandler(const HTTPServerRequest& request) override
		{
			return _createHandlers ? new StatusRequestHandler : nullptr;
		}

	private:
		bool _createHandlers;
	};

	class StatusFilter: public WebFilter
	{
	public:
		void process(HTTPServerRequest& request, HTTPServerResponse& response, const std::string& path, std::istream& resourceStream, Poco::OSP::Bundle::ConstPtr pBundle) override
		{
			response.setStatusAndReason(HTTPServerResponse::HTTP_ACCEPTED);
		}
	};

	class StatusFilterFactory: public WebFilterFactory
		/// Creates a StatusFilter, or none if created with createFilters false.
	{
	public:
		explicit StatusFilterFactory(bool createFilters):
			_createFilters(createFilters)
		{
		}

		WebFilter* createFilter(const WebFilter::Args& args) override
		{
			return _createFilters ? new StatusFilter : nullptr;
		}

	private:
		bool _createFilters;
	};

	const std::string CLIENT_ADDRESS("127.0.0.1:50000");
}


LibraryFactoryTest::LibraryFactoryTest(const std::string& name): CppUnit::TestCase(name)
{
}


LibraryFactoryTest::~LibraryFactoryTest() = default;


void LibraryFactoryTest::setUp()
{
}


void LibraryFactoryTest::tearDown()
{
}


void LibraryFactoryTest::testRequestHandlerKeepsLibraryLoaded()
{
	auto pLibrary = std::make_shared<int>(0);
	std::weak_ptr<int> library(pLibrary);
	auto pFactory = std::make_unique<LibraryRequestHandlerFactory>(pLibrary, std::make_unique<StatusRequestHandlerFactory>(true));
	pLibrary.reset();
	ServerRequestStub request(CLIENT_ADDRESS, false);

	std::unique_ptr<HTTPRequestHandler> pHandler(pFactory->createRequestHandler(request));
	pFactory.reset();

	assertFalse (library.expired());
	pHandler->handleRequest(request, request.response());
	assertTrue (request.response().getStatus() == HTTPServerResponse::HTTP_ACCEPTED);
	pHandler.reset();
	assertTrue (library.expired());
}


void LibraryFactoryTest::testRequestHandlerFactoryWithoutHandler()
{
	auto pLibrary = std::make_shared<int>(0);
	LibraryRequestHandlerFactory factory(pLibrary, std::make_unique<StatusRequestHandlerFactory>(false));
	ServerRequestStub request(CLIENT_ADDRESS, false);

	std::unique_ptr<HTTPRequestHandler> pHandler(factory.createRequestHandler(request));

	assertTrue (pHandler == nullptr);
}


void LibraryFactoryTest::testFilterKeepsLibraryLoaded()
{
	auto pLibrary = std::make_shared<int>(0);
	std::weak_ptr<int> library(pLibrary);
	auto pFactory = std::make_unique<LibraryWebFilterFactory>(pLibrary, std::make_unique<StatusFilterFactory>(true));
	pLibrary.reset();
	ServerRequestStub request(CLIENT_ADDRESS, false);
	std::istringstream resource("content");

	std::unique_ptr<WebFilter> pFilter(pFactory->createFilter(WebFilter::Args()));
	pFactory.reset();

	assertFalse (library.expired());
	pFilter->process(request, request.response(), "/index.html", resource, nullptr);
	assertTrue (request.response().getStatus() == HTTPServerResponse::HTTP_ACCEPTED);
	pFilter.reset();
	assertTrue (library.expired());
}


void LibraryFactoryTest::testFilterFactoryWithoutFilter()
{
	auto pLibrary = std::make_shared<int>(0);
	LibraryWebFilterFactory factory(pLibrary, std::make_unique<StatusFilterFactory>(false));

	std::unique_ptr<WebFilter> pFilter(factory.createFilter(WebFilter::Args()));

	assertTrue (pFilter == nullptr);
}


#if defined(OSPWEB_TEST_LIBRARY)


void LibraryFactoryTest::testLibraryStaysLoadedWhileReferenced()
{
	using Reference = Poco::OSP::Web::LibraryReference<WebFilterFactory>;
	auto pLoader = std::make_shared<Reference::Loader>();
	const std::string path(OSPWEB_TEST_LIBRARY);

	auto pFirst = std::make_unique<Reference>(pLoader, path, "WebFilter");
	auto pSecond = std::make_unique<Reference>(pLoader, path, "WebFilter");
	assertTrue (pLoader->isLibraryLoaded(path));
	std::unique_ptr<WebFilterFactory> pFactory(pLoader->create("TestFilterFactory"));
	assertTrue (pFactory != nullptr);
	pFactory.reset();

	pFirst.reset();
	assertTrue (pLoader->isLibraryLoaded(path));
	pSecond.reset();
	assertFalse (pLoader->isLibraryLoaded(path));
}


#endif // OSPWEB_TEST_LIBRARY


CppUnit::Test* LibraryFactoryTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("LibraryFactoryTest");

	CppUnit_addTest(pSuite, LibraryFactoryTest, testRequestHandlerKeepsLibraryLoaded);
	CppUnit_addTest(pSuite, LibraryFactoryTest, testRequestHandlerFactoryWithoutHandler);
	CppUnit_addTest(pSuite, LibraryFactoryTest, testFilterKeepsLibraryLoaded);
	CppUnit_addTest(pSuite, LibraryFactoryTest, testFilterFactoryWithoutFilter);
#if defined(OSPWEB_TEST_LIBRARY)
	CppUnit_addTest(pSuite, LibraryFactoryTest, testLibraryStaysLoadedWhileReferenced);
#endif

	return pSuite;
}
