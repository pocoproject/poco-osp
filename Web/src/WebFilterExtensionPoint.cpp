//
// WebFilterExtensionPoint.cpp
//
// Library: OSP/Web
// Package: Web
// Module:  WebFilterExtensionPoint
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#include "Poco/OSP/Web/WebFilterExtensionPoint.h"
#include "Poco/OSP/Web/WebServerDispatcher.h"
#include "Poco/OSP/Web/LibraryWebFilterFactory.h"
#include "Poco/OSP/Web/WebFilterFactory.h"
#include "LibraryReference.h"
#include "Poco/OSP/OSPException.h"
#include "Poco/OSP/ExtensionPointService.h"
#include "Poco/OSP/BundleEvents.h"
#include "Poco/DOM/NamedNodeMap.h"
#include "Poco/StringTokenizer.h"
#include "Poco/String.h"
#include "Poco/Delegate.h"
#include "Poco/SharedPtr.h"
#include <memory>
#include <utility>


using Poco::FastMutex;
using Poco::Delegate;


namespace Poco {
namespace OSP {
namespace Web {


const std::string WebFilterExtensionPoint::EXTPOINT_FILTER("osp.web.server.filter");
const std::string WebFilterExtensionPoint::ATTR_CLASS("class");
const std::string WebFilterExtensionPoint::ATTR_LIBRARY("library");
const std::string WebFilterExtensionPoint::ATTR_MEDIATYPE("mediaType");
const std::string WebFilterExtensionPoint::MANIFEST_NAME("WebFilter");


WebFilterExtensionPoint::WebFilterExtensionPoint(BundleContext::Ptr pContext, WebServerDispatcher* pDispatcher):
	_pContext(pContext),
	_pDispatcher(pDispatcher)
{
	poco_check_ptr (_pDispatcher);
	
	_pContext->events().bundleStopped += Delegate<WebFilterExtensionPoint, BundleEvent>(this, &WebFilterExtensionPoint::onBundleStopped);
}


WebFilterExtensionPoint::~WebFilterExtensionPoint()
{
	try
	{
		_pContext->events().bundleStopped -= Delegate<WebFilterExtensionPoint, BundleEvent>(this, &WebFilterExtensionPoint::onBundleStopped);
	}
	catch (...)
	{
		poco_unexpected();
	}
}


void WebFilterExtensionPoint::handleExtension(Bundle::ConstPtr pBundle, Poco::XML::Element* pExtensionElem)
{
	FastMutex::ScopedLock lock(_mutex);

	if (pExtensionElem)
	{
		const Poco::XML::XMLString& extPoint = pExtensionElem->getAttribute(ExtensionPointService::POINT_ATTR);
		if (extPoint == EXTPOINT_FILTER)
			handleFilter(pBundle, pExtensionElem);
		else
			throw BundleException("Failed to parse extension point");
	}
}


void WebFilterExtensionPoint::handleFilter(Bundle::ConstPtr pBundle, Poco::XML::Element* pExtensionElem)
{
	const std::string& mediaType = pExtensionElem->getAttribute(ATTR_MEDIATYPE);
	const std::string& className = pExtensionElem->getAttribute(ATTR_CLASS);	
	std::string library = pExtensionElem->getAttribute(ATTR_LIBRARY);
	if (library.empty())
		library = pBundle->symbolicName();
	
	WebFilter::Args args;
	Poco::AutoPtr<Poco::XML::NamedNodeMap> pAttrs = pExtensionElem->attributes();
	for (unsigned long i = 0; i < pAttrs->length(); i++)
	{
		args[pAttrs->item(i)->nodeName()] = pAttrs->item(i)->getNodeValue();
	}

	std::string libraryPath = _pContext->pathForLibrary(library);
	LibraryWebFilterFactory::LibraryPtr pLibrary = std::make_shared<LibraryReference<WebFilterFactory>>(_pLoader, libraryPath, MANIFEST_NAME);
	std::unique_ptr<WebFilterFactory> pFactory(_pLoader->create(className));
	pFactory->init(_pContext->contextForBundle(pBundle));
	
	_pDispatcher->addFilter(mediaType, Poco::makeShared<LibraryWebFilterFactory>(pLibrary, std::move(pFactory)), args);
	_bundleFilterMap.insert(BundleFilterMap::value_type(pBundle, mediaType));
}


void WebFilterExtensionPoint::onBundleStopped(const void* pSender, BundleEvent& ev)
{
	FastMutex::ScopedLock lock(_mutex);

	Bundle::ConstPtr pBundle = ev.bundle();

	BundleFilterMap::iterator itBFM = _bundleFilterMap.find(pBundle);
	while (itBFM != _bundleFilterMap.end() && itBFM->first == pBundle)
	{
		_pDispatcher->removeFilter(itBFM->second);
		++itBFM;
	}
	_bundleFilterMap.erase(pBundle);
}


} } } // namespace Poco::OSP::Web
