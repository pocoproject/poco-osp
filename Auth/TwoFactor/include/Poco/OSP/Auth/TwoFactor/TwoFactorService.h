//
// TwoFactorService.h
//
// Library: OSP/Auth/TwoFactor
// Package: TwoFactor
// Module:  TwoFactorService
//
// Definition of the TwoFactorService class.
//
// Copyright (c) 2020, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties,
// copied or duplicated in any form, in whole or in part.
//


#ifndef OSP_Auth_TwoFactor_TwoFactorService_INCLUDED
#define OSP_Auth_TwoFactor_TwoFactorService_INCLUDED


#include "Poco/OSP/Auth/TwoFactor/TwoFactor.h"
#include "Poco/OSP/Service.h"
#include "Poco/AutoPtr.h"


namespace Poco::OSP::Auth::TwoFactor {


class OSPAuthTwoFactor_API TwoFactorService: public Poco::OSP::Service
	/// This service helps to implement two-factor authentication
	/// using Time-based One-Time Passwords (TOTPs) specified in
	/// RFC 6238 and supported by Google Authenticator or Authy.
	///
	/// The username of each member is the account name of the user that
	/// AuthService::authenticateAccount() returns, so that a user who can
	/// log in with several names has one secret.
	///
	/// Note: only an application's own login handler checks the second
	/// factor, by calling validateTOTP() after it has accepted the password.
	/// HTTP Basic authentication, bearer tokens, the shell and the metrics
	/// server accept a user without it; a web server mapping that must require
	/// it allows only session authentication (authMethods="session").
{
public:
	using Ptr = Poco::AutoPtr<TwoFactorService>;

	enum ValidationResult
	{
		VALIDATION_SUCCEEDED,
		VALIDATION_FAILED_CODE_INCORRECT,
		VALIDATION_FAILED_CODE_ALREADY_USED
	};

	[[nodiscard]] virtual bool is2FAEnabled(const std::string& username) const = 0;
		/// Returns true if two-factor authentication has been
		/// enabled for the given username.

	virtual void enable2FA(const std::string& username) = 0;
		/// Enables two-factor authentication for the given
		/// username and generates a secret for the user.

	virtual void disable2FA(const std::string& username) = 0;
		/// Disables two-factor authentication for the given
		/// username and deletes the stored secret for the user.

	[[nodiscard]] virtual ValidationResult validateTOTP(const std::string& username, const std::string& totp) = 0;
		/// Validates the given time-based one-time password.

	[[nodiscard]] virtual std::string secret(const std::string& username) const = 0;
		/// Returns the base32-encoded secret for the given username.

	[[nodiscard]] virtual std::string generateAuthenticatorURI(const std::string& username) const = 0;
		/// Generates a TOTP URI with the secret suitable for encoding as QR code
		/// for importing into Google Authenticator and similar apps.

	// Service
	[[nodiscard]] const std::type_info& type() const override;
	[[nodiscard]] bool isA(const std::type_info& otherType) const override;

	static const std::string SERVICE_NAME;
};


} // namespace Poco::OSP::Auth::TwoFactor


#endif // OSP_Auth_TwoFactor_TwoFactorService_INCLUDED
