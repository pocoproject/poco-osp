//
// Authenticator.h
//
// Library: OSP/Auth/Remoting
// Package: Auth
// Module:  Authenticator
//
// Definition of the Authenticator class.
//
// Copyright (c) 2025, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties,
// copied or duplicated in any form, in whole or in part.
//


#ifndef OSP_Auth_Remoting_Authenticator_INCLUDED
#define OSP_Auth_Remoting_Authenticator_INCLUDED


#include "Poco/OSP/Auth/Remoting/Remoting.h"
#include "Poco/RemotingNG/Authenticator.h"
#include "Poco/AutoPtr.h"


namespace Poco {
namespace OSP {
namespace Auth {
namespace Remoting {


class OSPAuthRemoting_API Authenticator: public Poco::RemotingNG::Authenticator
	/// A RemotingNG Authenticator that trusts the identity already established
	/// by the OSP WebServerDispatcher (via session cookie, HTTP Basic, or
	/// Bearer JWT).
	///
	/// The dispatcher writes the authenticated username to the
	/// X-OSP-Authorized-User HTTP header before the request reaches the
	/// RemotingNG skeleton. This Authenticator verifies that the header is
	/// present and non-empty and returns the username as the authenticated
	/// identity.
	///
	/// This approach avoids duplicating authentication logic: the dispatcher
	/// is the single source of truth for authentication, and this class only
	/// gates access to RemotingNG endpoints behind a valid dispatcher-injected
	/// identity.
	///
	/// The dispatcher removes the X-OSP-Authorized-User and
	/// X-OSP-Authorized-Scope headers that a client sends, for every mapping,
	/// and sets them only for a request it has authenticated. For an endpoint
	/// mapped without a URL-level permission the header is therefore absent
	/// and authentication fails.
{
public:
	using Ptr = Poco::AutoPtr<Authenticator>;

	Authenticator();
		/// Creates the Authenticator.

	~Authenticator() override;
		/// Destroys the Authenticator.

	Poco::RemotingNG::AuthenticateResult authenticate(
		const Poco::RemotingNG::Credentials& credentials,
		Poco::UInt32 conversationID = 0) override;
		/// Returns AUTH_DONE with the username from X-OSP-Authorized-User if
		/// the header is present and non-empty, AUTH_FAILED otherwise.
};


} } } } // namespace Poco::OSP::Auth::Remoting


#endif // OSP_Auth_Remoting_Authenticator_INCLUDED
