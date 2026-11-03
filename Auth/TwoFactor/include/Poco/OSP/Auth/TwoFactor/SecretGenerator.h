//
// SecretGenerator.h
//
// Library: OSP/Auth/TwoFactor
// Package: TwoFactor
// Module:  SecretGenerator
//
// Definition of the SecretGenerator class.
//
// Copyright (c) 2020, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties,
// copied or duplicated in any form, in whole or in part.
//


#ifndef OSP_Auth_TwoFactor_SecretGenerator_INCLUDED
#define OSP_Auth_TwoFactor_SecretGenerator_INCLUDED


#include "Poco/OSP/Auth/TwoFactor/TwoFactor.h"


namespace Poco::OSP::Auth::TwoFactor {


class OSPAuthTwoFactor_API SecretGenerator
	/// This class generates a secret for HOTP/TOTP as
	/// described in RFC 4226.
	///
	/// The class also provides helper functions for
	/// generating a URI suitable for encoding in a
	/// QR code.
{
public:
	SecretGenerator() = default;
		/// Creates a SecretGenerator with the default secret
		/// length (128 bits).

	explicit SecretGenerator(std::size_t secretLengthBits);
		/// Creates a SecretGenerator with the given secret
		/// length in bits. The length must be a multiple of 8.
		/// The secret length should be from 80 to 160 bits.
		/// Lengths shorter than 80 will be rejected.

	SecretGenerator(const SecretGenerator&) = delete;
	SecretGenerator& operator = (const SecretGenerator&) = delete;

	[[nodiscard]] std::string generate() const;
		/// Generates a new secret and returns it in a binary string.

	[[nodiscard]] static std::string toBase32(const std::string& secret);
		/// Encodes a secret in Base32.

	[[nodiscard]] static std::string fromBase32(const std::string& base32);
		/// Decodes the base32-encoded secret to a binary string.

	[[nodiscard]] static std::string toURI(const std::string& secret, const std::string& issuer,
		const std::string& account);
		/// Generates a URI suitable for displaying as a QR code.
		///
		/// See https://github.com/google/google-authenticator/wiki/Key-Uri-Format for
		/// more information.

private:
	std::size_t _secretLengthBits = 128U;
};


} // namespace Poco::OSP::Auth::TwoFactor


#endif // OSP_Auth_TwoFactor_SecretGenerator_INCLUDED
