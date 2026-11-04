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


namespace Poco::OSP::Auth::Remoting {


class OSPAuthRemoting_API Authenticator: public Poco::RemotingNG::Authenticator
	/// A RemotingNG Authenticator that trusts the identity already established
	/// by the OSP WebServerDispatcher (via session cookie, HTTP Basic, or
	/// Bearer token).
	///
	/// The dispatcher makes the authenticated user name the
	/// Poco::OSP::Auth::RequestIdentity of the thread that runs the request
	/// handler and the RemotingNG skeleton; this Authenticator returns it.
	///
	/// A request that the dispatcher has not passed on, such as one received by a
	/// listener with its own HTTP server, or one for an endpoint mapped without a
	/// URL-level permission, has no identity and fails. The credentials that
	/// RemotingNG passes are not used.
{
public:
	using Ptr = Poco::AutoPtr<Authenticator>;

	Authenticator();
		/// Creates the Authenticator.

	~Authenticator() override;
		/// Destroys the Authenticator.

	[[nodiscard]] Poco::RemotingNG::AuthenticateResult authenticate(
		const Poco::RemotingNG::Credentials& credentials,
		Poco::UInt32 conversationID = 0) override;
		/// Returns AUTH_DONE with the user name of the RequestIdentity of the
		/// current thread, or AUTH_FAILED if the thread has none.
};


} // namespace Poco::OSP::Auth::Remoting


#endif // OSP_Auth_Remoting_Authenticator_INCLUDED
