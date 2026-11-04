//
// ServerRequestStub.h
//
// Definition of the ServerRequestStub and ServerResponseStub classes.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef ServerRequestStub_INCLUDED
#define ServerRequestStub_INCLUDED


#include "Poco/Net/HTTPServerParams.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Net/HTTPServerResponse.h"
#include "Poco/Net/SocketAddress.h"
#include <sstream>
#include <string>


class ServerResponseStub: public Poco::Net::HTTPServerResponse
	/// A HTTPServerResponse that writes the body to a string stream
	/// and sends nothing over the network.
{
public:
	ServerResponseStub();
	~ServerResponseStub() override;

	[[nodiscard]] std::string body() const;
		/// Returns the body written so far.

	void sendContinue() override;
	std::ostream& send() override;
	void sendFile(const std::string& path, const std::string& mediaType) override;
	void sendBuffer(const void* pBuffer, std::size_t length) override;
	void redirect(const std::string& uri, HTTPStatus status = HTTP_FOUND) override;
	void requireAuthentication(const std::string& realm) override;
	[[nodiscard]] bool sent() const override;

private:
	std::ostringstream _body;
	bool _sent = false;
};


class ServerRequestStub: public Poco::Net::HTTPServerRequest
	/// A HTTPServerRequest from a given client address, with a given
	/// body and a ServerResponseStub as response.
{
public:
	ServerRequestStub() = delete;

	ServerRequestStub(const std::string& clientAddress, bool secure, const std::string& body = std::string());
		/// Creates a request from the given client address ("host:port").
		/// secure() returns the given secure flag. A non-empty body is
		/// sent with its content length.

	~ServerRequestStub() override;

	[[nodiscard]] std::istream& stream() override;
	[[nodiscard]] const Poco::Net::SocketAddress& clientAddress() const override;
	[[nodiscard]] const Poco::Net::SocketAddress& serverAddress() const override;
	[[nodiscard]] const Poco::Net::HTTPServerParams& serverParams() const override;
	[[nodiscard]] ServerResponseStub& response() const override;
	[[nodiscard]] bool secure() const override;

private:
	Poco::Net::SocketAddress _clientAddress;
	Poco::Net::SocketAddress _serverAddress;
	Poco::Net::HTTPServerParams::Ptr _pParams;
	std::istringstream _body;
	mutable ServerResponseStub _response;
	bool _secure;
};


#endif // ServerRequestStub_INCLUDED
