//
// JWTValidator.cpp
//
// Copyright (c) 2019, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#include "JWTValidator.h"
#include "Poco/JWT/Signer.h"
#include "Poco/JWT/Token.h"
#include "Poco/String.h"
#include <algorithm>
#include <set>
#include <sstream>
#include <string>
#include <vector>


using namespace std::string_literals;


namespace Poco::OSP::JWT {


namespace
{
	[[nodiscard]] bool isSupported(const std::string& algorithm)
		/// Returns true if POCO JWT verifies tokens signed with the given algorithm.
	{
		static const std::set<std::string> SUPPORTED = Poco::JWT::Signer().addAllAlgorithms().getAlgorithms();
		return SUPPORTED.contains(algorithm);
	}

	[[nodiscard]] bool hasKey(const JWTValidator::Config& config, const std::string& algorithm)
		/// Returns true if the configuration has a key for the given supported algorithm.
	{
		if (Poco::startsWith(algorithm, "HS"s)) return !config.hmacKey.empty();
		if (Poco::startsWith(algorithm, "RS"s)) return !config.rsaPublicKey.empty();
		if (Poco::startsWith(algorithm, "ES"s)) return !config.ecPublicKey.empty();
		return false;
	}
}


const std::string JWTValidator::SERVICE_NAME("osp.jwt.validator");


JWTValidator::JWTValidator(const Config& config):
	_leeway(config.leeway),
	_issuer(config.issuer),
	_audience(config.audience),
	_logger(Poco::Logger::get("JWTValidator"s))
{
	configureSigner(config);
	for (const auto& algorithm: config.algorithms)
	{
		if (!isSupported(algorithm))
		{
			_logger.warning("The JWT algorithm %s is not supported; tokens signed with it are rejected."s, algorithm);
		}
		else if (!hasKey(config, algorithm))
		{
			_logger.warning("No key is configured for the JWT algorithm %s; tokens signed with it are rejected."s, algorithm);
		}
	}
}


JWTValidator::~JWTValidator() = default;


bool JWTValidator::validateToken(const std::string& jwt, std::string& username, std::string& scope)
{
	try
	{
		Poco::JWT::Token token;
		if (_signer.tryVerify(jwt, token))
		{
			const std::vector<std::string> audience = token.getAudience();
			const bool audienceMatches = _audience.empty() || std::find(audience.begin(), audience.end(), _audience) != audience.end();
			if ((_issuer.empty() || token.getIssuer() == _issuer) && audienceMatches && !token.getSubject().empty())
			{
				Poco::Timestamp bitEarlier;
				bitEarlier -= _leeway;
				if (token.getExpiration() >= bitEarlier)
				{
					Poco::Timestamp bitLater;
					bitLater += _leeway;
					if (token.getNotBefore() <= bitLater)
					{
						username = token.getSubject();
						scope = token.payload().optValue("scope"s, ""s);
						return true;
					}
				}
			}
		}
	}
	catch (Poco::Exception& exc)
	{
		// The message of the exception can contain the token.
		_logger.debug("JWT rejected: %s"s, std::string(exc.name()));
	}
	return false;
}


void JWTValidator::configureSigner(const Config& config)
{
	_signer.setAlgorithms(config.algorithms);
	if (!config.hmacKey.empty())
	{
		_signer.setHMACKey(config.hmacKey);
	}
	if (!config.rsaPublicKey.empty())
	{
		std::istringstream publicKeyStream(config.rsaPublicKey);
		Poco::SharedPtr<Poco::Crypto::RSAKey> pKey = new Poco::Crypto::RSAKey(&publicKeyStream);
		_signer.setRSAKey(pKey);
	}
	if (!config.ecPublicKey.empty())
	{
		std::istringstream publicKeyStream(config.ecPublicKey);
		Poco::SharedPtr<Poco::Crypto::ECKey> pKey = new Poco::Crypto::ECKey(&publicKeyStream);
		_signer.setECKey(pKey);
	}
}


} // namespace Poco::OSP::JWT
