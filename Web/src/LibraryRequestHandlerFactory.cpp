//
// LibraryRequestHandlerFactory.cpp
//
// Library: OSP/Web
// Package: Web
// Module:  LibraryRequestHandlerFactory
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "Poco/OSP/Web/LibraryRequestHandlerFactory.h"
#include "Poco/Net/HTTPRequestHandler.h"
#include "Poco/Net/HTTPRequestHandlerFactory.h"
#include <memory>
#include <utility>


using Poco::Net::HTTPRequestHandler;
using Poco::Net::HTTPServerRequest;
using Poco::Net::HTTPServerResponse;


namespace Poco::OSP::Web {


namespace
{
	class LibraryRequestHandler: public HTTPRequestHandler
		/// Passes the request to a request handler created from a library
		/// and keeps the library loaded until the request handler is destroyed.
	{
	public:
		LibraryRequestHandler() = delete;

		LibraryRequestHandler(LibraryRequestHandlerFactory::LibraryPtr pLibrary, std::unique_ptr<HTTPRequestHandler> pHandler):
			_pLibrary(std::move(pLibrary)),
			_pHandler(std::move(pHandler))
		{
		}

		~LibraryRequestHandler() override = default;

		LibraryRequestHandler(const LibraryRequestHandler&) = delete;
		LibraryRequestHandler(LibraryRequestHandler&&) = delete;
		LibraryRequestHandler& operator = (const LibraryRequestHandler&) = delete;
		LibraryRequestHandler& operator = (LibraryRequestHandler&&) = delete;

		void handleRequest(HTTPServerRequest& request, HTTPServerResponse& response) override
		{
			_pHandler->handleRequest(request, response);
		}

	private:
		// Declared first, so that the library is released after the handler is destroyed.
		LibraryRequestHandlerFactory::LibraryPtr _pLibrary;
		std::unique_ptr<HTTPRequestHandler> _pHandler;
	};
}


LibraryRequestHandlerFactory::LibraryRequestHandlerFactory(LibraryPtr pLibrary, std::unique_ptr<Poco::Net::HTTPRequestHandlerFactory> pFactory):
	_pLibrary(std::move(pLibrary)),
	_pFactory(std::move(pFactory))
{
}


LibraryRequestHandlerFactory::~LibraryRequestHandlerFactory() = default;


HTTPRequestHandler* LibraryRequestHandlerFactory::createRequestHandler(const HTTPServerRequest& request)
{
	std::unique_ptr<HTTPRequestHandler> pHandler(_pFactory->createRequestHandler(request));
	if (pHandler == nullptr) return nullptr;

	return new LibraryRequestHandler(_pLibrary, std::move(pHandler));
}


} // namespace Poco::OSP::Web
