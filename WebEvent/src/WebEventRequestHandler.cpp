//
// WebEventRequestHandler.cpp
//
// Library: OSP/WebEvent
// Package: WebEvent
// Module:  WebEventRequestHandler
//
// Copyright (c) 2013, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "Poco/OSP/WebEvent/WebEventRequestHandler.h"
#include "Poco/OSP/WebEvent/WebEventServiceImpl.h"
#include "Poco/OSP/Web/WebServerDispatcher.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Net/HTTPServerResponse.h"
#include "Poco/Net/WebSocket.h"
#include "Poco/Net/NetException.h"
#include "Poco/Exception.h"
#include "Poco/Format.h"
#include "Poco/SharedPtr.h"
#include "Poco/String.h"
#include "Poco/StringTokenizer.h"
#include "Poco/URI.h"
#include <memory>
#include <string>
#include <utility>


namespace Poco::OSP::WebEvent {


WebEventRequestHandler::WebEventRequestHandler(WebEventServiceImpl::Ptr pWebEventServiceImpl, OriginSetPtr pAllowedOrigins):
	_pWebEventServiceImpl(pWebEventServiceImpl),
	_pAllowedOrigins(std::move(pAllowedOrigins))
{
}


WebEventRequestHandler::~WebEventRequestHandler() = default;


void WebEventRequestHandler::handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response)
{
	if (originAllowed(request))
	{
		try
		{
			_pWebEventServiceImpl->preflightRequest(request);
			if (response.getStatus() == Poco::Net::HTTPResponse::HTTP_OK)
			{
				// The slot taken by preflightRequest() goes to addSubscriber(),
				// or back on every other way out.
				bool handedOver = false;
				try
				{
					const Poco::Net::HTTPServerRequest* pRequest = &request;
					_pWebEventServiceImpl->requestReceived(pRequest);
					if (response.getStatus() == Poco::Net::HTTPResponse::HTTP_OK)
					{
						std::string username = _pWebEventServiceImpl->resolveUser(request);
						std::string page = resolvePage(request);
						std::string clientAddress = request.clientAddress().toString();
						Poco::SharedPtr<Poco::Net::WebSocket> pWS = new Poco::Net::WebSocket(request, response);
						if (_pWebEventServiceImpl->logger().debug())
						{
							_pWebEventServiceImpl->logger().debug(username.empty() ?
								Poco::format("WebSocket connection established with %s.", clientAddress) :
								Poco::format("WebSocket connection established with %s (user %s).", clientAddress, username));
						}
						handedOver = true;
						_pWebEventServiceImpl->addSubscriber(pWS, username, page, clientAddress);
					}
					else
					{
						_pWebEventServiceImpl->logRejectedUpgrade(WebEventServiceImpl::Rejection::DELEGATE,
							Poco::format("WebSocket connection from %s rejected by a delegate "
								"of the requestReceived event (status %d).",
								request.clientAddress().toString(),
								static_cast<int>(response.getStatus())));
						response.setContentLength(0);
						response.send();
					}
				}
				catch (...)
				{
					if (!handedOver) _pWebEventServiceImpl->releaseSocketSlot();
					throw;
				}
				if (!handedOver) _pWebEventServiceImpl->releaseSocketSlot();
			}
			else
			{
				response.setContentLength(0);
				response.send();
			}
		}
		catch (Poco::Net::WebSocketException& exc)
		{
			// A client that is not a WebSocket client, or a broken one: no
			// action is needed on the server.
			_pWebEventServiceImpl->logger().information(Poco::format("WebSocket upgrade from %s failed: %s",
				request.clientAddress().toString(), exc.displayText()));
			switch (exc.code())
			{
			case Poco::Net::WebSocket::WS_ERR_HANDSHAKE_UNSUPPORTED_VERSION:
				response.set("Sec-WebSocket-Version", Poco::Net::WebSocket::WEBSOCKET_VERSION);
				[[fallthrough]];
			case Poco::Net::WebSocket::WS_ERR_NO_HANDSHAKE:
			case Poco::Net::WebSocket::WS_ERR_HANDSHAKE_NO_VERSION:
			case Poco::Net::WebSocket::WS_ERR_HANDSHAKE_NO_KEY:
				response.setStatusAndReason(Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
				response.setContentLength(0);
				response.send();
				break;
			default:
				// Not a handshake error of the client, for instance one thrown
				// by a delegate: the client still gets an answer.
				if (!response.sent())
				{
					response.setStatusAndReason(Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
					response.setContentLength(0);
					response.send();
				}
				break;
			}
		}
	}
	else
	{
		_pWebEventServiceImpl->logRejectedUpgrade(WebEventServiceImpl::Rejection::ORIGIN,
			Poco::format("WebSocket connection from %s rejected: origin %s is neither the server's own "
				"nor listed in osp.web.event.allowedOrigins.",
				request.clientAddress().toString(),
				request.get("Origin")));
		response.setStatusAndReason(Poco::Net::HTTPResponse::HTTP_FORBIDDEN);
		response.setContentLength(0);
		response.send();
	}
}


std::string WebEventRequestHandler::resolvePage(const Poco::Net::HTTPServerRequest& request)
{
	// Browsers send no Referer on WebSocket upgrade requests (the handshake
	// fetch is no-referrer), so clients identify their document in the
	// request's "page" query parameter; Referer covers other clients.
	try
	{
		Poco::URI uri(request.getURI());
		for (const auto& param: uri.getQueryParameters())
		{
			if (param.first == "page" && !param.second.empty())
			{
				return param.second;
			}
		}
	}
	catch (Poco::Exception&)
	{
	}
	return request.get("Referer", std::string());
}


WebEventRequestHandler::OriginSetPtr WebEventRequestHandler::parseAllowedOrigins(const std::string& list)
{
	auto pOrigins = std::make_shared<OriginSet>();
	const Poco::StringTokenizer origins(list, ",", Poco::StringTokenizer::TOK_TRIM | Poco::StringTokenizer::TOK_IGNORE_EMPTY);
	for (const auto& origin: origins)
	{
		pOrigins->insert(Poco::toLower(origin));
	}
	return pOrigins;
}


bool WebEventRequestHandler::originAllowed(const Poco::Net::HTTPServerRequest& request) const
{
	// Browsers send the Origin of the page with every WebSocket upgrade, but
	// do not restrict the upgrade to that origin: any page may connect, and
	// with the user's cookies. Clients other than browsers send no Origin.
	if (!request.has("Origin")) return true;

	const std::string origin = Poco::toLower(request.get("Origin"));
	if (_pAllowedOrigins != nullptr && _pAllowedOrigins->contains(origin)) return true;
	return Poco::OSP::Web::WebServerDispatcher::isSameOrigin(request, origin);
}


} // namespace Poco::OSP::WebEvent
