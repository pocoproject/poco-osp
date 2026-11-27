//
// JWTValidatorTest.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "JWTValidatorTest.h"
#include "JWTValidator.h"
#include "LogCapture.h"
#include "Poco/JWT/Signer.h"
#include "Poco/JWT/Token.h"
#include "Poco/Crypto/ECKey.h"
#include "Poco/Crypto/KeyPair.h"
#include "Poco/Crypto/RSAKey.h"
#include "Poco/Logger.h"
#include "Poco/Message.h"
#include "Poco/SharedPtr.h"
#include "Poco/Timespan.h"
#include "Poco/Timestamp.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "CppUnit/TestCase.h"
#include "CppUnit/Test.h"
#include <sstream>
#include <string>
#include <vector>


using Poco::OSP::JWT::JWTValidator;
using namespace std::string_literals;


namespace
{
	const std::string HMAC_KEY("0123456789abcdef0123456789abcdef");
	const std::string LOGGER_NAME("JWTValidator");
	const std::string END_OF_TEST("end of test");

	std::vector<Poco::Message> messagesUntilEnd(CaptureChannel& channel)
		/// Logs END_OF_TEST and returns the messages that were logged before it.
	{
		Poco::Logger::get(LOGGER_NAME).information(END_OF_TEST);
		std::vector<Poco::Message> messages;
		for (const auto& msg: channel.waitForMessages(1))
		{
			if (msg.getText() == END_OF_TEST) break;
			messages.push_back(msg);
		}
		return messages;
	}

	Poco::JWT::Token token(const std::string& subject)
		/// Returns a token for the given subject that expires in one hour.
	{
		Poco::JWT::Token token;
		token.setSubject(subject);
		Poco::Timestamp expiration;
		expiration += Poco::Timespan(3600, 0);
		token.setExpiration(expiration);
		return token;
	}

	std::string publicKey(const Poco::Crypto::KeyPair& key)
		/// Returns the public key in PEM format.
	{
		std::ostringstream ostr;
		key.save(&ostr);
		return ostr.str();
	}

	JWTValidator::Config hmacConfig()
	{
		JWTValidator::Config config;
		config.algorithms = {"HS256"s};
		config.hmacKey = HMAC_KEY;
		config.leeway = Poco::Timespan(5, 0);
		return config;
	}
}


JWTValidatorTest::JWTValidatorTest(const std::string& name): CppUnit::TestCase(name)
{
}


JWTValidatorTest::~JWTValidatorTest() = default;


void JWTValidatorTest::testHMACToken()
{
	JWTValidator::Ptr pValidator = new JWTValidator(hmacConfig());
	Poco::JWT::Token hmacToken = token("alice"s);
	hmacToken.payload().set("scope"s, "read"s);
	const std::string jwt = Poco::JWT::Signer(HMAC_KEY).sign(hmacToken, "HS256"s);

	std::string username;
	std::string scope;
	assertTrue (pValidator->validateToken(jwt, username, scope));
	assertEqual ("alice"s, username);
	assertEqual ("read"s, scope);
}


void JWTValidatorTest::testECTokenWithConfiguredKey()
{
	Poco::SharedPtr<Poco::Crypto::ECKey> pKey = new Poco::Crypto::ECKey("prime256v1"s);
	JWTValidator::Config config;
	config.algorithms = {"ES256"s};
	config.ecPublicKey = publicKey(*pKey);
	JWTValidator::Ptr pValidator = new JWTValidator(config);
	Poco::JWT::Token ecToken = token("alice"s);
	const std::string jwt = Poco::JWT::Signer(pKey).sign(ecToken, "ES256"s);

	std::string username;
	std::string scope;
	assertTrue (pValidator->validateToken(jwt, username, scope));
	assertEqual ("alice"s, username);
}


void JWTValidatorTest::testECTokenWithOtherKeyIsRejected()
{
	Poco::SharedPtr<Poco::Crypto::ECKey> pKey = new Poco::Crypto::ECKey("prime256v1"s);
	Poco::SharedPtr<Poco::Crypto::ECKey> pOtherKey = new Poco::Crypto::ECKey("prime256v1"s);
	JWTValidator::Config config;
	config.algorithms = {"ES256"s};
	config.ecPublicKey = publicKey(*pKey);
	JWTValidator::Ptr pValidator = new JWTValidator(config);
	Poco::JWT::Token ecToken = token("alice"s);
	const std::string jwt = Poco::JWT::Signer(pOtherKey).sign(ecToken, "ES256"s);

	std::string username;
	std::string scope;
	assertTrue (!pValidator->validateToken(jwt, username, scope));
}


