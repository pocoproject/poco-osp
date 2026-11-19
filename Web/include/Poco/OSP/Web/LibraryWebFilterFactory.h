//
// LibraryWebFilterFactory.h
//
// Library: OSP/Web
// Package: Web
// Module:  LibraryWebFilterFactory
//
// Definition of the LibraryWebFilterFactory class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef OSP_Web_LibraryWebFilterFactory_INCLUDED
#define OSP_Web_LibraryWebFilterFactory_INCLUDED


#include "Poco/OSP/Web/Web.h"
#include "Poco/OSP/Web/WebFilter.h"
#include "Poco/OSP/Web/WebFilterFactory.h"
#include <memory>


namespace Poco::OSP::Web {


class OSPWeb_API LibraryWebFilterFactory: public WebFilterFactory
	/// A LibraryWebFilterFactory passes filter requests to a WebFilterFactory
	/// created from a shared library, and keeps the library loaded as long as
	/// the LibraryWebFilterFactory or a WebFilter created by it exists.
	///
	/// WebFilterExtensionPoint registers the filter factories of bundles
	/// through this class, so that a filter can complete its response when
	/// the bundle that provides it is stopped.
{
public:
	using LibraryPtr = std::shared_ptr<const void>;
		/// An object that keeps a library loaded while it exists.

	LibraryWebFilterFactory() = delete;

	LibraryWebFilterFactory(LibraryPtr pLibrary, std::unique_ptr<WebFilterFactory> pFactory);
		/// Creates the LibraryWebFilterFactory for the given factory,
		/// which has been created from the library that pLibrary keeps loaded.

	~LibraryWebFilterFactory() override;
		/// Destroys the factory, then releases the library.

	[[nodiscard]] WebFilter* createFilter(const WebFilter::Args& args) override;
		/// Returns the WebFilter that the factory creates for the given arguments,
		/// or a null pointer if the factory creates none. The returned WebFilter
		/// keeps the library loaded until it is destroyed.

private:
	LibraryPtr _pLibrary;
	std::unique_ptr<WebFilterFactory> _pFactory;
};


} // namespace Poco::OSP::Web


#endif // OSP_Web_LibraryWebFilterFactory_INCLUDED
