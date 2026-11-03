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
#include "Poco/Logger.h"
#include "Poco/Mutex.h"
#include "Poco/Timespan.h"
#include "Poco/Timestamp.h"


namespace Poco::OSP::Auth::TwoFactor {


class OSPAuthTwoFactor_API TwoFactorServiceImpl: public TwoFactorService
	/// Default implementation of the TwoFactorService.
	///
	/// A code is accepted for the current time step and the steps before
	/// and after it, and only for a step later than the step of the code
	/// accepted last for the user (RFC 6238, section 5.2). After maxFailures
	/// consecutive failed validations, the user is locked out for
	/// lockOutPeriod through AuthAdminService::lockOutUser().
	///
	/// The secrets are stored encrypted with AES-256-GCM, with a random IV
	/// per secret and the user name as additional authenticated data, so a
	/// stored secret is only valid for the user it was stored for. The key
	/// is derived with PBKDF2-HMAC-SHA256 from the passphrase and the key
	/// salt. A secret in the format without version prefix (AES-256-CBC
	/// with a key derived from the passphrase only) is read as well, and
	/// stored again in the versioned format after the first successful
	/// validation of a code.
{
public:
	using Ptr = Poco::AutoPtr<TwoFactorServiceImpl>;

	struct Config
	{
		std::size_t secretLengthBits = 128;
			/// A multiple of 8, at least 80.
		std::size_t codeDigits = 6;
			/// From 6 to 8.
		std::string passphrase;
		std::string issuer;
		std::string keySalt;
			/// The salt for deriving the key from the passphrase; random and
			/// kept for the lifetime of the installation, and the same for all
			/// instances that share one auth database. Must not be empty.
		static constexpr std::size_t DEFAULT_MAX_FAILURES = 5;
		static constexpr long DEFAULT_LOCK_OUT_SECONDS = 600;

		std::size_t maxFailures = DEFAULT_MAX_FAILURES;
			/// The number of consecutive failed validations after which
			/// the user is locked out. 0 disables the lock-out.
		Poco::Timespan lockOutPeriod{DEFAULT_LOCK_OUT_SECONDS, 0};
			/// The time for which the user is locked out.
	};

	TwoFactorServiceImpl(const AuthAdminService::Ptr pAuthService, const Config& config, Poco::Logger& logger);
		/// Creates the TwoFactorServiceImpl, using the given AuthAdminService,
		/// config and logger. A lock-out after failed validations is logged
		/// to the logger.
		///
		/// Throws a Poco::InvalidArgumentException if the configuration is not valid.

	~TwoFactorServiceImpl() override;
		/// Destroys the TwoFactorServiceImpl.

	TwoFactorServiceImpl() = delete;
	TwoFactorServiceImpl(const TwoFactorServiceImpl&) = delete;
	TwoFactorServiceImpl(TwoFactorServiceImpl&&) = delete;
	TwoFactorServiceImpl& operator = (const TwoFactorServiceImpl&) = delete;
	TwoFactorServiceImpl& operator = (TwoFactorServiceImpl&&) = delete;

	// TwoFactorService
	[[nodiscard]] bool is2FAEnabled(const std::string& username) const override;
	void enable2FA(const std::string& username) override;
	void disable2FA(const std::string& username) override;
	ValidationResult validateTOTP(const std::string& username, const std::string& totp) override;
		/// Validates the code. Returns VALIDATION_FAILED_CODE_INCORRECT
		/// without checking the code while the user is locked out.
	std::string secret(const std::string& username) const override;
	[[nodiscard]] std::string generateAuthenticatorURI(const std::string& username) const override;

	// Service
	[[nodiscard]] const std::type_info& type() const override;
	[[nodiscard]] bool isA(const std::type_info& otherType) const override;

protected:
	[[nodiscard]] virtual Poco::Timestamp now() const;
		/// Returns the current time, which selects the time step of
		/// validateTOTP(). Can be overridden, for example in tests.

	void storeSecret(const std::string& username, const std::string& secret);
	[[nodiscard]] std::string loadSecret(const std::string& username) const;
	[[nodiscard]] Poco::Crypto::CipherKey generateKey() const;
		/// Returns the key of the format without version prefix.

	static const std::string ATTR_SECRET;
	static const std::string ATTR_LAST_STEP;
	static const std::string ATTR_FAILURES;
	static const std::string ATTR_LAST_TOTP;
		/// The last accepted code, kept by an older version instead of ATTR_LAST_STEP.
	static const std::string SALT;

private:
	void recordFailure(const std::string& username);
	[[nodiscard]] std::string encryptSecret(const std::string& username, const std::string& secret) const;
	[[nodiscard]] std::string decryptSecret(const std::string& username, const std::string& encrypted) const;

	static constexpr Poco::UInt64 TIME_STEP_SECONDS = 30;
	static constexpr std::size_t MIN_CODE_DIGITS = 6;
	static constexpr std::size_t MAX_CODE_DIGITS = 8;
	static constexpr std::size_t MIN_SECRET_LENGTH_BITS = 80;
	static constexpr std::size_t BITS_PER_BYTE = 8;
	static constexpr int KEY_DERIVATION_ITERATIONS = 600000;
	static constexpr std::size_t KEY_SIZE = 32;
	static constexpr std::size_t IV_SIZE = 12;
	static constexpr std::size_t TAG_SIZE = 16;
	static const std::string FORMAT_PREFIX;

	AuthAdminService::Ptr _pAuthService;
	Config _config;
	Poco::Logger& _logger;
	std::string _key;
	Poco::FastMutex _mutex;
};


} // namespace Poco::OSP::Auth::TwoFactor


#endif // OSP_Auth_TwoFactor_TwoFactorServiceImpl_INCLUDED
