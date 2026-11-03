//
// WebFilterExtensionPoint.h
//
// Library: OSP/Web
// Package: Web
// Module:  WebFilterExtensionPoint
//
// Definition of the WebFilterExtensionPoint class.
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef OSP_Web_WebFilterExtensionPoint_INCLUDED
#define OSP_Web_WebFilterExtensionPoint_INCLUDED


#include "Poco/OSP/Web/Web.h"
#include "Poco/OSP/Web/WebServerDispatcher.h"
#include "Poco/OSP/Web/WebFilterFactory.h"
#include "Poco/OSP/ExtensionPoint.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/ClassLoader.h"
#include "Poco/Mutex.h"
#include <map>
#include <memory>


namespace Poco::OSP::Web {


class OSPWeb_API WebFilterExtensionPoint: public ExtensionPoint
	/// WebServerExtension handles the "osp.web.server.filter" extensions point.
	///
	/// To register a Filter, for processing a resource before
	/// it's being sent as response:
	///
	///     <extension point="osp.web.server.filter"
	///                mediaType="media/type"
	///                class="MyFilterFactory"
	///                library="MyLib"/>
	///
	/// Note that the extension points are automatically removed if a bundle is stopped.
	/// Only one filter can be registered for a media type: the extension for a media type
	/// that already has a filter is rejected with a Poco::ExistsException.
	/// A library named by the library attribute stays loaded as long as a filter
	/// factory or a filter created from it exists.
	///
	/// The following attributes can be specified for "osp.web.server.filter":
	///    * mediaType:   The media type this filter applies to. Every resource having
	///                   this media type will be passed to the filter.
	///    * class: the class name of the filter factory. Factory classes of different bundles
	///      must have different fully qualified names (see WebServerExtensionPoint).
	///    * library: the name of the shared library containing the filter factory.
	///
	/// Any additional attributes are passed to the factory's createFilter() method.
{
public:
	WebFilterExtensionPoint() = delete;

	WebFilterExtensionPoint(BundleContext::Ptr pContext, WebServerDispatcher* pDispatcher);
		/// Creates the WebServerExtension. Expects a not null pointer to the Web which must be valid during the whole
		/// life-time of the extension object

	~WebFilterExtensionPoint() override;
		/// Destroys the WebServerExtension.

	// ExtensionPoint
	void handleExtension(Bundle::ConstPtr pBundle, Poco::XML::Element* pExtensionElem) override;

	static const std::string EXTPOINT_FILTER;

protected:
	void handleFilter(Bundle::ConstPtr pBundle, Poco::XML::Element* pExtensionElem);

	void onBundleStopped(const void* pSender, BundleEvent& ev);
		/// When a bundle is stopped, all of its filters are removed.

	static const std::string ATTR_CLASS;
	static const std::string ATTR_LIBRARY;
	static const std::string ATTR_MEDIATYPE;
	static const std::string MANIFEST_NAME;

private:
	using Loader = Poco::ClassLoader<WebFilterFactory>;
	using BundleFilterMap = std::multimap<Bundle::Ptr, std::string>;

	BundleContext::Ptr   _pContext;
	WebServerDispatcher* _pDispatcher;
	std::shared_ptr<Loader> _pLoader = std::make_shared<Loader>();
	BundleFilterMap      _bundleFilterMap;
	Poco::FastMutex      _mutex;
};


} // namespace Poco::OSP::Web


#endif // OSP_Web_WebFilterExtensionPoint_INCLUDED
