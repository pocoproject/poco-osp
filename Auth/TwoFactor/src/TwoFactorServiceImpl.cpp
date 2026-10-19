//
// TwoFactorServiceImpl.cpp
//
// Library: OSP/Auth/TwoFactor
// Package: TwoFactor
// Module:  TwoFactorServiceImpl
//
// Copyright (c) 2020, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties,
// copied or duplicated in any form, in whole or in part.
//


#include "Poco/OSP/Auth/TwoFactor/TwoFactorServiceImpl.h"
#include "Poco/OSP/Auth/TwoFactor/HOTPGenerator.h"
#include "Poco/OSP/Auth/TwoFactor/SecretGenerator.h"
#include "Poco/Crypto/Cipher.h"
#include "Poco/Crypto/CipherFactory.h"
#include "Poco/Timestamp.h"
#include "Poco/Exception.h"


using namespace std::string_literals;


namespace Poco {
namespace OSP {
namespace Auth {
namespace TwoFactor {


const std::string TwoFactorServiceImpl::ATTR_SECRET("auth_tfa_secret");
const std::string TwoFactorServiceImpl::ATTR_LAST_TOTP("auth_tfa_last_totp");
const std::string TwoFactorServiceImpl::SALT("\x52\x71\x23\x03\x22\x49\x11\x24");


TwoFactorServiceImpl::TwoFactorServiceImpl(const AuthAdminService::Ptr pAuthService, const Config& config):
	_pAuthService(pAuthService),
	_config(config)
{
}


TwoFactorServiceImpl::~TwoFactorServiceImpl()
{
}


bool TwoFactorServiceImpl::is2FAEnabled(const std::string& username) const
{
	return _pAuthService->getUserAttribute(username, ATTR_SECRET) != "";
}


void TwoFactorServiceImpl::enable2FA(const std::string& username)
{
	SecretGenerator secretGenerator(_config.secretLengthBits);
	std::string secret = secretGenerator.generate();
	storeSecret(username, secret);
}


void TwoFactorServiceImpl::disable2FA(const std::string& username)
{
	_pAuthService->removeUserAttribute(username, ATTR_SECRET);
	_pAuthService->removeUserAttribute(username, ATTR_LAST_TOTP);
}


TwoFactorService::ValidationResult TwoFactorServiceImpl::validateTOTP(const std::string& username, const std::string& totp)
{
	if (totp.length() != _config.codeDigits) return VALIDATION_FAILED_CODE_INCORRECT;

	std::string lastTOTP = _pAuthService->getUserAttribute(username, ATTR_LAST_TOTP);
	if (totp == lastTOTP) return VALIDATION_FAILED_CODE_ALREADY_USED;

	std::string secret = loadSecret(username);
	HOTPGenerator<Poco::SHA1Engine> hotpGenerator(secret, _config.codeDigits);
	Poco::UInt64 movingFactor = static_cast<Poco::UInt64>(Poco::Timestamp().epochTime())/30;

	for (Poco::UInt64 mv = movingFactor - 1; mv <= movingFactor + 1; mv++)
	{
		if (hotpGenerator.generate(movingFactor) == totp)
		{
			_pAuthService->setUserAttribute(username, ATTR_LAST_TOTP, totp);
			return VALIDATION_SUCCEEDED;
		}
	}
	return VALIDATION_FAILED_CODE_INCORRECT;
}


std::string TwoFactorServiceImpl::secret(const std::string& username) const
{
	return SecretGenerator::toBase32(loadSecret(username));
}


std::string TwoFactorServiceImpl::generateAuthenticatorURI(const std::string& username) const
{
	std::string secret = loadSecret(username);
	return SecretGenerator::toURI(secret, _config.issuer, username);
}


const std::type_info& TwoFactorServiceImpl::type() const
{
	return typeid(TwoFactorServiceImpl);
}


bool TwoFactorServiceImpl::isA(const std::type_info& otherType) const
{
	return otherType == typeid(TwoFactorServiceImpl) || TwoFactorService::isA(otherType);
}


void TwoFactorServiceImpl::storeSecret(const std::string& username, const std::string& secret)
{
	Poco::Crypto::CipherKey key = generateKey();
	Poco::SharedPtr<Poco::Crypto::Cipher> pCipher = Poco::Crypto::CipherFactory::defaultFactory().createCipher(key);
	std::string encrypted = pCipher->encryptString(secret, Poco::Crypto::Cipher::ENC_BASE64_NO_LF);
	_pAuthService->setUserAttribute(username, ATTR_SECRET, encrypted);
}


std::string TwoFactorServiceImpl::loadSecret(const std::string& username) const
{
	std::string encrypted = _pAuthService->getUserAttribute(username, ATTR_SECRET);
	if (encrypted.empty()) throw Poco::IllegalStateException("User does not have TOTP secret", username);

	Poco::Crypto::CipherKey key = generateKey();
	Poco::SharedPtr<Poco::Crypto::Cipher> pCipher = Poco::Crypto::CipherFactory::defaultFactory().createCipher(key);
	return pCipher->decryptString(encrypted, Poco::Crypto::Cipher::ENC_BASE64_NO_LF);
}


Poco::Crypto::CipherKey TwoFactorServiceImpl::generateKey() const
{
	return Poco::Crypto::CipherKey("aes256"s, _config.passphrase, SALT, 10000, "sha256"s);
}


} } } } // namespace Poco::OSP::Auth::TwoFactor
