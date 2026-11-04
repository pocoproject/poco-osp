//
// TestLibrary.cpp
//
// Request handler and filter factories that the Web tests load from a shared library.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "Poco/OSP/Web/WebFilter.h"
#include "Poco/OSP/Web/WebFilterFactory.h"
#include "Poco/OSP/Web/WebRequestHandlerFactory.h"
#include "Poco/OSP/Web/WebSessionManager.h"
#include "Poco/OSP/Bundle.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/ServiceFinder.h"
#include "Poco/Net/HTTPRequestHandler.h"
#include "Poco/Net/HTTPResponse.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Net/HTTPServerResponse.h"
#include "Poco/Any.h"
#include "Poco/ClassLibrary.h"
#include <istream>
#include <sstream>
#include <string>
#include <utility>


class TestFilterFactory: public Poco::OSP::Web::WebFilterFactory
	/// Creates no filters.
{
public:
	TestFilterFactory() = default;

	~TestFilterFactory() override = default;

	TestFilterFactory(const TestFilterFactory&) = delete;
	TestFilterFactory(TestFilterFactory&&) = delete;
	TestFilterFactory& operator = (const TestFilterFactory&) = delete;
	TestFilterFactory& operator = (TestFilterFactory&&) = delete;

	[[nodiscard]] Poco::OSP::Web::WebFilter* createFilter(const Poco::OSP::Web::WebFilter::Args& args) override;
};


Poco::OSP::Web::WebFilter* TestFilterFactory::createFilter([[maybe_unused]] const Poco::OSP::Web::WebFilter::Args& args)
{
	return nullptr;
}


struct SessionValue
	/// A type that only this library defines, so that the Poco::Any holder
	/// of a SessionValue has its virtual functions in this library.
{
	std::string text;
};


namespace
{
	constexpr int SESSION_TIMEOUT_SECONDS = 600;

	void storeSessionValue(const Poco::OSP::BundleContext::Ptr& pContext, Poco::Net::HTTPServerRequest& request)
		/// Stores a SessionValue under the key "value" in the session "test" of the request.
	{
		using Poco::OSP::Web::WebSessionManager;

		WebSessionManager::Ptr pManager = Poco::OSP::ServiceFinder::find<WebSessionManager>(pContext);
		pManager->get("test", request, SESSION_TIMEOUT_SECONDS, pContext)->set("value",
			Poco::Any(SessionValue{"value"}));
	}
}


class SessionValueRequestHandler: public Poco::Net::HTTPRequestHandler
	/// Stores a SessionValue under the key "value" in the session "test" of the request.
{
public:
	SessionValueRequestHandler() = delete;

	explicit SessionValueRequestHandler(Poco::OSP::BundleContext::Ptr pContext);

	~SessionValueRequestHandler() override = default;

	SessionValueRequestHandler(const SessionValueRequestHandler&) = delete;
	SessionValueRequestHandler(SessionValueRequestHandler&&) = delete;
	SessionValueRequestHandler& operator = (const SessionValueRequestHandler&) = delete;
	SessionValueRequestHandler& operator = (SessionValueRequestHandler&&) = delete;

	void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) override;

private:
	Poco::OSP::BundleContext::Ptr _pContext;
};


SessionValueRequestHandler::SessionValueRequestHandler(Poco::OSP::BundleContext::Ptr pContext):
	_pContext(std::move(pContext))
{
}


void SessionValueRequestHandler::handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response)
{
	storeSessionValue(_pContext, request);
	response.setStatusAndReason(Poco::Net::HTTPResponse::HTTP_NO_CONTENT);
	response.send();
}


class SessionValueRequestHandlerFactory: public Poco::OSP::Web::WebRequestHandlerFactory
	/// Creates SessionValueRequestHandler objects.
{
public:
	SessionValueRequestHandlerFactory() = default;

	~SessionValueRequestHandlerFactory() override = default;

	SessionValueRequestHandlerFactory(const SessionValueRequestHandlerFactory&) = delete;
	SessionValueRequestHandlerFactory(SessionValueRequestHandlerFactory&&) = delete;
	SessionValueRequestHandlerFactory& operator = (const SessionValueRequestHandlerFactory&) = delete;
	SessionValueRequestHandlerFactory& operator = (SessionValueRequestHandlerFactory&&) = delete;

	[[nodiscard]] Poco::Net::HTTPRequestHandler* createRequestHandler(const Poco::Net::HTTPServerRequest& request) override;
};


