//
// LibraryRequestHandlerFactory.h
//
// Library: OSP/Web
// Package: Web
// Module:  LibraryRequestHandlerFactory
//
// Definition of the LibraryRequestHandlerFactory class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef OSP_Web_LibraryRequestHandlerFactory_INCLUDED
#define OSP_Web_LibraryRequestHandlerFactory_INCLUDED


#include "Poco/OSP/Web/Web.h"
#include "Poco/Net/HTTPRequestHandlerFactory.h"
#include <memory>


namespace Poco::OSP::Web {


class OSPWeb_API LibraryRequestHandlerFactory: public Poco::Net::HTTPRequestHandlerFactory
	/// A LibraryRequestHandlerFactory passes requests to a request handler
	/// factory created from a shared library, and keeps the library loaded
	/// as long as the LibraryRequestHandlerFactory or a request handler
	/// created by it exists.
	///
	/// WebServerExtensionPoint registers the request handler factories of
	/// bundles through this class, so that a request handler can complete
	/// its request when the bundle that provides it is stopped.
{
public:
	using LibraryPtr = std::shared_ptr<const void>;
		/// An object that keeps a library loaded while it exists.

	LibraryRequestHandlerFactory() = delete;

	LibraryRequestHandlerFactory(LibraryPtr pLibrary, std::unique_ptr<Poco::Net::HTTPRequestHandlerFactory> pFactory);
		/// Creates the LibraryRequestHandlerFactory for the given factory,
		/// which has been created from the library that pLibrary keeps loaded.

	~LibraryRequestHandlerFactory() override;
		/// Destroys the factory, then releases the library.

	[[nodiscard]] Poco::Net::HTTPRequestHandler* createRequestHandler(const Poco::Net::HTTPServerRequest& request) override;
		/// Returns the request handler that the factory creates for the request,
		/// or a null pointer if the factory creates none. The returned request
		/// handler keeps the library loaded until it is destroyed.

private:
	LibraryPtr _pLibrary;
	std::unique_ptr<Poco::Net::HTTPRequestHandlerFactory> _pFactory;
};


} // namespace Poco::OSP::Web


#endif // OSP_Web_LibraryRequestHandlerFactory_INCLUDED
