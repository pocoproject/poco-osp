//
// WebEcho.cpp
//
// Copyright (c) 2007-2016, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#include "Poco/Net/HTMLForm.h"
#include "Poco/Net/HTTPRequest.h"
#include "Poco/Net/HTTPRequestHandler.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Net/HTTPServerResponse.h"
#include "Poco/Net/NameValueCollection.h"
#include "Poco/Net/Net.h"
#include "Poco/OSP/Web/WebRequestHandlerFactory.h"
#include "Poco/ClassLibrary.h"
#include "Poco/Exception.h"
#include "Poco/URI.h"


using Poco::Net::HTMLForm;
using Poco::Net::HTTPRequest;
using Poco::Net::HTTPRequestHandler;
using Poco::Net::HTTPServerRequest;
using Poco::Net::HTTPServerResponse;
using Poco::Net::NameValueCollection;
using Poco::Net::htmlize;
using Poco::OSP::Web::WebRequestHandlerFactory;
using Poco::URI;


class EchoHandler: public HTTPRequestHandler
	/// Return a HTML document with information about the HTTP request.
{
public:
	void handleRequest(HTTPServerRequest& request, HTTPServerResponse& response)
	{
		HTMLForm form;
		URI requestURI;
		try
		{
			form.load(request, request.stream());
			requestURI = URI(request.getURI());
		}
		catch (Poco::SyntaxException&)
		{
			// A malformed percent escape in the URI or the form data is an error of the client.
			response.setStatusAndReason(HTTPServerResponse::HTTP_BAD_REQUEST);
			response.setContentLength(0);
			response.send();
			return;
		}

		response.setContentType("text/html");
		response.setChunkedTransferEncoding(true);
        
		std::ostream& ostr = response.send();
        
		ostr << "<html><head><title>Echo HTTP Headers and HTML Form Information</title>"
		        "<link rel=\"stylesheet\" href=\"css/styles.css\" type=\"text/css\"/></head><body>"
		        "<div class=\"header\">"
				"<h1 class=\"category\">Open Service Platform</h1>"
				"<h1 class=\"title\">Echo HTTP Headers and HTML Form Information</H1>"
				"</div>"
		        "<div class=\"body\">";
        
		// echo the items available by method
		ostr << "<h2>Request</h2>"
		     << "<ul>"
		     << "<li><b>Method:</b> " << htmlize(request.getMethod()) << "</li>"
             << "<li><b>URI Path:</b> " << htmlize(requestURI.getPath()) << "</li>"
             << "<li><b>URI Query:</b> " << htmlize(requestURI.getQuery()) << "</li>"
             << "<li><b>HTTP Version:</b> " << htmlize(request.getVersion()) << "</li>"
             << "<li><b>Host:</b> " << htmlize(request.get(HTTPRequest::HOST, std::string())) << "</li>"
             << "<li><b>Content Type:</b> " << htmlize(request.getContentType()) << "</li>"
             << "<li><b>Chunked:</b> " << (request.getChunkedTransferEncoding() ? "true" : "false") << "</li>"
             << "<li><b>Keep Alive:</b> " << (request.getKeepAlive() ? "true" : "false") << "</li>"
             << "<li><b>Transfer Encoding:</b> " << htmlize(request.getTransferEncoding()) << "</li>"
             << "<li><b>Client Address:</b> " << request.clientAddress().toString() << "</li>";

        if (request.hasContentLength())
        {
            ostr << "<li><b>Content Length:</b> " << request.getContentLength64() << "</li>";
        }

		ostr << "</ul>";

        // echo the request headers
        {
            ostr << "<hr>"
                    "<h2>Request Headers</h2><ul>\n";

            NameValueCollection::ConstIterator itr(request.begin());
            NameValueCollection::ConstIterator itrEnd(request.end());
            
            for (; itr != itrEnd; ++itr)
            {
                ostr << "<li><b>" << htmlize(itr->first) << "</b>: " <<  htmlize(itr->second) << "</li>";
            }
            ostr << "</ul>";
        }

        // echo any cookies
        {
            NameValueCollection cookies;
            request.getCookies(cookies);

            if (!cookies.empty())
            {
                ostr << "<hr>";
                ostr << "<h2>Cookies</h2><ul>\n";

                NameValueCollection::ConstIterator itr(cookies.begin());
                NameValueCollection::ConstIterator itrEnd(cookies.end());
                
                for (; itr != itrEnd; ++itr)
                {
                    ostr << "<li><b>" << htmlize(itr->first) << "</b>: " <<  htmlize(itr->second) << "</li>";
                }
                ostr << "</ul>";
            }
        }
        
        // echo any form data (GETs or POSTs)
		if (!form.empty())
		{
            ostr << "<hr>"
			        "<h2>Form Data</h2><ul>\n";

			NameValueCollection::ConstIterator itr(form.begin());
			NameValueCollection::ConstIterator itrEnd(form.end());
			for (; itr != itrEnd; ++itr)
			{
				ostr << "<li><b>" << htmlize(itr->first) << "</b>: " << htmlize(itr->second) << "</li>\n";
			}
			ostr << "</ul>";
		}
        
        ostr << "<hr>"
             << "<h2>Response</h2>"
             << "<ul>"
		     << "<li><b>Status:</b> "  << response.getStatus()  << "</li>"
             << "<li><b>Reason:</b> "  << response.getReason()  << "</li>"
            << "</ul>";
        
        // echo the response headers
        {
            ostr << "<hr>"
                    "<h2>Response Headers</h2><ul>\n";
            
            NameValueCollection::ConstIterator itr(response.begin());
            NameValueCollection::ConstIterator itrEnd(response.end());
            
            for (; itr != itrEnd; ++itr)
            {
                ostr << "<li><b>" << htmlize(itr->first) << "</b>: " <<  htmlize(itr->second) << "</li>";
            }
            ostr << "</ul>";
        }
        
		ostr << "</div></body></html>";
	}
};


class EchoHandlerFactory: public WebRequestHandlerFactory
{
public:
	HTTPRequestHandler* createRequestHandler(const HTTPServerRequest& request)
	{
		return new EchoHandler();
	}
};


POCO_BEGIN_NAMED_MANIFEST(WebServer, WebRequestHandlerFactory)
	POCO_EXPORT_CLASS(EchoHandlerFactory)
POCO_END_MANIFEST
