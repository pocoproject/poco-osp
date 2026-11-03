//
// WebIndex.cpp
//
// Copyright (c) 2007-2016, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#include "Poco/Net/HTTPRequestHandler.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Net/HTTPServerResponse.h"
#include "Poco/Net/HTTPServerParams.h"
#include "Poco/Net/Net.h"
#include "Poco/OSP/Web/WebRequestHandlerFactory.h"
#include "Poco/OSP/Web/WebServerDispatcher.h"
#include "Poco/OSP/ServiceRegistry.h"
#include "Poco/OSP/Bundle.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/AutoPtr.h"
#include "Poco/ClassLibrary.h"
#include "Poco/URI.h"


using Poco::Net::HTTPRequestHandler;
using Poco::Net::HTTPServerRequest;
using Poco::Net::HTTPServerResponse;
using Poco::Net::htmlize;
using Poco::OSP::Web::WebRequestHandlerFactory;
using Poco::OSP::Web::WebServerDispatcher;
using Poco::OSP::Service;
using Poco::OSP::ServiceRef;
using Poco::AutoPtr;


class IndexHandler: public HTTPRequestHandler
	/// Return a HTML document with a list of all available
	/// resources on the OSP web server.
{
public:
	IndexHandler(AutoPtr<WebServerDispatcher> pWebServerDispatcher):
		_pWebServerDispatcher(pWebServerDispatcher)
	{
	}
	
	void handleRequest(HTTPServerRequest& request, HTTPServerResponse& response) override
	{
		WebServerDispatcher::PathInfoMap pathInfos;
		_pWebServerDispatcher->listVirtualPaths(pathInfos);
		
		response.setContentType("text/html");
		response.setChunkedTransferEncoding(true);
		std::ostream& ostr = response.send();

		const std::string& softwareVersion = request.serverParams().getSoftwareVersion();

		ostr << "<HTML><HEAD><TITLE>Server Index</TITLE>"
		        "<LINK REL=\"stylesheet\" HREF=\"css/styles.css\" TYPE=\"text/css\"/></HEAD><BODY>"
		        "<DIV CLASS=\"header\">"
				"<H1 CLASS=\"category\">Open Service Platform</h1>"
				"<H1 CLASS=\"title\">Web Server Index</H1>"
				"</DIV>"
		        "<DIV CLASS=\"body\">"
		        "<P>The following resources are available on this server:</P>"
		        "<UL>";
		for (WebServerDispatcher::PathInfoMap::const_iterator it = pathInfos.begin(); it != pathInfos.end(); ++it)
		{
			ostr << "<LI>";
			std::string title(it->second.description);
			if (title.empty()) title = it->first;
			ostr << "<A HREF=\"" << it->first << "\" TITLE=\"Path: " << it->first << "\">" << htmlize(title) << "</A><BR>";
			ostr << "<I>" << htmlize(it->second.pBundle->name()) << " " 
			     << it->second.pBundle->version().toString() << "</I></LI>";
		}
		ostr << "</UL><HR><P>";
		ostr << htmlize(softwareVersion) << " at " << request.serverAddress().toString();
		ostr << "</P></DIV></BODY></HTML>";
	}
	
private:
	AutoPtr<WebServerDispatcher> _pWebServerDispatcher;
};


class IndexHandlerFactory: public WebRequestHandlerFactory
{
public:
	IndexHandlerFactory() = default;

	[[nodiscard]] HTTPRequestHandler* createRequestHandler(const HTTPServerRequest& request) override
	{
		const std::string path = Poco::URI(request.getURI()).getPath();
		if (_pWebServerDispatcher != nullptr && (path == "/" || path == "/index.html"))
		{
			return new IndexHandler(_pWebServerDispatcher);
		}
		return nullptr;
	}
	
protected:
	void initImpl() override
	{
		ServiceRef::Ptr pWebServerDispatcherRef = context()->registry().findByName(WebServerDispatcher::SERVICE_NAME);
		if (pWebServerDispatcherRef != nullptr)
		{
			 _pWebServerDispatcher = pWebServerDispatcherRef->castedInstance<WebServerDispatcher>();
		}
		else
		{
			context()->logger().error("The WebServerDispatcher service is not available. The server index page is not served.");
		}
	}
	
private:
	AutoPtr<WebServerDispatcher> _pWebServerDispatcher;
};


POCO_BEGIN_NAMED_MANIFEST(WebServer, WebRequestHandlerFactory)
	POCO_EXPORT_CLASS(IndexHandlerFactory)
POCO_END_MANIFEST
