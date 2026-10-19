//
// WebEventRequestHandler.h
//
// Library: OSP/WebEvent
// Package: WebEvent
// Module:  WebEventRequestHandler
//
// Definition of the WebEventRequestHandler interface.
//
// Copyright (c) 2013, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#ifndef OSP_WebEvent_WebEventRequestHandler_INCLUDED
#define OSP_WebEvent_WebEventRequestHandler_INCLUDED


#include "Poco/OSP/WebEvent/WebEvent.h"
#include "Poco/OSP/WebEvent/WebEventServiceImpl.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/Net/HTTPRequestHandler.h"
#include "Poco/Net/WebSocket.h"


namespace Poco {
namespace OSP {
namespace WebEvent {


class OSPWebEvent_API WebEventRequestHandler: public Poco::Net::HTTPRequestHandler
	/// This class handles an incoming WebSocket upgrade request, 
	/// as well as the SUBSCRIBE message. Once the WebSocket connection
	/// has been established and the SUBSCRIBE request has been handled,
	/// the WebSocket is transferred to the WebEventServiceImpl instance.
{
public:
	WebEventRequestHandler(WebEventServiceImpl::Ptr pWebEventServiceImpl, Poco::OSP::BundleContext::Ptr pContext);
		/// Creates the WebEventRequestHandler.
		
	~WebEventRequestHandler();
		/// Destroys the WebEventRequestHandler.
		
	void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response);
		/// Handles an incoming WebSocket upgrade request, as well
		/// as the SUBSCRIBE message.

private:
	static std::string resolvePage(const Poco::Net::HTTPServerRequest& request);
		/// Returns the URL of the document that opened the connection: the
		/// "page" query parameter of the upgrade request if present (browsers
		/// send no Referer on WebSocket upgrades), the Referer header otherwise.

	bool authenticate() const;
	bool authorize() const;

	WebEventServiceImpl::Ptr _pWebEventServiceImpl;
	Poco::OSP::BundleContext::Ptr _pContext;
};


} } } // namespace Poco::OSP::WebEvent


#endif // OSP_WebEvent_WebEventRequestHandler_INCLUDED
