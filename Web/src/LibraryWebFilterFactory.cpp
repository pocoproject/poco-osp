//
// LibraryWebFilterFactory.cpp
//
// Library: OSP/Web
// Package: Web
// Module:  LibraryWebFilterFactory
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "Poco/OSP/Web/LibraryWebFilterFactory.h"
#include "Poco/OSP/Web/WebFilter.h"
#include "Poco/OSP/Web/WebFilterFactory.h"
#include "Poco/OSP/Bundle.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Net/HTTPServerResponse.h"
#include <istream>
#include <memory>
#include <string>
#include <utility>


namespace Poco::OSP::Web {


namespace
{
	class LibraryWebFilter: public WebFilter
		/// Passes the resource to a WebFilter created from a library and
		/// keeps the library loaded until the WebFilter is destroyed.
	{
	public:
		LibraryWebFilter() = delete;

		LibraryWebFilter(LibraryWebFilterFactory::LibraryPtr pLibrary, std::unique_ptr<WebFilter> pFilter):
			_pLibrary(std::move(pLibrary)),
			_pFilter(std::move(pFilter))
		{
		}

		~LibraryWebFilter() override = default;

		LibraryWebFilter(const LibraryWebFilter&) = delete;
		LibraryWebFilter(LibraryWebFilter&&) = delete;
		LibraryWebFilter& operator = (const LibraryWebFilter&) = delete;
		LibraryWebFilter& operator = (LibraryWebFilter&&) = delete;

		void process(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response, const std::string& path, std::istream& resourceStream, Poco::OSP::Bundle::ConstPtr pBundle) override
		{
			_pFilter->process(request, response, path, resourceStream, pBundle);
		}

	private:
		// Declared first, so that the library is released after the filter is destroyed.
		LibraryWebFilterFactory::LibraryPtr _pLibrary;
		std::unique_ptr<WebFilter> _pFilter;
	};
}


LibraryWebFilterFactory::LibraryWebFilterFactory(LibraryPtr pLibrary, std::unique_ptr<WebFilterFactory> pFactory):
	_pLibrary(std::move(pLibrary)),
	_pFactory(std::move(pFactory))
{
}


LibraryWebFilterFactory::~LibraryWebFilterFactory() = default;


WebFilter* LibraryWebFilterFactory::createFilter(const WebFilter::Args& args)
{
	std::unique_ptr<WebFilter> pFilter(_pFactory->createFilter(args));
	if (pFilter == nullptr) return nullptr;

	return new LibraryWebFilter(_pLibrary, std::move(pFilter));
}


} // namespace Poco::OSP::Web
