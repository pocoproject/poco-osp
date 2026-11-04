//
// AuthService.h
//
// Library: OSP
// Package: Auth
// Module:  AuthService
//
// Definition of the AuthService service interface class.
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#ifndef OSP_Auth_AuthService_INCLUDED
#define OSP_Auth_AuthService_INCLUDED


#include "Poco/OSP/Service.h"
#include "Poco/AutoPtr.h"


namespace Poco::OSP::Auth {


class OSP_API AuthService: public Poco::OSP::Service
	/// The AuthService (authentication/authorization service) is used to
	/// authenticate users using a user name/credentials combination, and to
	/// assign certain permissions to users.
	///
	/// The specific format of user credentials is defined by the actual implementation
	/// of this service. In the simplest form, the credentials are a simple password.
	///
	/// Permissions are simple strings identifying the permissions a specific user has.
	/// The AuthService itself does not care about the syntax and format of permissions.
{
public:
	using Ptr = Poco::AutoPtr<AuthService>;

	AuthService();
		/// Creates the AuthService.

	~AuthService() override;
		/// Destroys the AuthService.

	[[nodiscard]] virtual bool authenticate(const std::string& userName, const std::string& credentials) const = 0;
		/// Checks if the userName and credentials combination is valid.
		///
		/// Returns true if the user specified by userName is known and the specified credentials
		/// confirm the identity of the user. Returns false otherwise.

	[[nodiscard]] virtual bool authenticateAccount(const std::string& userName, const std::string& credentials,
		std::string& accountName) const;
		/// Checks the userName and credentials combination like authenticate().
		/// If the combination is valid, stores in accountName the name under which
		/// this service knows the user and returns true; otherwise returns false and
		/// leaves accountName unchanged.
		///
		/// A user may be able to log in with several names, for example with the
		/// account name or the user principal name of a directory service. The other
		/// members (authorize(), userExists() and the user's attributes) take the
		/// account name, so a caller that keeps the user name for later calls, such
		/// as a session, keeps accountName.
		///
		/// The default implementation calls authenticate() and stores userName.

	[[nodiscard]] virtual bool authorize(const std::string& userName, const std::string& permission) const = 0;
		/// Checks if a user has a specific permission.
		///
		/// Returns true if the user specified by userName has the specified permission.

	[[nodiscard]] virtual bool authorize(const std::string& userName, const std::string& roleOrScope,
		const std::string& permission) const = 0;
		/// Checks if a user has a specific permission within the given role or scope.
		///
		/// If roleOrScope is empty, the result is the same as the two-argument version.
		/// If a non-empty roleOrScope is given, the method checks whether the user
		/// has the given permission within that given role or scope.
		///
		/// Role or scope is defined by the specific implementation. 
		///
		/// Returns true if the user specified by userName has the specified permission.

	[[nodiscard]] virtual bool userExists(const std::string& userName) const = 0;
		/// Checks if a user has exists.
		///
		/// Returns true if the user specified by userName exists, otherwise false.

	[[nodiscard]] virtual bool userEnabled(const std::string& userName) const;
		/// Checks if a user is enabled.
		///
		/// Returns false if the user specified by userName has been disabled
		/// or locked out, otherwise true. The WebServerDispatcher then refuses
		/// the user's sessions and bearer tokens as well.
		///
		/// The default implementation returns true.

	// Service
	[[nodiscard]] const std::type_info& type() const override;
	[[nodiscard]] bool isA(const std::type_info& otherType) const override;
};


} // namespace Poco::OSP::Auth


#endif // OSP_Auth_AuthService_INCLUDED
