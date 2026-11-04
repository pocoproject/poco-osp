//
// WebInfo.cpp
//
// Copyright (c) 2007-2016, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "Poco/Net/HTTPRequestHandler.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Net/HTTPServerResponse.h"
#include "Poco/Net/HTTPServerParams.h"
#include "Poco/Net/IPAddress.h"
#include "Poco/Net/NetworkInterface.h"
#include "Poco/Net/Net.h"
#include "Poco/OSP/Web/WebRequestHandlerFactory.h"
#include "Poco/LocalDateTime.h"
#include "Poco/DateTimeFormatter.h"
#include "Poco/DateTimeFormat.h"
#include "Poco/Environment.h"
#include "Poco/Process.h"
#include "Poco/ClassLibrary.h"


using Poco::Net::HTTPRequestHandler;
using Poco::Net::HTTPServerRequest;
using Poco::Net::HTTPServerResponse;
using Poco::Net::htmlize;
using Poco::OSP::Web::WebRequestHandlerFactory;
using Poco::Environment;
using Poco::LocalDateTime;
using Poco::DateTimeFormatter;
using Poco::DateTimeFormat;
using Poco::Process;


namespace
{
	class InfoHandler: public HTTPRequestHandler
		/// Return a HTML document with information about the process.
	{
	public:
		void handleRequest(HTTPServerRequest& request, HTTPServerResponse& response) override
		{
			response.setContentType("text/html");
			response.setChunkedTransferEncoding(true);
			std::ostream& ostr = response.send();

			const std::string& softwareVersion = request.serverParams().getSoftwareVersion();
			LocalDateTime now;

			std::string osName = Environment::osName();
			std::string osDisplayName = Environment::osDisplayName();
			if (osDisplayName != osName)
			{
				osName += " (";
				osName += osDisplayName;
				osName += ")";
			}

			ostr << "<HTML><HEAD><TITLE>Server Information</TITLE>"
			        "<LINK REL=\"stylesheet\" HREF=\"css/styles.css\" TYPE=\"text/css\"/></HEAD><BODY>"
			        "<DIV CLASS=\"header\">"
					"<H1 CLASS=\"category\">Open Service Platform</h1>"
					"<H1 CLASS=\"title\">Server Information</H1>"
					"</DIV>"
			        "<DIV CLASS=\"body\">"
			        "<UL>";

			ostr << "<LI><B>Host:</B> " << Environment::nodeName() << "</LI>"
			     << "<LI><B>Node ID:</B> " << Environment::nodeId() << "</LI>"
			     << "<LI><B>IP Addresses:</B> " << getHostAddresses() << "</LI>"
			     << "<LI><B>OS Name:</B> " << osName << "</LI>"
			     << "<LI><B>OS Version:</B> " << Environment::osVersion() << "</LI>"
			     << "<LI><B>OS Architecture:</B> " << Environment::osArchitecture() << "</LI>"
			     << "<LI><B>Processor Cores:</B> " << Environment::processorCount() << "</LI>"
			     << "<LI><B>Local Server Time:</B> " << DateTimeFormatter::format(now, DateTimeFormat::HTTP_FORMAT)
			     << "</LI>"
			     << "<LI><B>Server Process ID:</B> " << Process::id() << "</LI>";

			ostr << "</UL><HR><P>";
			ostr << htmlize(softwareVersion) << " at " << request.serverAddress().toString();
			ostr << "</P></DIV></BODY></HTML>";
		}

	protected:
		[[nodiscard]] std::string getHostAddresses()
		{
			std::string result;
			for (const auto& networkInterface: Poco::Net::NetworkInterface::list())
			{
				const Poco::Net::IPAddress& addr = networkInterface.address();
				if (addr.isUnicast() && !addr.isLoopback())
				{
					if (!result.empty())
						result += ", ";
					result += addr.toString();
				}
			}
			return result;
		}
	};


	class InfoHandlerFactory: public WebRequestHandlerFactory
	{
	public:
		[[nodiscard]] HTTPRequestHandler* createRequestHandler(const HTTPServerRequest& /*request*/) override
		{
			return new InfoHandler;
		}
	};
}


POCO_BEGIN_NAMED_MANIFEST(WebServer, WebRequestHandlerFactory)
	POCO_EXPORT_CLASS(InfoHandlerFactory)
POCO_END_MANIFEST
