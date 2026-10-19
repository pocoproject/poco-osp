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


namespace Poco {
namespace OSP {
namespace Auth {
namespace TwoFactor {


class OSPAuthTwoFactor_API TwoFactorService: public Poco::OSP::Service
	/// This service helps to implement two-factor authentication
	/// using Time-based One-Time Passwords (TOTPs) specified in
	/// RFC 6238 and supported by Google Authenticator or Authy.
{
public:
	using Ptr = Poco::AutoPtr<TwoFactorService>;

	enum ValidationResult
	{
		VALIDATION_SUCCEEDED,
		VALIDATION_FAILED_CODE_INCORRECT,
		VALIDATION_FAILED_CODE_ALREADY_USED
	};

	virtual bool is2FAEnabled(const std::string& username) const = 0;
		/// Returns true if two-factor authentication has been
		/// enabled for the given username.

	virtual void enable2FA(const std::string& username) = 0;
		/// Enables two-factor authentication for the given
		/// username and generates a secret for the user.

	virtual void disable2FA(const std::string& username) = 0;
		/// Disables two-factor authentication for the given
		/// username and deletes the stored secret for the user.

	virtual ValidationResult validateTOTP(const std::string& username, const std::string& totp) = 0;
		/// Validates the given time-based one-time password.

	virtual std::string secret(const std::string& username) const = 0;
		/// Returns the base32-encoded secret for the given username.

	virtual std::string generateAuthenticatorURI(const std::string& username) const = 0;
		/// Generates a TOTP URI with the secret suitable for encoding as QR code
		/// for importing into Google Authenticator and similar apps.

	// Service
	const std::type_info& type() const;
	bool isA(const std::type_info& otherType) const;

	static const std::string SERVICE_NAME;
};


} } } } // namespace Poco::OSP::Auth::TwoFactor


#endif // OSP_Auth_TwoFactor_TwoFactorService_INCLUDED
