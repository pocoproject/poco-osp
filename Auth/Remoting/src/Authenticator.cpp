//
// Authenticator.cpp
//
// Library: OSP/Auth/Remoting
// Package: Auth
// Module:  Authenticator
//
// Copyright (c) 2025, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties,
// copied or duplicated in any form, in whole or in part.
//


#include "Poco/OSP/Auth/Remoting/Authenticator.h"
#include "Poco/RemotingNG/Context.h"
#include "Poco/Net/HTTPServerRequest.h"


namespace Poco {
namespace OSP {
namespace Auth {
namespace Remoting {


using namespace std::string_literals;


Authenticator::Authenticator() = default;


Authenticator::~Authenticator() = default;


Poco::RemotingNG::AuthenticateResult Authenticator::authenticate(
	const Poco::RemotingNG::Credentials& /*credentials*/,
	Poco::UInt32 /*conversationID*/)
{
	// Trust the dispatcher-injected X-OSP-Authorized-User header. The dispatcher
	// (WebServerDispatcher::authorize) has already validated the session cookie,
	// HTTP Basic credentials, or Bearer JWT before the request reached here.
	// It removes this header from every request a client sends and sets it only
	// after successful authentication, so a client cannot supply it.
	auto pContext = Poco::RemotingNG::Context::get();
	if (!pContext || !pContext->has("httpRequest"s))
	{
		return Poco::RemotingNG::AuthenticateResult(
			Poco::RemotingNG::AuthenticateResult::AUTH_FAILED);
	}

	auto* pRequest = pContext->getValue<Poco::Net::HTTPServerRequest*>("httpRequest"s);
	if (!pRequest)
	{
		return Poco::RemotingNG::AuthenticateResult(
			Poco::RemotingNG::AuthenticateResult::AUTH_FAILED);
	}

	std::string username = pRequest->get("X-OSP-Authorized-User"s, ""s);
	if (username.empty())
	{
		return Poco::RemotingNG::AuthenticateResult(
			Poco::RemotingNG::AuthenticateResult::AUTH_FAILED);
	}

	Poco::RemotingNG::Credentials authCreds;
	authCreds.setAttribute(Poco::RemotingNG::Credentials::ATTR_USERNAME, username);
	return Poco::RemotingNG::AuthenticateResult(
		Poco::RemotingNG::AuthenticateResult::AUTH_DONE, authCreds);
}


} } } } // namespace Poco::OSP::Auth::Remoting