Poco::Net::HTTPRequestHandler* SessionValueRequestHandlerFactory::createRequestHandler([[maybe_unused]] const Poco::Net::HTTPServerRequest& request)
{
	return new SessionValueRequestHandler(context());
}


class SessionValueFilter: public Poco::OSP::Web::WebFilter
	/// Stores a SessionValue under the key "value" in the session "test" of the request.
{
public:
	SessionValueFilter() = delete;

	explicit SessionValueFilter(Poco::OSP::BundleContext::Ptr pContext);

	~SessionValueFilter() override = default;

	SessionValueFilter(const SessionValueFilter&) = delete;
	SessionValueFilter(SessionValueFilter&&) = delete;
	SessionValueFilter& operator = (const SessionValueFilter&) = delete;
	SessionValueFilter& operator = (SessionValueFilter&&) = delete;

	void process(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response,
		const std::string& path, std::istream& resourceStream, Poco::OSP::Bundle::ConstPtr pBundle) override;

private:
	Poco::OSP::BundleContext::Ptr _pContext;
};


SessionValueFilter::SessionValueFilter(Poco::OSP::BundleContext::Ptr pContext):
	_pContext(std::move(pContext))
{
}


void SessionValueFilter::process(Poco::Net::HTTPServerRequest& request,
	[[maybe_unused]] Poco::Net::HTTPServerResponse& response, [[maybe_unused]] const std::string& path,
	[[maybe_unused]] std::istream& resourceStream, [[maybe_unused]] Poco::OSP::Bundle::ConstPtr pBundle)
{
	storeSessionValue(_pContext, request);
}


class SessionValueFilterFactory: public Poco::OSP::Web::WebFilterFactory
	/// Creates SessionValueFilter objects.
{
public:
	SessionValueFilterFactory() = default;

	~SessionValueFilterFactory() override = default;

	SessionValueFilterFactory(const SessionValueFilterFactory&) = delete;
	SessionValueFilterFactory(SessionValueFilterFactory&&) = delete;
	SessionValueFilterFactory& operator = (const SessionValueFilterFactory&) = delete;
	SessionValueFilterFactory& operator = (SessionValueFilterFactory&&) = delete;

	[[nodiscard]] Poco::OSP::Web::WebFilter* createFilter(const Poco::OSP::Web::WebFilter::Args& args) override;
};


Poco::OSP::Web::WebFilter* SessionValueFilterFactory::createFilter(
	[[maybe_unused]] const Poco::OSP::Web::WebFilter::Args& args)
{
	return new SessionValueFilter(context());
}


namespace
{
	const int libraryInstance = 0;
		/// Each loaded copy of the library has its own object, whose address identifies the copy.
}


class LibraryInstanceRequestHandler: public Poco::Net::HTTPRequestHandler
	/// Answers with the address of libraryInstance in the copy of the library that created it.
{
public:
	LibraryInstanceRequestHandler() = default;

	~LibraryInstanceRequestHandler() override = default;

	LibraryInstanceRequestHandler(const LibraryInstanceRequestHandler&) = delete;
	LibraryInstanceRequestHandler(LibraryInstanceRequestHandler&&) = delete;
	LibraryInstanceRequestHandler& operator = (const LibraryInstanceRequestHandler&) = delete;
	LibraryInstanceRequestHandler& operator = (LibraryInstanceRequestHandler&&) = delete;

	void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) override;
};


void LibraryInstanceRequestHandler::handleRequest([[maybe_unused]] Poco::Net::HTTPServerRequest& request,
	Poco::Net::HTTPServerResponse& response)
{
	std::ostringstream body;
	body << static_cast<const void*>(&libraryInstance);
	response.send() << body.str();
}


