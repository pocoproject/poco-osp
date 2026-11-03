//
// HOTPGenerator.h
//
// Library: OSP/Auth/TwoFactor
// Package: TwoFactor
// Module:  HOTPGenerator
//
// Definition of the HOTPGenerator class.
//
// Copyright (c) 2020, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties,
// copied or duplicated in any form, in whole or in part.
//


#ifndef OSP_Auth_TwoFactor_HOTPGenerator_INCLUDED
#define OSP_Auth_TwoFactor_HOTPGenerator_INCLUDED


#include "Poco/OSP/Auth/TwoFactor/TwoFactor.h"
#include "Poco/HMACEngine.h"
#include "Poco/SHA1Engine.h"
#include "Poco/ByteOrder.h"
#include "Poco/NumberFormatter.h"


namespace Poco::OSP::Auth::TwoFactor {


template <typename Engine>
class HOTPGenerator
	/// This class generates HOTPs (HMAC-based One Time Passwords)
	/// according to RFC 4226.
	///
	/// The hash function for the HMAC is configurable via a template
	/// parameter and is typically Poco::SHA1Engine.
{
public:
	explicit HOTPGenerator(const std::string& secret, std::size_t codeDigits = 6):
		_hmacEngine(secret),
		_codeDigits(codeDigits)
		/// Creates the HOTPGenerator with the given secret (binary string)
		/// and number of code digits, which should be from 6-8.
	{
		poco_assert (codeDigits > 0 && codeDigits <= 8);
	}

	HOTPGenerator() = delete;
	HOTPGenerator(const HOTPGenerator&) = delete;
	HOTPGenerator& operator = (const HOTPGenerator&) = delete;

	[[nodiscard]] std::string generate(Poco::UInt64 movingFactor)
		/// Returns a HOTP string based on the secret and the given movingFactor.
	{
		Poco::UInt64 movingFactorBE = Poco::ByteOrder::toBigEndian(movingFactor);
		_hmacEngine.reset();
		_hmacEngine.update(&movingFactorBE, sizeof(movingFactorBE));
		const auto hash = _hmacEngine.digest();
		const Poco::UInt32 offset = hash[hash.size() - 1] & 0xf;
		const Poco::UInt32 binary =
			((hash[offset]     & 0x7f) << 24) |
			((hash[offset + 1] & 0xff) << 16) |
			((hash[offset + 2] & 0xff) << 8)  |
			 (hash[offset + 3] & 0xff);

		Poco::UInt32 hotp = binary % digitsPower();

		return Poco::NumberFormatter::format0(hotp, static_cast<int>(_codeDigits));
	}

protected:
	[[nodiscard]] Poco::UInt32 digitsPower() const
	{
		static const Poco::UInt32 DIGITS_POWER[] = {1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000};

		return DIGITS_POWER[_codeDigits];
	}

private:
	static constexpr std::size_t MIN_DIGEST_SIZE = 20;
	static_assert(Engine::DIGEST_SIZE >= MIN_DIGEST_SIZE,
		"HOTPGenerator requires a digest of at least 20 bytes (RFC 4226, section 5.3)");

	Poco::HMACEngine<Engine> _hmacEngine;
	std::size_t _codeDigits;
};


} // namespace Poco::OSP::Auth::TwoFactor


#endif // OSP_Auth_TwoFactor_HOTPGenerator_INCLUDED
