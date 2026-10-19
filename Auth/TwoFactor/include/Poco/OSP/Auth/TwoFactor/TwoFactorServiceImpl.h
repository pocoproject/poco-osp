//
// TwoFactorServiceImpl.h
//
// Library: OSP/Auth/TwoFactor
// Package: TwoFactor
// Module:  TwoFactorServiceImpl
//
// Definition of the TwoFactorServiceImpl class.
//
// Copyright (c) 2020, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties,
// copied or duplicated in any form, in whole or in part.
//


#ifndef OSP_Auth_TwoFactor_TwoFactorServiceImpl_INCLUDED
#define OSP_Auth_TwoFactor_TwoFactorServiceImpl_INCLUDED


#include "Poco/OSP/Auth/TwoFactor/TwoFactorService.h"
#include "Poco/OSP/Auth/AuthAdminService.h"
#include "Poco/Crypto/CipherKey.h"
#include "Poco/AutoPtr.h"


namespace Poco {
namespace OSP {
namespace Auth {
namespace TwoFactor {


class TwoFactorServiceImpl: public TwoFactorService
	/// Default implementation of the TwoFactorService.
{
public:
	using Ptr = Poco::AutoPtr<TwoFactorServiceImpl>;

	struct Config
	{
		std::size_t secretLengthBits = 128;
		std::size_t codeDigits = 6;
		std::string passphrase;
		std::string issuer;
	};

	TwoFactorServiceImpl(const AuthAdminService::Ptr pAuthService, const Config& config);
		/// Creates the TwoFactorServiceImpl, using the given AuthAdminService and
		/// config.

	~TwoFactorServiceImpl();
		/// Destroys the TwoFactorServiceImpl.

	// TwoFactorService
	bool is2FAEnabled(const std::string& username) const;
	void enable2FA(const std::string& username);
	void disable2FA(const std::string& username);
	ValidationResult validateTOTP(const std::string& username, const std::string& totp);
	std::string secret(const std::string& username) const;
	std::string generateAuthenticatorURI(const std::string& username) const;

	// Service
	const std::type_info& type() const;
	bool isA(const std::type_info& otherType) const;

protected:
	void storeSecret(const std::string& username, const std::string& secret);
	std::string loadSecret(const std::string& username) const;
	Poco::Crypto::CipherKey generateKey() const;

	static const std::string ATTR_SECRET;
	static const std::string ATTR_LAST_TOTP;
	static const std::string SALT;

private:
	AuthAdminService::Ptr _pAuthService;
	Config _config;
};


} } } } // namespace Poco::OSP::Auth::TwoFactor


#endif // OSP_Auth_TwoFactor_TwoFactorServiceImpl_INCLUDED
