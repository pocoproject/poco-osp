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
#include "Poco/OSP/Auth/TwoFactor/TwoFactorService.h"
#include "Poco/OSP/Auth/AuthAdminService.h"
#include "Poco/Crypto/Cipher.h"
#include "Poco/Crypto/CipherFactory.h"
#include "Poco/Crypto/CryptoException.h"
#include "Poco/Base64Decoder.h"
#include "Poco/Base64Encoder.h"
#include "Poco/StreamCopier.h"
#include "Poco/String.h"
#include "Poco/Timestamp.h"
#include "Poco/DateTime.h"
#include "Poco/DigestEngine.h"
#include "Poco/NumberParser.h"
#include "Poco/Exception.h"
#include "Poco/Logger.h"
#include "Poco/Mutex.h"
#include "Poco/Types.h"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <cstddef>
#include <memory>
#include <sstream>
#include <string>


using namespace std::string_literals;


namespace {
	using CipherContext = std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)>;


	void checkOpenSSL(int rc)
	{
		if (rc != 1) throw Poco::Crypto::OpenSSLException();
	}


	[[nodiscard]] CipherContext newCipherContext()
	{
		CipherContext pContext(EVP_CIPHER_CTX_new(), &EVP_CIPHER_CTX_free);
		if (pContext == nullptr) throw Poco::Crypto::OpenSSLException();
		return pContext;
	}


	// OpenSSL takes byte buffers as unsigned char.

	[[nodiscard]] unsigned char* bytes(std::string& str, std::size_t offset = 0)
	{
		// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
		return reinterpret_cast<unsigned char*>(&str[offset]);
	}


	[[nodiscard]] const unsigned char* bytes(const std::string& str)
	{
		// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
		return reinterpret_cast<const unsigned char*>(str.data());
	}


	[[nodiscard]] std::string toBase64(const std::string& data)
	{
		std::ostringstream ostr;
		Poco::Base64Encoder encoder(ostr);
		encoder.rdbuf()->setLineLength(0);
		encoder << data;
		encoder.close();
		return ostr.str();
	}


	[[nodiscard]] std::string fromBase64(const std::string& base64)
	{
		std::istringstream istr(base64);
		Poco::Base64Decoder decoder(istr);
		std::string data;
		Poco::StreamCopier::copyToString(decoder, data);
		return data;
	}
}


