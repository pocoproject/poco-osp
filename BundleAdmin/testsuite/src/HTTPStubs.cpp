//
// HTTPStubs.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "HTTPStubs.h"
#include "Poco/Net/HTTPMessage.h"
#include "Poco/Net/HTTPResponse.h"
#include "Poco/Net/HTTPServerParams.h"
#include "Poco/Net/HTTPServerResponse.h"
#include <cstddef>
#include <ios>
#include <istream>
#include <ostream>
#include <string>


using namespace std::string_literals;


const std::string RequestStub::ORIGIN("http://localhost:22080");


ResponseStub::ResponseStub() = default;


ResponseStub::~ResponseStub() = default;


void ResponseStub::sendContinue()
{
}


std::ostream& ResponseStub::send()
{
	_sent = true;
	return _body;
}


void ResponseStub::sendFile(const std::string& /*path*/, const std::string& /*mediaType*/)
{
	_sent = true;
}


void ResponseStub::sendBuffer(const void* pBuffer, std::size_t length)
{
	_sent = true;
	_body.write(static_cast<const char*>(pBuffer), static_cast<std::streamsize>(length));
}


void ResponseStub::redirect(const std::string& uri, HTTPStatus status)
{
	setStatusAndReason(status);
	set("Location"s, uri);
	_sent = true;
}


void ResponseStub::requireAuthentication(const std::string& /*realm*/)
{
	setStatusAndReason(HTTP_UNAUTHORIZED);
}


bool ResponseStub::sent() const
{
	return _sent;
}


std::string ResponseStub::body() const
{
	return _body.str();
}


RequestStub::RequestStub(const std::string& method, const std::string& uri, const std::string& body):
	_clientAddress("127.0.0.1"s, 50000),
	_serverAddress("127.0.0.1"s, 22080),
	_pParams(new Poco::Net::HTTPServerParams),
	_body(body)
{
	setMethod(method);
	setURI(uri);
	setVersion(Poco::Net::HTTPMessage::HTTP_1_1);
	set("Host"s, "localhost:22080"s);
	if (!body.empty()) setContentLength(static_cast<std::streamsize>(body.size()));
}


RequestStub::~RequestStub() = default;


std::istream& RequestStub::stream()
{
	return _body;
}


const Poco::Net::SocketAddress& RequestStub::clientAddress() const
{
	return _clientAddress;
}


const Poco::Net::SocketAddress& RequestStub::serverAddress() const
{
	return _serverAddress;
}


const Poco::Net::HTTPServerParams& RequestStub::serverParams() const
{
	return *_pParams;
}


Poco::Net::HTTPServerResponse& RequestStub::response() const
{
	return _response;
}


bool RequestStub::secure() const
{
	return false;
}
