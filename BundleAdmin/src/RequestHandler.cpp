//
// RequestHandler.cpp
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#include "RequestHandler.h"
#include "Poco/OSP/ServiceRef.h"
#include "Poco/OSP/ServiceRegistry.h"
#include "Poco/OSP/Web/WebServerDispatcher.h"
#include "Poco/OSP/Web/WebSession.h"
#include "Poco/OSP/Web/WebSessionManager.h"
#include "Poco/Net/HTTPRequest.h"
#include "Poco/Net/HTTPResponse.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Net/HTTPServerResponse.h"
#include "Poco/Exception.h"
#include "Poco/Logger.h"
#include "Poco/URI.h"
#include <exception>
#include <set>
#include <string>
#include <vector>


using Poco::Util::MapConfiguration;
using Poco::AutoPtr;


RequestHandler::RequestHandler(Poco::OSP::BundleContext::Ptr pContext):
	_pContext(pContext),
	_pTemplateArgs(new MapConfiguration)
{
}


RequestHandler::~RequestHandler() = default;


void RequestHandler::run()
{
	// The messages of exceptions can contain values from uploaded bundles.
	try
	{
		handle();
	}
	catch (Poco::Exception& exc)
	{
		if (!response().sent())
			sendErrorResponse(Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR, htmlize(exc.displayText()));
		else
			logIncompleteResponse(exc.displayText());
	}
	catch (std::exception& exc)
	{
		if (!response().sent())
			sendErrorResponse(Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR, htmlize(exc.what()));
		else
			logIncompleteResponse(exc.what());
	}
}


std::ostream& RequestHandler::stream()
{
	if (_pStream == nullptr)
	{
		response().setContentType("text/html");
		response().setChunkedTransferEncoding(true);
		_pStream = &response().send();
	}
	return *_pStream;
}


void RequestHandler::beginPage(const std::string& title)
{
	templat().clear();
	templat().setString("title", htmlize(title));
	templat().setString("backlink", std::string());
	sendTemplate("html.beginPage");
}


void RequestHandler::beginPage(const std::string& title, const std::string backlinkPath, const std::string& backlinkTitle)
{
	templat().clear();
	templat().setString("title", htmlize(title));
	templat().setString("backlink.path", htmlize(backlinkPath));
	templat().setString("backlink.title", htmlize(backlinkTitle));
	templat().setString("backlink", expandArgs(bundle()->properties().getString("html.backlink")));
	sendTemplate("html.beginPage");
}


void RequestHandler::endPage()
{
	sendTemplate("html.endPage");
}


void RequestHandler::beginTable()
{
	sendTemplate("html.beginTable");
}


void RequestHandler::endTable()
{
	sendTemplate("html.endTable");
}

	
void RequestHandler::beginRow(bool even)
{
	sendTemplate(even ? "html.beginEvenRow" : "html.beginOddRow");
}

	
void RequestHandler::endRow()
{
	sendTemplate("html.endRow");
}

	
void RequestHandler::beginHeaderCell()
{
	sendTemplate("html.beginHeaderCell");
}

	
void RequestHandler::endHeaderCell()
{
	sendTemplate("html.endHeaderCell");
}

	
void RequestHandler::beginCell()
{
	sendTemplate("html.beginCell");
}

	
void RequestHandler::endCell()
{
	sendTemplate("html.endCell");
}


void RequestHandler::headerCell(const std::string& text)
{
	beginHeaderCell();
	write(text);
	endHeaderCell();
}

	
void RequestHandler::cell(const std::string& text)
{
	beginCell();
	write(text);
	endCell();
}


void RequestHandler::beginList()
{
	sendTemplate("html.beginList");
}

	
void RequestHandler::endList()
{
	sendTemplate("html.endList");
}

	
void RequestHandler::beginItem()
{
	sendTemplate("html.beginItem");
}

	
void RequestHandler::endItem()
{
	sendTemplate("html.endItem");
}

	
void RequestHandler::item(const std::string& text)
{
	beginItem();
	write(text);
	endItem();
}


void RequestHandler::item(const std::string& label, const std::string& value)
{
	beginItem();
	stream() << "<B>";
	write(label);
	stream() << ":</B> ";
	write(value);
	endItem();
}


void RequestHandler::heading(const std::string& title)
{
	templat().clear();
	templat().setString("title", htmlize(title));
	sendTemplate("html.heading");
}


void RequestHandler::beginPara()
{
	sendTemplate("html.beginPara");
}


void RequestHandler::endPara()
{
	sendTemplate("html.endPara");
}


void RequestHandler::beginActionBar()
{
	sendTemplate("html.beginActionBar");
}


void RequestHandler::endActionBar()
{
	sendTemplate("html.endActionBar");
}


void RequestHandler::write(const std::string& text)
{
	stream() << htmlize(text);
}


void RequestHandler::action(Poco::OSP::Bundle::Ptr pBundle, const std::string& action, const std::string& label)
{
	this->action(pBundle, std::string(), action, label);
}


void RequestHandler::action(Poco::OSP::Bundle::Ptr pBundle, const std::string& actionPath, const std::string& action, const std::string& label)
{
	this->action(pBundle->symbolicName(), actionPath, action, label);
}


void RequestHandler::action(const std::string& actionPath, const std::string& action, const std::string& label)
{
	this->action(std::string(), actionPath, action, label);
}


