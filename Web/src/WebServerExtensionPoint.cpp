//
// WebServerExtensionPoint.cpp
//
// Library: OSP/Web
// Package: Web
// Module:  WebServerExtensionPoint
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "Poco/OSP/Web/WebServerExtensionPoint.h"
#include "Poco/OSP/Web/WebServerDispatcher.h"
#include "Poco/OSP/Web/LibraryRequestHandlerFactory.h"
#include "Poco/OSP/Web/WebRequestHandlerFactory.h"
#include "LibraryReference.h"
#include "Poco/OSP/OSPException.h"
#include "Poco/OSP/Configuration.h"
#include "Poco/OSP/ExtensionPointService.h"
#include "Poco/StringTokenizer.h"
#include "Poco/String.h"
#include "Poco/OSP/PreferencesService.h"
#include "Poco/OSP/ServiceRef.h"
#include "Poco/OSP/ServiceRegistry.h"
#include "Poco/Util/AbstractConfiguration.h"
#include "Poco/SharedPtr.h"
#include "Poco/Bugcheck.h"
#include <algorithm>
#include <string>
#include <memory>
#include <string_view>
#include <utility>


using Poco::FastMutex;


namespace Poco::OSP::Web {


namespace
{
	std::string expandAttribute(
		std::string_view value,
		const Configuration* pAppConfig,
		const Poco::Util::AbstractConfiguration& bundleProperties)
		/// Replaces each reference to a key that starts with "osp." and is in the application
		/// configuration with the key's value there, then expands the other references with
		/// the bundle properties.
	{
		static constexpr std::string_view APPLICATION_KEY_PREFIX("osp.");

		std::string result;
		std::string_view::size_type pos = 0;
		while (pAppConfig != nullptr && pos < value.size())
		{
			const std::string_view::size_type begin = value.find("${", pos);
			if (begin == std::string_view::npos) break;
			const std::string_view::size_type end = value.find('}', begin);
			if (end == std::string_view::npos) break;
			const std::string key(value.substr(begin + 2, std::min(end, value.find(":-", begin)) - begin - 2));
			result.append(value, pos, begin - pos);
			if (key.starts_with(APPLICATION_KEY_PREFIX) && pAppConfig->has(key))
				result += pAppConfig->getString(key);
			else
				result.append(value, begin, end + 1 - begin);
			pos = end + 1;
		}
		result.append(value.substr(pos));
		return bundleProperties.expand(result);
	}
}


const std::string WebServerExtensionPoint::EXTPOINT_DIRECTORY("osp.web.server.directory");
const std::string WebServerExtensionPoint::EXTPOINT_HANDLER("osp.web.server.requesthandler");
const std::string WebServerExtensionPoint::ATTR_PATH("path");
const std::string WebServerExtensionPoint::ATTR_PATTERN("pattern");
const std::string WebServerExtensionPoint::ATTR_METHODS("methods");
const std::string WebServerExtensionPoint::ATTR_DESCRIPTION("description");
const std::string WebServerExtensionPoint::ATTR_SECURE("secure");
const std::string WebServerExtensionPoint::ATTR_REALM("realm");
const std::string WebServerExtensionPoint::ATTR_PERMISSION("permission");
const std::string WebServerExtensionPoint::ATTR_AUTHMETHODS("authMethods");
const std::string WebServerExtensionPoint::ATTR_SESSION("session");
const std::string WebServerExtensionPoint::ATTR_CSRFPROTECTION("csrfProtection");
const std::string WebServerExtensionPoint::ATTR_CSRFTOKENHEADER("csrfTokenHeader");
const std::string WebServerExtensionPoint::ATTR_ALLOWWEBSOCKETORIGIN("allowWebSocketOrigin");
const std::string WebServerExtensionPoint::ATTR_RESOURCE("resource");
const std::string WebServerExtensionPoint::ATTR_CLASS("class");
const std::string WebServerExtensionPoint::ATTR_LIBRARY("library");
const std::string WebServerExtensionPoint::ATTR_ALLOWSPECIALIZATION("allowSpecialization");
const std::string WebServerExtensionPoint::ATTR_INDEX("index");
const std::string WebServerExtensionPoint::ATTR_HIDDEN("hidden");
const std::string WebServerExtensionPoint::ATTR_CACHE("cache");
const std::string WebServerExtensionPoint::ATTR_EXACTMATCH("exactMatch");
const std::string WebServerExtensionPoint::ATTR_CORS("cors");
const std::string WebServerExtensionPoint::ATTR_ALLOWORIGIN("allowOrigin");
const std::string WebServerExtensionPoint::ATTR_ALLOWMETHODS("allowMethods");
const std::string WebServerExtensionPoint::ATTR_ALLOWHEADERS("allowHeaders");
const std::string WebServerExtensionPoint::ATTR_ALLOWCREDENTIALS("allowCredentials");
const std::string WebServerExtensionPoint::ATTR_RESPONSEFORMAT("responseFormat");
const std::string WebServerExtensionPoint::ATTR_REDIRECTON401("redirectOn401");
const std::string WebServerExtensionPoint::MANIFEST_NAME("WebServer");


WebServerExtensionPoint::WebServerExtensionPoint(BundleContext::Ptr pContext, WebServerDispatcher* pDispatcher):
	_pContext(pContext),
	_pDispatcher(pDispatcher)
{
	poco_check_ptr (_pDispatcher);
}


WebServerExtensionPoint::~WebServerExtensionPoint() = default;


void WebServerExtensionPoint::handleExtension(Bundle::ConstPtr pBundle, Poco::XML::Element* pExtensionElem)
{
	FastMutex::ScopedLock lock(_mutex);

	if (pExtensionElem != nullptr)
	{
		const Poco::XML::XMLString& extPoint = pExtensionElem->getAttribute(ExtensionPointService::POINT_ATTR);
		if (extPoint == EXTPOINT_DIRECTORY)
			handleDirectory(pBundle, pExtensionElem);
		else if (extPoint == EXTPOINT_HANDLER)
			handleRequestHandler(pBundle, pExtensionElem);
		else
			throw BundleException("Failed to parse extension point");
	}
}


void WebServerExtensionPoint::handleDirectory(Bundle::ConstPtr pBundle, Poco::XML::Element* pExtensionElem)
{
	WebServerDispatcher::VirtualPath vPath;
	handleCommon(pBundle, pExtensionElem, vPath);
	vPath.resource  = pExtensionElem->getAttribute(ATTR_RESOURCE);
	vPath.indexPage = pExtensionElem->getAttribute(ATTR_INDEX);

	_pDispatcher->addVirtualPath(vPath);
}


void WebServerExtensionPoint::handleRequestHandler(Bundle::ConstPtr pBundle, Poco::XML::Element* pExtensionElem)
{
	WebServerDispatcher::VirtualPath vPath;
	handleCommon(pBundle, pExtensionElem, vPath);

	const Configuration::Ptr pAppConfig = applicationConfiguration();
	const Poco::Util::AbstractConfiguration& bundleProperties = pBundle->properties();
	std::string pattern = expandAttribute(pExtensionElem->getAttribute(ATTR_PATTERN), pAppConfig, bundleProperties);
	if (!pattern.empty())
	{
		vPath.pPattern = new Poco::RegularExpression(pattern, Poco::RegularExpression::RE_ANCHORED);
		vPath.path     = pattern;
	}

	std::string methods = expandAttribute(pExtensionElem->getAttribute(ATTR_METHODS), pAppConfig, bundleProperties);
	if (!methods.empty())
	{
		Poco::StringTokenizer tok(methods, ",;", Poco::StringTokenizer::TOK_TRIM | Poco::StringTokenizer::TOK_IGNORE_EMPTY);
		vPath.methods.insert(tok.begin(), tok.end());
	}

	const std::string& className = pExtensionElem->getAttribute(ATTR_CLASS);

	std::string library = pExtensionElem->getAttribute(ATTR_LIBRARY);
	if (library.empty())
		library = pBundle->symbolicName();

	std::string libraryPath = _pContext->pathForLibrary(library);
	auto pLibrary = std::make_shared<LibraryReference<WebRequestHandlerFactory>>(_pLoader, libraryPath, MANIFEST_NAME);
	std::unique_ptr<WebRequestHandlerFactory> pFactory(pLibrary->create(className));
	pFactory->init(_pContext->contextForBundle(pBundle));
	vPath.pFactory = Poco::makeShared<LibraryRequestHandlerFactory>(pLibrary, std::move(pFactory));

	_pDispatcher->addVirtualPath(vPath);
}


void WebServerExtensionPoint::handleCommon(Bundle::ConstPtr pBundle, Poco::XML::Element* pExtensionElem, WebServerDispatcher::VirtualPath& vPath)
{
	const Configuration::Ptr pAppConfig = applicationConfiguration();
	const auto expand = [&](const std::string& name)
	{
		return expandAttribute(pExtensionElem->getAttribute(name), pAppConfig, pBundle->properties());
	};
	vPath.pOwnerBundle             = pBundle;
	vPath.path                     = expand(ATTR_PATH);
	vPath.description              = expand(ATTR_DESCRIPTION);
	vPath.security.realm           = expand(ATTR_REALM);
	vPath.security.permission      = expand(ATTR_PERMISSION);
	vPath.security.authMethods     = WebServerDispatcher::parseAuthMethods(pExtensionElem->getAttribute(ATTR_AUTHMETHODS));
	vPath.security.session         = expand(ATTR_SESSION);
	vPath.security.secure          = pExtensionElem->getAttribute(ATTR_SECURE) == "true";
	vPath.security.csrfProtection  = pExtensionElem->getAttribute(ATTR_CSRFPROTECTION) == "true";
	vPath.security.csrfTokenHeader = expand(ATTR_CSRFTOKENHEADER);
	vPath.cors.enable              = pExtensionElem->getAttribute(ATTR_CORS) == "true";
	vPath.cors.allowOrigin         = expand(ATTR_ALLOWORIGIN);
	vPath.cors.allowMethods        = expand(ATTR_ALLOWMETHODS);
	vPath.cors.allowCredentials    = pExtensionElem->getAttribute(ATTR_ALLOWCREDENTIALS) != "false";
	vPath.cors.allowHeaders        = expand(ATTR_ALLOWHEADERS);
	vPath.hidden                   = pExtensionElem->getAttribute(ATTR_HIDDEN) == "true";
	vPath.cache                    = pExtensionElem->getAttribute(ATTR_CACHE) != "false";
	vPath.exactMatch               = pExtensionElem->getAttribute(ATTR_EXACTMATCH) == "true";
	vPath.redirectOn401            = expand(ATTR_REDIRECTON401);
	// Fall back to a global default from PreferencesService so admins can set
	// one login redirect for all directory/handler extensions instead of
	// repeating it on every bundle. Bundle-local properties don't include the
	// global app config; PreferencesService::configuration() does.
	if (vPath.redirectOn401.empty() && pAppConfig != nullptr)
	{
		vPath.redirectOn401 = pAppConfig->getString("osp.web.redirectOn401", "");
	}
	vPath.security.allowWebSocketOrigin = expand(ATTR_ALLOWWEBSOCKETORIGIN);

	if (vPath.security.csrfProtection && vPath.security.csrfTokenHeader.empty())
	{
		vPath.security.csrfTokenHeader = "X-XSRF-TOKEN";
	}

	const std::string& specialization = Poco::toLower(pExtensionElem->getAttribute(ATTR_ALLOWSPECIALIZATION));
	if (specialization.empty() || specialization == "none")
		vPath.security.mode = WebServerDispatcher::SM_NONE;
	else if (specialization == "owner")
		vPath.security.mode = WebServerDispatcher::SM_OWNER;
	else if (specialization == "all")
		vPath.security.mode = WebServerDispatcher::SM_ALL;
	else
		throw BundleException("Failed to parse extension point: Illegal attribute value " + specialization + " for attribute " + ATTR_ALLOWSPECIALIZATION);

	const std::string& responseFormat = Poco::toLower(pExtensionElem->getAttribute(ATTR_RESPONSEFORMAT));
	if (responseFormat.empty() || responseFormat == "html")
		vPath.responseFormat = WebServerDispatcher::RESPONSE_FORMAT_HTML;
	else if (responseFormat == "json")
		vPath.responseFormat = WebServerDispatcher::RESPONSE_FORMAT_JSON;
	else
		throw BundleException("Failed to parse extension point: Illegal attribute value " + responseFormat + " for attribute " + ATTR_RESPONSEFORMAT);
}


Configuration::Ptr WebServerExtensionPoint::applicationConfiguration() const
{
	ServiceRef::Ptr pPrefsRef = _pContext->registry().findByName(PreferencesService::SERVICE_NAME);
	if (pPrefsRef == nullptr) return nullptr;

	return pPrefsRef->castedInstance<PreferencesService>()->configuration();
}


} // namespace Poco::OSP::Web