namespace Poco::OSP::Auth::TwoFactor {


const std::string TwoFactorServiceImpl::ATTR_SECRET("auth_tfa_secret");
const std::string TwoFactorServiceImpl::ATTR_LAST_STEP("auth_tfa_last_step");
const std::string TwoFactorServiceImpl::ATTR_FAILURES("auth_tfa_failures");
const std::string TwoFactorServiceImpl::ATTR_LAST_TOTP("auth_tfa_last_totp");
const std::string TwoFactorServiceImpl::SALT("\x52\x71\x23\x03\x22\x49\x11\x24");
const std::string TwoFactorServiceImpl::FORMAT_PREFIX("v=2;");


TwoFactorServiceImpl::TwoFactorServiceImpl(const AuthAdminService::Ptr pAuthService, const Config& config,
	Poco::Logger& logger):
	_pAuthService(pAuthService),
	_config(config),
	_logger(logger),
	_key(KEY_SIZE, '\0')
{
	if (_config.codeDigits < MIN_CODE_DIGITS || _config.codeDigits > MAX_CODE_DIGITS)
	{
		throw Poco::InvalidArgumentException("auth.twofactor.codeDigits must be from 6 to 8",
			std::to_string(_config.codeDigits));
	}
	if (_config.secretLengthBits < MIN_SECRET_LENGTH_BITS || _config.secretLengthBits % BITS_PER_BYTE != 0)
	{
		throw Poco::InvalidArgumentException("auth.twofactor.secretLengthBits must be a multiple of 8 and at least 80",
			std::to_string(_config.secretLengthBits));
	}
	if (_config.keySalt.empty()) throw Poco::InvalidArgumentException("The TOTP secret key salt must not be empty");

	checkOpenSSL(PKCS5_PBKDF2_HMAC(
		_config.passphrase.data(), static_cast<int>(_config.passphrase.size()),
		bytes(_config.keySalt), static_cast<int>(_config.keySalt.size()),
		KEY_DERIVATION_ITERATIONS, EVP_sha256(),
		static_cast<int>(_key.size()), bytes(_key)));
}


TwoFactorServiceImpl::~TwoFactorServiceImpl()
{
	Poco::secureClear(_key);
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
	_pAuthService->removeAuthAttribute(username, ATTR_SECRET);
	_pAuthService->removeAuthAttribute(username, ATTR_LAST_STEP);
	_pAuthService->removeAuthAttribute(username, ATTR_FAILURES);
	_pAuthService->removeAuthAttribute(username, ATTR_LAST_TOTP);
}


TwoFactorService::ValidationResult TwoFactorServiceImpl::validateTOTP(const std::string& username, const std::string& totp)
{
	// Serialised, so that two requests with the same code cannot both pass the check of the last step.
	Poco::FastMutex::ScopedLock lock(_mutex);

	if (_pAuthService->isUserLockedOut(username)) return VALIDATION_FAILED_CODE_INCORRECT;

	if (totp.length() != _config.codeDigits)
	{
		recordFailure(username);
		return VALIDATION_FAILED_CODE_INCORRECT;
	}

	std::string secret = loadSecret(username);
	HOTPGenerator<Poco::SHA1Engine> hotpGenerator(secret, _config.codeDigits);
	Poco::UInt64 movingFactor = static_cast<Poco::UInt64>(now().epochTime())/TIME_STEP_SECONDS;

	Poco::UInt64 lastStep = 0;
	const bool hasLastStep =
		Poco::NumberParser::tryParseUnsigned64(_pAuthService->getUserAttribute(username, ATTR_LAST_STEP), lastStep);
	const Poco::DigestEngine::Digest given(totp.begin(), totp.end());
	bool alreadyUsed = false;
	for (Poco::UInt64 mv = movingFactor - 1; mv <= movingFactor + 1; mv++)
	{
		const std::string code = hotpGenerator.generate(mv);
		if (Poco::DigestEngine::constantTimeEquals(Poco::DigestEngine::Digest(code.begin(), code.end()), given))
		{
			if (hasLastStep && mv <= lastStep)
			{
				alreadyUsed = true;
			}
			else
			{
				_pAuthService->setAuthAttribute(username, ATTR_LAST_STEP, std::to_string(mv));
				_pAuthService->removeAuthAttribute(username, ATTR_LAST_TOTP);
				_pAuthService->removeAuthAttribute(username, ATTR_FAILURES);
				// A secret in the format without version is not bound to the user.
				if (_pAuthService->getUserAttribute(username, ATTR_SECRET).compare(0, FORMAT_PREFIX.size(),
					FORMAT_PREFIX) != 0)
				{
					storeSecret(username, secret);
				}
				return VALIDATION_SUCCEEDED;
			}
		}
	}
	if (alreadyUsed) return VALIDATION_FAILED_CODE_ALREADY_USED;

	recordFailure(username);
	return VALIDATION_FAILED_CODE_INCORRECT;
}


void TwoFactorServiceImpl::recordFailure(const std::string& username)
{
	if (_config.maxFailures == 0) return;

	Poco::UInt64 failures = 0;
	if (!Poco::NumberParser::tryParseUnsigned64(_pAuthService->getUserAttribute(username, ATTR_FAILURES), failures))
	{
		failures = 0;
	}
	++failures;
	if (failures >= _config.maxFailures)
	{
		_pAuthService->lockOutUser(username, Poco::DateTime(now()) + _config.lockOutPeriod);
		_pAuthService->removeAuthAttribute(username, ATTR_FAILURES);
		_logger.notice("Locked out user %s for %d seconds after %?u wrong two-factor authentication codes."s, username,
			_config.lockOutPeriod.totalSeconds(), failures);
	}
	else
	{
		_pAuthService->setAuthAttribute(username, ATTR_FAILURES, std::to_string(failures));
	}
}


Poco::Timestamp TwoFactorServiceImpl::now() const
{
	return Poco::Timestamp();
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
	_pAuthService->setAuthAttribute(username, ATTR_SECRET, encryptSecret(username, secret));
}


std::string TwoFactorServiceImpl::loadSecret(const std::string& username) const
{
	std::string encrypted = _pAuthService->getUserAttribute(username, ATTR_SECRET);
	if (encrypted.empty()) throw Poco::IllegalStateException("User does not have TOTP secret", username);

	if (encrypted.compare(0, FORMAT_PREFIX.size(), FORMAT_PREFIX) == 0)
	{
		return decryptSecret(username, encrypted.substr(FORMAT_PREFIX.size()));
	}

	Poco::Crypto::CipherKey key = generateKey();
	Poco::SharedPtr<Poco::Crypto::Cipher> pCipher = Poco::Crypto::CipherFactory::defaultFactory().createCipher(key);
	return pCipher->decryptString(encrypted, Poco::Crypto::Cipher::ENC_BASE64_NO_LF);
}


Poco::Crypto::CipherKey TwoFactorServiceImpl::generateKey() const
{
	return Poco::Crypto::CipherKey("aes256"s, _config.passphrase, SALT, 10000, "sha256"s);
}


std::string TwoFactorServiceImpl::encryptSecret(const std::string& username, const std::string& secret) const
{
	// Stored as FORMAT_PREFIX followed by the Base64 encoding of IV, ciphertext and tag.
	std::string data(IV_SIZE + secret.size() + TAG_SIZE, '\0');
	checkOpenSSL(RAND_bytes(bytes(data), static_cast<int>(IV_SIZE)));

	CipherContext pContext = newCipherContext();
	checkOpenSSL(EVP_EncryptInit_ex(pContext.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr));
	checkOpenSSL(EVP_CIPHER_CTX_ctrl(pContext.get(), EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(IV_SIZE), nullptr));
	checkOpenSSL(EVP_EncryptInit_ex(pContext.get(), nullptr, nullptr, bytes(_key), bytes(data)));
	int length = 0;
	checkOpenSSL(EVP_EncryptUpdate(pContext.get(), nullptr, &length, bytes(username),
		static_cast<int>(username.size())));
	checkOpenSSL(EVP_EncryptUpdate(pContext.get(), bytes(data, IV_SIZE), &length, bytes(secret),
		static_cast<int>(secret.size())));
	checkOpenSSL(EVP_EncryptFinal_ex(pContext.get(), bytes(data, IV_SIZE + static_cast<std::size_t>(length)), &length));
	checkOpenSSL(EVP_CIPHER_CTX_ctrl(pContext.get(), EVP_CTRL_GCM_GET_TAG, static_cast<int>(TAG_SIZE),
		bytes(data, IV_SIZE + secret.size())));
	return FORMAT_PREFIX + toBase64(data);
}


std::string TwoFactorServiceImpl::decryptSecret(const std::string& username, const std::string& encrypted) const
{
	std::string data = fromBase64(encrypted);
	if (data.size() < IV_SIZE + TAG_SIZE)
		throw Poco::Crypto::CryptoException("The stored TOTP secret is truncated", username);

	const std::size_t secretSize = data.size() - IV_SIZE - TAG_SIZE;
	std::string secret(secretSize, '\0');
	CipherContext pContext = newCipherContext();
	checkOpenSSL(EVP_DecryptInit_ex(pContext.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr));
	checkOpenSSL(EVP_CIPHER_CTX_ctrl(pContext.get(), EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(IV_SIZE), nullptr));
	checkOpenSSL(EVP_DecryptInit_ex(pContext.get(), nullptr, nullptr, bytes(_key), bytes(data)));
	int length = 0;
	checkOpenSSL(EVP_DecryptUpdate(pContext.get(), nullptr, &length, bytes(username),
		static_cast<int>(username.size())));
	checkOpenSSL(EVP_DecryptUpdate(pContext.get(), bytes(secret), &length, bytes(data, IV_SIZE),
		static_cast<int>(secretSize)));
	checkOpenSSL(EVP_CIPHER_CTX_ctrl(pContext.get(), EVP_CTRL_GCM_SET_TAG, static_cast<int>(TAG_SIZE),
		bytes(data, IV_SIZE + secretSize)));
	if (EVP_DecryptFinal_ex(pContext.get(), bytes(secret, static_cast<std::size_t>(length)), &length) != 1)
	{
		Poco::secureClear(secret);
		throw Poco::Crypto::CryptoException("The stored TOTP secret is not valid for the user", username);
	}
	return secret;
}


} // namespace Poco::OSP::Auth::TwoFactor
