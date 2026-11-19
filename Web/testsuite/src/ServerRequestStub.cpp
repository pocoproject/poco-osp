//
// ServerRequestStub.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "ServerRequestStub.h"
#include "Poco/Net/HTTPResponse.h"
#include "Poco/Net/HTTPServerParams.h"
#include "Poco/Net/HTTPServerResponse.h"
#include "Poco/Types.h"
#include <ostream>
#include <string>
#include <cstddef>
#include <ios>
#include <istream>


ServerResponseStub::ServerResponseStub() = default;


ServerResponseStub::~ServerResponseStub() = default;


std::string ServerResponseStub::body() const
{
	return _body.str();
}


void ServerResponseStub::sendContinue()
{
}


std::ostream& ServerResponseStub::send()
{
	_sent = true;
	return _body;
}


void ServerResponseStub::sendFile(const std::string& /*path*/, const std::string& /*mediaType*/)
{
	_sent = true;
}


void ServerResponseStub::sendBuffer(const void* pBuffer, std::size_t length)
{
	_sent = true;
	_body.write(static_cast<const char*>(pBuffer), static_cast<std::streamsize>(length));
}


void ServerResponseStub::redirect(const std::string& uri, HTTPStatus status)
{
	setStatusAndReason(status);
	set("Location", uri);
	_sent = true;
}


void ServerResponseStub::requireAuthentication(const std::string& /*realm*/)
{
	setStatusAndReason(HTTP_UNAUTHORIZED);
}


bool ServerResponseStub::sent() const
{
	return _sent;
}


ServerRequestStub::ServerRequestStub(const std::string& clientAddress, bool secure, const std::string& body):
	_clientAddress(clientAddress),
	_serverAddress("127.0.0.1", 80),
	_pParams(new Poco::Net::HTTPServerParams),
	_body(body),
	_secure(secure)
{
	if (!body.empty())
	{
		setContentLength64(static_cast<Poco::Int64>(body.size()));
	}
}


ServerRequestStub::~ServerRequestStub() = default;


std::istream& ServerRequestStub::stream()
{
	return _body;
}


const Poco::Net::SocketAddress& ServerRequestStub::clientAddress() const
{
	return _clientAddress;
}


const Poco::Net::SocketAddress& ServerRequestStub::serverAddress() const
{
	return _serverAddress;
}


const Poco::Net::HTTPServerParams& ServerRequestStub::serverParams() const
{
	return *_pParams;
}


ServerResponseStub& ServerRequestStub::response() const
{
	return _response;
}


bool ServerRequestStub::secure() const
{
	return _secure;
}