void JWTValidatorTest::testAudienceIsChecked()
{
	JWTValidator::Config config = hmacConfig();
	config.audience = "osp"s;
	JWTValidator::Ptr pValidator = new JWTValidator(config);
	Poco::JWT::Signer signer(HMAC_KEY);
	Poco::JWT::Token ospToken = token("alice"s);
	ospToken.setAudience(std::vector<std::string>{"other"s, "osp"s});
	Poco::JWT::Token otherToken = token("alice"s);
	otherToken.setAudience("other"s);
	Poco::JWT::Token noAudienceToken = token("alice"s);

	std::string username;
	std::string scope;
	assertTrue (pValidator->validateToken(signer.sign(ospToken, "HS256"s), username, scope));
	assertTrue (!pValidator->validateToken(signer.sign(otherToken, "HS256"s), username, scope));
	assertTrue (!pValidator->validateToken(signer.sign(noAudienceToken, "HS256"s), username, scope));
}


void JWTValidatorTest::testTokenWithoutSubjectIsRejected()
{
	JWTValidator::Ptr pValidator = new JWTValidator(hmacConfig());
	Poco::JWT::Token noSubjectToken = token(""s);

	std::string username;
	std::string scope;
	assertTrue (!pValidator->validateToken(Poco::JWT::Signer(HMAC_KEY).sign(noSubjectToken, "HS256"s), username, scope));
}


void JWTValidatorTest::testMalformedTokenIsNotLogged()
{
	JWTValidator::Ptr pValidator = new JWTValidator(hmacConfig());
	LogCapture capture(LOGGER_NAME);
	const std::string malformed("eyJhbGciOiJIUzI1NiJ9.not-a-token"s);

	std::string username;
	std::string scope;
	assertTrue (!pValidator->validateToken(malformed, username, scope));
	assertTrue (messagesUntilEnd(capture.channel()).empty());
}


void JWTValidatorTest::testMissingKeyIsLoggedAtStart()
{
	LogCapture capture(LOGGER_NAME);
	JWTValidator::Config config;
	config.algorithms = {"HS256"s, "ES256"s};
	config.hmacKey = HMAC_KEY;
	JWTValidator::Ptr pValidator = new JWTValidator(config);

	const std::vector<Poco::Message> messages = messagesUntilEnd(capture.channel());
	assertEqual (1, static_cast<int>(messages.size()));
	assertTrue (messages[0].getPriority() == Poco::Message::PRIO_WARNING);
	assertTrue (messages[0].getText().find("ES256"s) != std::string::npos);
}


void JWTValidatorTest::testUnsupportedAlgorithmIsLoggedAtStart()
{
	LogCapture capture(LOGGER_NAME);
	JWTValidator::Config config;
	config.algorithms = {"HS256"s, "HS265"s, "PS256"s, "none"s};
	config.hmacKey = HMAC_KEY;
	JWTValidator::Ptr pValidator = new JWTValidator(config);

	const std::vector<Poco::Message> messages = messagesUntilEnd(capture.channel());
	assertEqual (3, static_cast<int>(messages.size()));
	for (const auto& algorithm: {"HS265"s, "PS256"s, "none"s})
	{
		int named = 0;
		for (const auto& msg: messages)
		{
			if (msg.getText().find("algorithm " + algorithm + " is not supported") != std::string::npos) ++named;
		}
		assertEqual (1, named);
	}
}


void JWTValidatorTest::testRSATokenWithConfiguredKey()
{
	Poco::SharedPtr<Poco::Crypto::RSAKey> pKey =
		new Poco::Crypto::RSAKey(Poco::Crypto::RSAKey::KL_2048, Poco::Crypto::RSAKey::EXP_LARGE);
	JWTValidator::Config config;
	config.algorithms = {"RS256"s};
	config.rsaPublicKey = publicKey(*pKey);
	JWTValidator::Ptr pValidator = new JWTValidator(config);
	Poco::JWT::Token rsaToken = token("alice"s);
	const std::string jwt = Poco::JWT::Signer(pKey).sign(rsaToken, "RS256"s);

	std::string username;
	std::string scope;
	assertTrue (pValidator->validateToken(jwt, username, scope));
	assertEqual ("alice"s, username);
}


void JWTValidatorTest::testTokenExpiredWithinLeewayIsAccepted()
{
	JWTValidator::Ptr pValidator = new JWTValidator(hmacConfig());
	Poco::JWT::Token expiredToken = token("alice"s);
	Poco::Timestamp expiration;
	expiration -= Poco::Timespan(2, 0);
	expiredToken.setExpiration(expiration);

	std::string username;
	std::string scope;
	assertTrue (pValidator->validateToken(Poco::JWT::Signer(HMAC_KEY).sign(expiredToken, "HS256"s), username, scope));
}


void JWTValidatorTest::testTokenExpiredBeyondLeewayIsRejected()
{
	JWTValidator::Ptr pValidator = new JWTValidator(hmacConfig());
	Poco::JWT::Token expiredToken = token("alice"s);
	Poco::Timestamp expiration;
	expiration -= Poco::Timespan(60, 0);
	expiredToken.setExpiration(expiration);

	std::string username;
	std::string scope;
	assertTrue (!pValidator->validateToken(Poco::JWT::Signer(HMAC_KEY).sign(expiredToken, "HS256"s), username, scope));
}