class LibraryInstanceRequestHandlerFactory: public Poco::OSP::Web::WebRequestHandlerFactory
	/// Creates LibraryInstanceRequestHandler objects.
{
public:
	LibraryInstanceRequestHandlerFactory() = default;

	~LibraryInstanceRequestHandlerFactory() override = default;

	LibraryInstanceRequestHandlerFactory(const LibraryInstanceRequestHandlerFactory&) = delete;
	LibraryInstanceRequestHandlerFactory(LibraryInstanceRequestHandlerFactory&&) = delete;
	LibraryInstanceRequestHandlerFactory& operator = (const LibraryInstanceRequestHandlerFactory&) = delete;
	LibraryInstanceRequestHandlerFactory& operator = (LibraryInstanceRequestHandlerFactory&&) = delete;

	[[nodiscard]] Poco::Net::HTTPRequestHandler* createRequestHandler(
		const Poco::Net::HTTPServerRequest& request) override;
};


Poco::Net::HTTPRequestHandler* LibraryInstanceRequestHandlerFactory::createRequestHandler(
	[[maybe_unused]] const Poco::Net::HTTPServerRequest& request)
{
	return new LibraryInstanceRequestHandler;
}


class LibraryInstanceFilter: public Poco::OSP::Web::WebFilter
	/// Answers with the address of libraryInstance in the copy of the library that created it.
{
public:
	LibraryInstanceFilter() = default;

	~LibraryInstanceFilter() override = default;

	LibraryInstanceFilter(const LibraryInstanceFilter&) = delete;
	LibraryInstanceFilter(LibraryInstanceFilter&&) = delete;
	LibraryInstanceFilter& operator = (const LibraryInstanceFilter&) = delete;
	LibraryInstanceFilter& operator = (LibraryInstanceFilter&&) = delete;

	void process(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response,
		const std::string& path, std::istream& resourceStream, Poco::OSP::Bundle::ConstPtr pBundle) override;
};


void LibraryInstanceFilter::process([[maybe_unused]] Poco::Net::HTTPServerRequest& request,
	Poco::Net::HTTPServerResponse& response, [[maybe_unused]] const std::string& path,
	[[maybe_unused]] std::istream& resourceStream, [[maybe_unused]] Poco::OSP::Bundle::ConstPtr pBundle)
{
	std::ostringstream body;
	body << static_cast<const void*>(&libraryInstance);
	response.send() << body.str();
}


class LibraryInstanceFilterFactory: public Poco::OSP::Web::WebFilterFactory
	/// Creates LibraryInstanceFilter objects.
{
public:
	LibraryInstanceFilterFactory() = default;

	~LibraryInstanceFilterFactory() override = default;

	LibraryInstanceFilterFactory(const LibraryInstanceFilterFactory&) = delete;
	LibraryInstanceFilterFactory(LibraryInstanceFilterFactory&&) = delete;
	LibraryInstanceFilterFactory& operator = (const LibraryInstanceFilterFactory&) = delete;
	LibraryInstanceFilterFactory& operator = (LibraryInstanceFilterFactory&&) = delete;

	[[nodiscard]] Poco::OSP::Web::WebFilter* createFilter(const Poco::OSP::Web::WebFilter::Args& args) override;
};


Poco::OSP::Web::WebFilter* LibraryInstanceFilterFactory::createFilter(
	[[maybe_unused]] const Poco::OSP::Web::WebFilter::Args& args)
{
	return new LibraryInstanceFilter;
}


POCO_BEGIN_NAMED_MANIFEST(WebFilter, Poco::OSP::Web::WebFilterFactory)
	POCO_EXPORT_CLASS(TestFilterFactory)
	POCO_EXPORT_CLASS(SessionValueFilterFactory)
	POCO_EXPORT_CLASS(LibraryInstanceFilterFactory)
POCO_END_MANIFEST


POCO_BEGIN_NAMED_MANIFEST(WebServer, Poco::OSP::Web::WebRequestHandlerFactory)
	POCO_EXPORT_CLASS(SessionValueRequestHandlerFactory)
	POCO_EXPORT_CLASS(LibraryInstanceRequestHandlerFactory)
POCO_END_MANIFEST