void RequestHandler::action(const std::string& symbolicName, const std::string& actionPath, const std::string& action, const std::string& label)
{
	beginCell();
	templat().clear();
	templat().setString("symbolicName", htmlize(symbolicName));
	templat().setString("action", htmlize(action));
	templat().setString("actionLabel", htmlize(label));
	templat().setString("actionPath", htmlize(bundle()->properties().expand(actionPath)));
	templat().setString("csrfToken", htmlize(csrfTokenOfSession()));
	sendTemplate("html.actionForm");
	endCell();
}


void RequestHandler::sendTemplate(const std::string& templateName)
{
	stream() << expandArgs(bundle()->properties().getString(templateName));
}


void RequestHandler::linkBundle(Poco::OSP::Bundle::ConstPtr pBundle, const std::string& name)
{
	templat().clear();
	templat().setString("symbolicName", htmlize(encodeQueryValue(pBundle->symbolicName())));
	templat().setString("name", htmlize(name));
	sendTemplate("html.bundleLink");
}


void RequestHandler::linkBundle(const std::string& symbolicName)
{
	templat().clear();
	templat().setString("symbolicName", htmlize(encodeQueryValue(symbolicName)));
	templat().setString("name", htmlize(symbolicName));
	sendTemplate("html.bundleLink");
}


bool RequestHandler::isSameOriginPost()
{
	if (request().getMethod() != Poco::Net::HTTPRequest::HTTP_POST) return false;

	const std::string origin = request().get("Origin", request().get("Referer", ""));
	return !origin.empty() && Poco::OSP::Web::WebServerDispatcher::isSameOrigin(request(), origin);
}


bool RequestHandler::hasValidCSRFToken(const std::string& token)
{
	Poco::OSP::Web::WebSession::Ptr pSession = session();
	return pSession == nullptr || pSession->isCSRFToken(token);
}


bool RequestHandler::isNeededByThisBundle(Poco::OSP::Bundle::Ptr pBundle)
{
	std::set<int> visited;
	std::vector<Poco::OSP::Bundle::Ptr> pending{bundle()};
	while (!pending.empty())
	{
		Poco::OSP::Bundle::Ptr pCurrent = pending.back();
		pending.pop_back();
		if (!visited.insert(pCurrent->id()).second) continue;
		if (pCurrent->id() == pBundle->id()) return true;

		for (const auto& dependency: pCurrent->resolvedDependencies())
		{
			Poco::OSP::Bundle::Ptr pDependency = context()->findBundle(dependency.id);
			if (pDependency != nullptr) pending.push_back(pDependency);
		}
	}
	return false;
}


void RequestHandler::sendForbidden()
{
	sendErrorResponse(Poco::Net::HTTPResponse::HTTP_FORBIDDEN, htmlize(text("forbidden")));
}


std::string RequestHandler::csrfTokenOfSession()
{
	Poco::OSP::Web::WebSession::Ptr pSession = session();
	return pSession == nullptr ? std::string() : pSession->getValue<std::string>(Poco::OSP::Web::WebSession::CSRF_TOKEN, std::string());
}


Poco::OSP::Web::WebSession::Ptr RequestHandler::session()
{
	const std::string name = bundle()->properties().getString("web.session", "");
	if (name.empty()) return nullptr;

	Poco::OSP::ServiceRef::Ptr pSessionManagerRef = context()->registry().findByName(Poco::OSP::Web::WebSessionManager::SERVICE_NAME);
	if (pSessionManagerRef == nullptr) return nullptr;
	Poco::OSP::Web::WebSessionManager::Ptr pSessionManager = pSessionManagerRef->castedInstance<Poco::OSP::Web::WebSessionManager>();
	return pSessionManager->find(name, request());
}


void RequestHandler::logIncompleteResponse(const std::string& reason)
{
	context()->logger().error("The bundle %s sent the page %s incomplete: %s", bundle()->symbolicName(), request().getURI(), reason);
}


std::string RequestHandler::text(const std::string& id)
{
	std::string resId("text.");
	resId += id;
	return expandArgs(bundle()->properties().getString(resId));
}


std::string RequestHandler::expandArgs(const std::string& text)
{
	std::string result;
	std::string::size_type pos = 0;
	for (;;)
	{
		const std::string::size_type start = text.find("${", pos);
		if (start == std::string::npos) break;
		const std::string::size_type end = text.find('}', start + 2);
		if (end == std::string::npos) break;
		result.append(text, pos, start - pos);
		const std::string name = text.substr(start + 2, end - start - 2);
		if (templat().has(name))
			result += templat().getRawString(name);
		else
			result.append(text, start, end - start + 1);
		pos = end + 1;
	}
	result.append(text, pos, std::string::npos);
	return result;
}


std::string RequestHandler::htmlize(const std::string& str)
{
	std::string::const_iterator it(str.begin());
	std::string::const_iterator end(str.end());
	std::string html;
	for (; it != end; ++it)
	{
		switch (*it)
		{
		case '<': html += "&lt;"; break;
		case '>': html += "&gt;"; break;
		case '"': html += "&quot;"; break;
		case '\'': html += "&#39;"; break;
		case '&': html += "&amp;"; break;
		default:  html += *it; break;
		}
	}
	return html;
}


std::string RequestHandler::encodeQueryValue(const std::string& str)
{
	std::string encoded;
	Poco::URI::encode(str, "?#/:;+@&=", encoded);
	return encoded;
}