void JWTValidatorTest::testTokenWithoutExpirationIsRejected()
{
	JWTValidator::Ptr pValidator = new JWTValidator(hmacConfig());
	Poco::JWT::Token noExpirationToken;
	noExpirationToken.setSubject("alice"s);

	std::string username;
	std::string scope;
	assertTrue (!pValidator->validateToken(Poco::JWT::Signer(HMAC_KEY).sign(noExpirationToken, "HS256"s),
		username, scope));
}


void JWTValidatorTest::testTokenNotYetValidIsRejected()
{
	JWTValidator::Ptr pValidator = new JWTValidator(hmacConfig());
	Poco::JWT::Token futureToken = token("alice"s);
	Poco::Timestamp notBefore;
	notBefore += Poco::Timespan(60, 0);
	futureToken.setNotBefore(notBefore);

	std::string username;
	std::string scope;
	assertTrue (!pValidator->validateToken(Poco::JWT::Signer(HMAC_KEY).sign(futureToken, "HS256"s), username, scope));
}


void JWTValidatorTest::testIssuerIsChecked()
{
	JWTValidator::Config config = hmacConfig();
	config.issuer = "osp"s;
	JWTValidator::Ptr pValidator = new JWTValidator(config);
	Poco::JWT::Signer signer(HMAC_KEY);
	Poco::JWT::Token ospToken = token("alice"s);
	ospToken.setIssuer("osp"s);
	Poco::JWT::Token otherToken = token("alice"s);
	otherToken.setIssuer("other"s);
	Poco::JWT::Token noIssuerToken = token("alice"s);

	std::string username;
	std::string scope;
	assertTrue (pValidator->validateToken(signer.sign(ospToken, "HS256"s), username, scope));
	assertTrue (!pValidator->validateToken(signer.sign(otherToken, "HS256"s), username, scope));
	assertTrue (!pValidator->validateToken(signer.sign(noIssuerToken, "HS256"s), username, scope));
}


void JWTValidatorTest::testAlgorithmThatIsNotConfiguredIsRejected()
{
	JWTValidator::Ptr pValidator = new JWTValidator(hmacConfig());
	Poco::JWT::Signer signer(HMAC_KEY);
	signer.addAlgorithm("HS384"s);
	Poco::JWT::Token hs384Token = token("alice"s);

	std::string username;
	std::string scope;
	assertTrue (!pValidator->validateToken(signer.sign(hs384Token, "HS384"s), username, scope));
}


void JWTValidatorTest::testUnsignedTokenIsRejected()
{
	JWTValidator::Config config = hmacConfig();
	config.algorithms.insert("none"s);
	JWTValidator::Ptr pValidator = new JWTValidator(config);
	Poco::JWT::Token unsignedToken = token("alice"s);
	unsignedToken.setAlgorithm("none"s);

	std::string username;
	std::string scope;
	assertTrue (!pValidator->validateToken(unsignedToken.toString(), username, scope));
}


void JWTValidatorTest::setUp()
{
}


void JWTValidatorTest::tearDown()
{
}


CppUnit::Test* JWTValidatorTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("JWTValidatorTest");

	CppUnit_addTest(pSuite, JWTValidatorTest, testHMACToken);
	CppUnit_addTest(pSuite, JWTValidatorTest, testECTokenWithConfiguredKey);
	CppUnit_addTest(pSuite, JWTValidatorTest, testECTokenWithOtherKeyIsRejected);
	CppUnit_addTest(pSuite, JWTValidatorTest, testAudienceIsChecked);
	CppUnit_addTest(pSuite, JWTValidatorTest, testTokenWithoutSubjectIsRejected);
	CppUnit_addTest(pSuite, JWTValidatorTest, testMalformedTokenIsNotLogged);
	CppUnit_addTest(pSuite, JWTValidatorTest, testMissingKeyIsLoggedAtStart);
	CppUnit_addTest(pSuite, JWTValidatorTest, testUnsupportedAlgorithmIsLoggedAtStart);
	CppUnit_addTest(pSuite, JWTValidatorTest, testRSATokenWithConfiguredKey);
	CppUnit_addTest(pSuite, JWTValidatorTest, testTokenExpiredWithinLeewayIsAccepted);
	CppUnit_addTest(pSuite, JWTValidatorTest, testTokenExpiredBeyondLeewayIsRejected);
	CppUnit_addTest(pSuite, JWTValidatorTest, testTokenWithoutExpirationIsRejected);
	CppUnit_addTest(pSuite, JWTValidatorTest, testTokenNotYetValidIsRejected);
	CppUnit_addTest(pSuite, JWTValidatorTest, testIssuerIsChecked);
	CppUnit_addTest(pSuite, JWTValidatorTest, testAlgorithmThatIsNotConfiguredIsRejected);
	CppUnit_addTest(pSuite, JWTValidatorTest, testUnsignedTokenIsRejected);

	return pSuite;
}
