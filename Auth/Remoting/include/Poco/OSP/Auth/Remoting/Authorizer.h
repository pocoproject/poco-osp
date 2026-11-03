//
// Authorizer.h
//
// Library: OSP/Auth/Remoting
// Package: Auth
// Module:  Authorizer
//
// Definition of the Authorizer class.
//
// Copyright (c) 2025, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties,
// copied or duplicated in any form, in whole or in part.
//


#ifndef OSP_Auth_Remoting_Authorizer_INCLUDED
#define OSP_Auth_Remoting_Authorizer_INCLUDED


#include "Poco/OSP/Auth/Remoting/Remoting.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/RemotingNG/Authorizer.h"
#include "Poco/AutoPtr.h"


namespace Poco::OSP::Auth::Remoting {


class OSPAuthRemoting_API Authorizer: public Poco::RemotingNG::Authorizer
	/// A RemotingNG Authorizer that delegates permission checks to the
	/// OSP AuthService.
	///
	/// The authenticated username and optional scope are read from the
	/// X-OSP-Authorized-User and X-OSP-Authorized-Scope HTTP headers set
	/// by the OSP WebServerDispatcher after successful session, HTTP Basic
	/// or Bearer JWT authentication. The authorization check is then
	/// delegated to the AuthService registered under the service name
	/// "osp.auth".
	///
	/// When the X-OSP-Authorized-Scope header is present (Bearer JWT),
	/// the three-argument AuthService::authorize(user, scope, permission)
	/// overload is used, which requires the permission to be granted by
	/// both the user and the scope role.
	///
	/// This Authorizer does not fall back to Basic credentials present in
	/// the RemotingNG Context: for endpoints with URL-level permissions the
	/// dispatcher has already validated the credentials; for endpoints
	/// without URL-level permissions, trusting the Basic username would
	/// bypass password verification.
{
public:
	using Ptr = Poco::AutoPtr<Authorizer>;

	explicit Authorizer(Poco::OSP::BundleContext::Ptr pContext);
		/// Creates the Authorizer using the given BundleContext for
		/// AuthService lookup.

	~Authorizer() override;
		/// Destroys the Authorizer.

	[[nodiscard]] bool authorize(const std::string& method, const std::string& permission) override;
		/// Returns true if the user identified by X-OSP-Authorized-User is
		/// granted the given permission by the OSP AuthService, false
		/// otherwise.

private:
	Poco::OSP::BundleContext::Ptr _pContext;
};


} // namespace Poco::OSP::Auth::Remoting


#endif // OSP_Auth_Remoting_Authorizer_INCLUDED
