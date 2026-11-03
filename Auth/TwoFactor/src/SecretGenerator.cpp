//
// SecretGenerator.h
//
// Library: OSP/Auth/TwoFactor
// Package: TwoFactor
// Module:  SecretGenerator
//
// Copyright (c) 2020, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties,
// copied or duplicated in any form, in whole or in part.
//


#include "Poco/OSP/Auth/TwoFactor/SecretGenerator.h"
#include "Poco/RandomStream.h"
#include "Poco/MemoryStream.h"
#include "Poco/Base32Encoder.h"
#include "Poco/Base32Decoder.h"
#include "Poco/StreamCopier.h"
#include "Poco/URI.h"
#include <sstream>


using namespace std::string_literals;


namespace Poco::OSP::Auth::TwoFactor {


SecretGenerator::SecretGenerator(std::size_t secretLengthBits):
	_secretLengthBits(secretLengthBits)
{
	poco_assert (secretLengthBits >= 80 && secretLengthBits % 8 == 0);
}


std::string SecretGenerator::generate() const
{
	const std::size_t bytes = _secretLengthBits/8;
	std::string secret;
	Poco::RandomInputStream rnd;
	for (std::size_t i = 0; i < bytes; i++)
	{
		secret += rnd.get();
	}
	return secret;
}


std::string SecretGenerator::toBase32(const std::string& secret)
{
	std::stringstream sstr;
	Poco::Base32Encoder encoder(sstr, Poco::BASE32_NO_PADDING);
	encoder << secret;
	encoder.close();
	return sstr.str();
}


std::string SecretGenerator::fromBase32(const std::string& base32)
{
	std::string secret;
	Poco::MemoryInputStream istr(base32.data(), base32.size());
	Poco::Base32Decoder decoder(istr);
	Poco::StreamCopier::copyToString(decoder, secret);
	return secret;
}


std::string SecretGenerator::toURI(const std::string& secret, const std::string& issuer, const std::string& account)
{
	std::string uri = "otpauth://totp/";
	Poco::URI::encode(issuer, "?#"s, uri);
	uri += ':';
	Poco::URI::encode(account, "?#"s, uri);
	uri += "?secret=";
	uri += toBase32(secret);
	uri += "&issuer=";
	Poco::URI::encode(issuer, "?#/:;+@&="s, uri);
	return uri;
}


} // namespace Poco::OSP::Auth::TwoFactor

