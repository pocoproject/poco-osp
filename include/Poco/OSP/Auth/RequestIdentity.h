//
// RequestIdentity.h
//
// Library: OSP
// Package: Auth
// Module:  RequestIdentity
//
// Definition of the RequestIdentity class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef OSP_Auth_RequestIdentity_INCLUDED
#define OSP_Auth_RequestIdentity_INCLUDED


#include "Poco/OSP/OSP.h"
#include <string>


namespace Poco::OSP::Auth {


class OSP_API RequestIdentity
	/// The identity of the request that the current thread handles: the user
	/// name and the scope that the WebServerDispatcher has authenticated.
	///
	/// The WebServerDispatcher sets it while the request handler runs, for a request that
	/// WebServerDispatcher::authorize() has accepted for a user (a path that requires a permission).
	/// Code that the handler calls in the same thread, such as a RemotingNG Authenticator, obtains it
	/// with current(). A request that has not passed the dispatcher has no identity, and a client
	/// cannot supply one.
	///
	/// RequestIdentity objects can be nested.
{
public:
	RequestIdentity() = delete;

	POCO_NODISCARD_CTOR RequestIdentity(const std::string& user, const std::string& scope);
		/// Makes the given user name and scope the identity of the current thread
		/// until the RequestIdentity is destroyed. An empty scope means no scope.

	~RequestIdentity();
		/// Restores the identity that was current when the RequestIdentity was created.

	RequestIdentity(const RequestIdentity&) = delete;
	RequestIdentity(RequestIdentity&&) = delete;
	RequestIdentity& operator = (const RequestIdentity&) = delete;
	RequestIdentity& operator = (RequestIdentity&&) = delete;

	[[nodiscard]] static const RequestIdentity* current();
		/// Returns the identity of the current thread, or nullptr if it has none.

	[[nodiscard]] const std::string& user() const;
		/// Returns the user name.

	[[nodiscard]] const std::string& scope() const;
		/// Returns the scope, or an empty string if the identity has no scope.

private:
	std::string _user;
	std::string _scope;
	const RequestIdentity* _pPrevious;
};


//
// inlines
//
inline const std::string& RequestIdentity::user() const
{
	return _user;
}


inline const std::string& RequestIdentity::scope() const
{
	return _scope;
}


} // namespace Poco::OSP::Auth


#endif // OSP_Auth_RequestIdentity_INCLUDED
