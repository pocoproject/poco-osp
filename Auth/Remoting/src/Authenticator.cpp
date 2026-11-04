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
#include "Poco/OSP/Auth/RequestIdentity.h"


namespace Poco::OSP::Auth::Remoting {


Authenticator::Authenticator() = default;


Authenticator::~Authenticator() = default;


Poco::RemotingNG::AuthenticateResult Authenticator::authenticate(
	const Poco::RemotingNG::Credentials& /*credentials*/,
	Poco::UInt32 /*conversationID*/)
{
	// The dispatcher's identity, not a header field: a client can send one to a listener with its own HTTP server.
	const RequestIdentity* pIdentity = RequestIdentity::current();
	if (pIdentity == nullptr)
	{
		return Poco::RemotingNG::AuthenticateResult(
			Poco::RemotingNG::AuthenticateResult::AUTH_FAILED);
	}

	Poco::RemotingNG::Credentials authCreds;
	authCreds.setAttribute(Poco::RemotingNG::Credentials::ATTR_USERNAME, pIdentity->user());
	return Poco::RemotingNG::AuthenticateResult(
		Poco::RemotingNG::AuthenticateResult::AUTH_DONE, authCreds);
}


} // namespace Poco::OSP::Auth::Remoting
