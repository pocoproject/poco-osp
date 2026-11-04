//
// HTTPStubs.h
//
// Definition of the RequestStub and ResponseStub classes.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef HTTPStubs_INCLUDED
#define HTTPStubs_INCLUDED


#include "Poco/Net/HTTPServerParams.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Net/HTTPServerResponse.h"
#include "Poco/Net/SocketAddress.h"
#include <cstddef>
#include <sstream>
#include <string>


class ResponseStub: public Poco::Net::HTTPServerResponse
	/// A HTTPServerResponse that keeps the body in a string
	/// and sends nothing over the network.
{
public:
	ResponseStub();
	~ResponseStub() override;

	void sendContinue() override;
	std::ostream& send() override;
	void sendFile(const std::string& path, const std::string& mediaType) override;
	void sendBuffer(const void* pBuffer, std::size_t length) override;
	void redirect(const std::string& uri, HTTPStatus status = HTTP_FOUND) override;
	void requireAuthentication(const std::string& realm) override;
	[[nodiscard]] bool sent() const override;

	[[nodiscard]] std::string body() const;
		/// Returns the body sent so far.

private:
	std::ostringstream _body;
	bool _sent = false;
};


class RequestStub: public Poco::Net::HTTPServerRequest
	/// A HTTPServerRequest from 127.0.0.1 to the server localhost:22080,
	/// with the given body and a ResponseStub as response.
{
public:
	RequestStub() = delete;

	RequestStub(const std::string& method, const std::string& uri, const std::string& body);
		/// Creates the request with the Host header field localhost:22080.

	~RequestStub() override;

	[[nodiscard]] std::istream& stream() override;
	[[nodiscard]] const Poco::Net::SocketAddress& clientAddress() const override;
	[[nodiscard]] const Poco::Net::SocketAddress& serverAddress() const override;
	[[nodiscard]] const Poco::Net::HTTPServerParams& serverParams() const override;
	[[nodiscard]] Poco::Net::HTTPServerResponse& response() const override;
	[[nodiscard]] bool secure() const override;

	[[nodiscard]] ResponseStub& responseStub();
		/// Returns the response.

	static const std::string ORIGIN;
		/// The origin of the server, http://localhost:22080.

private:
	Poco::Net::SocketAddress _clientAddress;
	Poco::Net::SocketAddress _serverAddress;
	Poco::Net::HTTPServerParams::Ptr _pParams;
	std::istringstream _body;
	mutable ResponseStub _response;
};


//
// inlines
//
inline ResponseStub& RequestStub::responseStub()
{
	return _response;
}


#endif // HTTPStubs_INCLUDED
