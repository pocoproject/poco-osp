//
// Session.h
//
// Library: OSP/Shell
// Package: Shell
// Module:  Session
//
// Definition of the Session class.
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties, 
// copied or duplicated in any form, in whole or in part.
//


#ifndef OSP_Shell_Session_INCLUDED
#define OSP_Shell_Session_INCLUDED


#include "Poco/OSP/Shell/Shell.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/Auth/AuthService.h"


namespace Poco::OSP::Shell {


class OSPShell_API Session
	/// The Session class provides user authentication and
	/// authorization for the CommandProcessor.
	///
	/// It is also passed as the first argument to
	/// the execute() member function of Command, so
	/// that command implementations can perform
	/// custom authorization.
{
public:
	Session(Poco::OSP::BundleContext::Ptr pContext, const std::string& authServiceName);
		/// Creates the Session, using the given bundle context and
		/// the given service name of the authentication service.
		///
		/// An empty service name disables authentication and authorization.

	~Session();
		/// Destroys the Session.

	Session() = delete;
	Session(const Session&) = delete;
	Session& operator = (const Session&) = delete;

	[[nodiscard]] bool login(const std::string& userName, const std::string& credentials);
		/// Attempts to authenticate the user by checking the
		/// given userName and credentials with the authentication service.
		///
		/// Returns true if the user was successfully authenticated, false otherwise.
		///
		/// The session keeps the account name that the authentication service
		/// returns for the login (see Poco::OSP::Auth::AuthService::authenticateAccount())
		/// as the name of its user, which authorize() and user() use.
		///
		/// If no service with the configured name is registered, the login is
		/// rejected. Only an empty service name disables authentication.
		///
		/// If the authentication service throws an exception, or the service
		/// registered with the configured name is not an authentication
		/// service, the login is rejected and the error is logged.
		///
		/// A rejected login leaves the session without a user, also if a user
		/// was logged in before.
	
	[[nodiscard]] bool authorize(const std::string& permission) const;
		/// Returns true if the user is authenticated and has the
		/// given permission, false otherwise.
		///
		/// An empty permission, or an empty service name, only requires the
		/// user to be authenticated. If no service with the configured name is
		/// registered, every other permission is denied.
		///
		/// If the authentication service throws an exception, or the service
		/// registered with the configured name is not an authentication
		/// service, the permission is denied and the error is logged.

	void loginWithKey(const std::string& userName);
		/// Marks the user as authenticated without checking credentials.
		/// Used for the SSH sessions, whose user the SSH server has already
		/// authenticated with a password or a public key.

	[[nodiscard]] bool authenticated() const;
		/// Returns true if the user is authenticated.
	
	[[nodiscard]] const std::string& user() const;
		/// Returns the name of the current user, or an empty string
		/// if no user is logged in.

	[[nodiscard]] int failedLogins() const;
		/// Returns the number of logins rejected in this session.

protected:
	[[nodiscard]] Poco::OSP::Auth::AuthService::Ptr authService() const;
		/// Returns the authentication service, or a null pointer if the service
		/// name is empty or no service with this name is registered. The service
		/// is looked up for each call, so that a service that has been
		/// unregistered is no longer used.

private:
	Poco::OSP::BundleContext::Ptr _pContext;
	std::string _authServiceName;
	std::string _userName;
	bool _authenticated = false;
	int _failedLogins = 0;
};


//
// inlines
//
inline bool Session::authenticated() const
{
	return _authenticated;
}


inline const std::string& Session::user() const
{
	return _userName;
}


inline int Session::failedLogins() const
{
	return _failedLogins;
}


} // namespace Poco::OSP::Shell


#endif // OSP_Shell_Session_INCLUDED
