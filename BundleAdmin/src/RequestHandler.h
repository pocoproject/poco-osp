//
// RequestHandler.h
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef BundleAdmin_RequestHandler_INCLUDED
#define BundleAdmin_RequestHandler_INCLUDED


#include "Poco/Net/AbstractHTTPRequestHandler.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/Web/WebSession.h"
#include "Poco/Util/MapConfiguration.h"
#include "Poco/AutoPtr.h"
#include <ostream>


class RequestHandler: public Poco::Net::AbstractHTTPRequestHandler
	/// The base class for all request handlers.
{
protected:
	RequestHandler(Poco::OSP::BundleContext::Ptr pContext);
		/// Creates the RequestHandler.

	~RequestHandler() override;
		/// Destroys the RequestHandler.

	void run() override;
		/// Calls handle(). If handle() throws an exception before the response
		/// has been sent, sends a 500 Internal Server Error page with the
		/// escaped message of the exception. An exception after the response
		/// has been sent is logged.

	virtual void handle() = 0;
		/// Handles the request.
	
	[[nodiscard]] Poco::OSP::BundleContext::Ptr context() const;
		/// Returns the bundle context.
	
	[[nodiscard]] Poco::OSP::Bundle::ConstPtr bundle() const;
		/// Returns the bundle.
		
	Poco::Util::MapConfiguration& templat();
		/// Returns the template args.
		
	std::ostream& stream();
		/// Prepares the response (when called for the first time)
		/// and returns the response stream.

	void beginPage(const std::string& title);
		/// Writes the beginning of the HTML page.

	void beginPage(const std::string& title, const std::string backlinkPath, const std::string& backlinkTitle);
		/// Writes the beginning of the HTML page and includes a link to the parent page.
	
	void endPage();
		/// Finishes the HTML page.
		
	void beginTable();
		/// Starts a table.
		
	void endTable();
		/// Ends a table.
		
	void beginRow(bool even);
		/// Starts a table row.
		
	void endRow();
		/// Ends a table row.
		
	void beginHeaderCell();
		/// Starts a header cell.
		
	void endHeaderCell();
		/// Ends a header cell.
		
	void beginCell();
		/// Begins a cell.
		
	void endCell();
		/// Ends a cell.
		
	void headerCell(const std::string& text);
		/// Writes a header cell.
		
	void cell(const std::string& text);
		/// Writes a cell.
		
	void beginList();
		/// Starts an unordered list.
		
	void endList();
		/// Ends an unordered list.
		
	void beginItem();
		/// Starts a list item.
		
	void endItem();
		/// Ends a list item.
		
	void item(const std::string& text);
		/// Writes a list item.

	void item(const std::string& label, const std::string& value);
		/// Writes a list item.
		
	void heading(const std::string& title);
		/// Writes a heading.
		
	void beginPara();
		/// Starts a paragraph.
		
	void endPara();
		/// Ends a paragraph.
		
	void beginActionBar();
		/// Starts an action bar.
		
	void endActionBar();
		/// Ends an action bar.
		
	void write(const std::string& text);
		/// Writes some text.

	void action(Poco::OSP::Bundle::Ptr pBundle, const std::string& action, const std::string& label);
		/// Writes an action button.
		
	void action(Poco::OSP::Bundle::Ptr pBundle, const std::string& actionPath, const std::string& action, const std::string& label);
		/// Writes an action button.

	void action(const std::string& actionPath, const std::string& action, const std::string& label);
		/// Writes an action button.

	void action(const std::string& symbolicName, const std::string& actionPath, const std::string& action, const std::string& label);
		/// Writes an action button.
		
	void sendTemplate(const std::string& templateName);
		/// Sends a template, with the template arguments inserted (see expandArgs()).
		/// The arguments must be valid HTML.
		
	void linkBundle(Poco::OSP::Bundle::ConstPtr pBundle, const std::string& name);
		/// Writes a link to a bundle.

	void linkBundle(const std::string& symbolicName);
		/// Writes a link to a bundle.

	[[nodiscard]] bool isSameOriginPost();
		/// Returns true if the request is a POST request, and its Origin header
		/// field (or, without one, its Referer header field) names the origin
		/// of the request itself.

	[[nodiscard]] bool hasValidCSRFToken(const std::string& token);
		/// Returns true if the request does not belong to the web session named
		/// by the bundle property web.session, or if token is the CSRF token of
		/// that session.

	[[nodiscard]] bool isNeededByThisBundle(Poco::OSP::Bundle::Ptr pBundle);
		/// Returns true if pBundle is this bundle or one that it requires, directly or indirectly
		/// (Require-Bundle or Require-Module): stopping, uninstalling or replacing it would stop this handler.

	void sendForbidden();
		/// Sends a 403 Forbidden response for a request that would change
		/// the state of a bundle, but is not allowed to.

	[[nodiscard]] std::string csrfTokenOfSession();
		/// Returns the CSRF token of the web session of the request named by the
		/// bundle property web.session, or an empty string if the request has no
		/// such session.

	[[nodiscard]] std::string text(const std::string& id);
		/// Returns a text fragment from the resources, with the template
		/// arguments inserted (see expandArgs()).

	[[nodiscard]] std::string expandArgs(const std::string& text);
		/// Replaces every ${name} in text for which a template argument exists
		/// with the value of the argument. The values, which can contain text from
		/// bundle manifests, are inserted as they are, without expanding them again.

	[[nodiscard]] static std::string htmlize(const std::string& str);
		/// Escapes characters in str to make it valid HTML, also in attribute values.

	[[nodiscard]] static std::string encodeQueryValue(const std::string& str);
		/// Encodes str for a parameter value in the query of a URI.
			
private:
	[[nodiscard]] Poco::OSP::Web::WebSession::Ptr session();
		/// Returns the web session of the request named by the bundle property
		/// web.session, or a null pointer if the request has no such session
		/// or no web session manager is registered.

	void logIncompleteResponse(const std::string& reason);
		/// Logs an error that ended the request after a part of the response
		/// had been sent.

	Poco::OSP::BundleContext::Ptr _pContext;
	std::ostream* _pStream = nullptr;
	Poco::AutoPtr<Poco::Util::MapConfiguration> _pTemplateArgs;
};


//
// inlines
//
inline Poco::OSP::BundleContext::Ptr RequestHandler::context() const
{
	return _pContext;
}


inline Poco::OSP::Bundle::ConstPtr RequestHandler::bundle() const
{
	return _pContext->thisBundle();
}


inline Poco::Util::MapConfiguration& RequestHandler::templat()
{
	return *_pTemplateArgs;
}


#endif // BundleAdmin_RequestHandler_INCLUDED
