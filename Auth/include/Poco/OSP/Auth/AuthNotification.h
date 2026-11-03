//
// AuthNotification.h
//
// Library: OSP
// Package: Auth
// Module:  AuthNotification
//
// Definition of the authentication notification class.
//
// Copyright (c) 2025, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties,
// copied or duplicated in any form, in whole or in part.
//


#ifndef OSP_Auth_AuthNotification_INCLUDED
#define OSP_Auth_AuthNotification_INCLUDED

#include "Poco/OSP/Auth/Auth.h"
#include "Poco/OSP/Auth/AuthTypes.h"
#include "Poco/Notification.h"

#include <string>

namespace Poco::OSP::Auth {


class OSPAuth_API AuthNotification: public Poco::Notification
	/// Notification posted on successful login.
{
public:
	using Ptr = Poco::AutoPtr<AuthNotification>;

	AuthNotification(bool isExternal,
		const std::string& userName,
		const std::string& fullName,
		const Roles& roles,
		const Permissions& permissions,
		const Attributes& attributes
	);

	~AuthNotification() override;

	[[nodiscard]] bool isExternal() const;
	[[nodiscard]] const std::string& userName() const;
	[[nodiscard]] const std::string& fullName() const;
	[[nodiscard]] const Roles& roles() const;
	[[nodiscard]] const Permissions& permissions() const;
		/// Returns the permissions granted to the user directly,
		/// without the permissions of the user's roles.

	[[nodiscard]] const Attributes& attributes() const;

private:

	bool _isExternal { false };
	std::string _userName;
	std::string _fullName;
	Roles _roles;
	Permissions _permissions;
	Attributes _attributes;

};


//
// inlines
//

inline bool AuthNotification::isExternal() const
{
	return _isExternal;
}

inline const std::string& AuthNotification::userName() const
{
	return _userName;
}

inline const std::string& AuthNotification::fullName() const
{
	return _fullName;
}

inline const Roles& AuthNotification::roles() const
{
	return _roles;
}

inline const Permissions& AuthNotification::permissions() const
{
	return _permissions;
}

inline const Attributes& AuthNotification::attributes() const
{
	return _attributes;
}


} // namespace Poco::OSP::Auth

#endif // OSP_Auth_AuthNotification_INCLUDED
