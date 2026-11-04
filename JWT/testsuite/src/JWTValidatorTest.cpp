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

	std::string publicKey(const Poco::Crypto::ECKey& key)
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

	return pSuite;
}
