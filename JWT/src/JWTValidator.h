//
// JWTValidator.h
//
// Copyright (c) 2019, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#ifndef OSP_JWT_JWTValidator_INCLUDED
#define OSP_JWT_JWTValidator_INCLUDED


#include "Poco/OSP/Web/TokenValidator.h"
#include "Poco/JWT/Signer.h"
#include "Poco/Logger.h"


namespace Poco::OSP::JWT {


class JWTValidator: public Poco::OSP::Web::TokenValidator
	/// A TokenValidator that validates JSON Web Tokens (JWTs).
{
public:
	using Ptr = Poco::AutoPtr<JWTValidator>;

	struct Config
	{
		std::set<std::string> algorithms;
		std::string hmacKey;
		std::string rsaPublicKey;
		std::string ecPublicKey;
		Poco::Timespan leeway;
		std::string issuer;   /// if not empty, the issuer that a token must name
		std::string audience; /// if not empty, the audience that a token must name
	};

	explicit JWTValidator(const Config& config);
		/// Creates the JWTValidator with the given configuration.
		///
		/// Logs a warning for every algorithm for which the configuration has no key.

	~JWTValidator() override;
		/// Destroys the JWTValidator.

	// TokenValidator
	[[nodiscard]] bool validateToken(const std::string& token, std::string& username, std::string& scope) override;
		/// Accepts a token that is signed with one of the configured algorithms and
		/// keys, has not expired, is valid already, names a subject, and names the
		/// configured issuer and audience, if they are configured.
		///
		/// Rejected tokens are logged at debug level at most, and never with the token.

	static const std::string SERVICE_NAME;

protected:
	void configureSigner(const Config& config);

private:
	Poco::JWT::Signer _signer;
	Poco::Timespan _leeway;
	std::string _issuer;
	std::string _audience;
	Poco::Logger& _logger;
};


} // namespace Poco::OSP::JWT


#endif // OSP_JWT_JWTValidator_INCLUDED
