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
#include "Poco/Net/HTTPRequestHandler.h"
#include <memory>
#include <set>
#include <string>


namespace Poco::OSP::WebEvent {


class OSPWebEvent_API WebEventRequestHandler: public Poco::Net::HTTPRequestHandler
	/// This class handles an incoming WebSocket upgrade request. Once the
	/// WebSocket connection has been established, it is transferred to the
	/// WebEventServiceImpl instance, which handles all messages, including
	/// the first SUBSCRIBE.
{
public:
	using OriginSet = std::set<std::string>;
	using OriginSetPtr = std::shared_ptr<const OriginSet>;

	explicit WebEventRequestHandler(WebEventServiceImpl::Ptr pWebEventServiceImpl, OriginSetPtr pAllowedOrigins = OriginSetPtr());
		/// Creates the WebEventRequestHandler.
		///
		/// An upgrade request that carries an Origin header is accepted
		/// only if the origin is the request's own or is one of
		/// pAllowedOrigins, given in lower case (for example
		/// "https://app.example.com"). The request's own origin has the
		/// scheme of the connection (https if it is secure, http otherwise)
		/// and the host and port of the Host header, the default port of
		/// the scheme where none is given. Browsers always send an Origin
		/// header; a request without one comes from another kind of client
		/// and is accepted.

	[[nodiscard]] static OriginSetPtr parseAllowedOrigins(const std::string& list);
		/// Returns the origins of a comma-separated list, trimmed and in
		/// lower case, as the value of osp.web.event.allowedOrigins is given.

	~WebEventRequestHandler() override;
		/// Destroys the WebEventRequestHandler.

	void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) override;
		/// Checks the upgrade request, upgrades the connection and hands
		/// it over to the WebEventServiceImpl.

private:
	[[nodiscard]] static std::string resolvePage(const Poco::Net::HTTPServerRequest& request);
		/// Returns the URL of the document that opened the connection: the
		/// "page" query parameter of the upgrade request if present (browsers
		/// send no Referer on WebSocket upgrades), the Referer header otherwise.

	[[nodiscard]] bool originAllowed(const Poco::Net::HTTPServerRequest& request) const;
		/// Returns true if the request carries no Origin header, or one
		/// that is allowed (see the constructor).

	WebEventServiceImpl::Ptr _pWebEventServiceImpl;
	OriginSetPtr _pAllowedOrigins;
};


} // namespace Poco::OSP::WebEvent


#endif // OSP_WebEvent_WebEventRequestHandler_INCLUDED
