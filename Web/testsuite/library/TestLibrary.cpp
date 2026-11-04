//
// TestLibrary.cpp
//
// A library with a WebFilterFactory, for the tests of LibraryReference, and
// with a WebRequestHandlerFactory that stores a value in a web session.
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
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/ServiceFinder.h"
#include "Poco/Net/HTTPRequestHandler.h"
#include "Poco/Net/HTTPResponse.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Net/HTTPServerResponse.h"
#include "Poco/Any.h"
#include "Poco/ClassLibrary.h"
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
	static constexpr int SESSION_TIMEOUT_SECONDS = 600;

	Poco::OSP::BundleContext::Ptr _pContext;
};


SessionValueRequestHandler::SessionValueRequestHandler(Poco::OSP::BundleContext::Ptr pContext):
	_pContext(std::move(pContext))
{
}


void SessionValueRequestHandler::handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response)
{
	using Poco::OSP::Web::WebSessionManager;

	WebSessionManager::Ptr pManager = Poco::OSP::ServiceFinder::find<WebSessionManager>(_pContext);
	pManager->get("test", request, SESSION_TIMEOUT_SECONDS, _pContext)->set("value", Poco::Any(SessionValue{"value"}));
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


POCO_BEGIN_NAMED_MANIFEST(WebFilter, Poco::OSP::Web::WebFilterFactory)
	POCO_EXPORT_CLASS(TestFilterFactory)
POCO_END_MANIFEST


POCO_BEGIN_NAMED_MANIFEST(WebServer, Poco::OSP::Web::WebRequestHandlerFactory)
	POCO_EXPORT_CLASS(SessionValueRequestHandlerFactory)
POCO_END_MANIFEST
