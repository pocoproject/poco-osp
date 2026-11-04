//
// TwoFactorServiceImplTest.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "TwoFactorServiceImplTest.h"
#include "MemoryAuthAdminService.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestCase.h"
#include "CppUnit/TestSuite.h"
#include "Poco/OSP/Auth/TwoFactor/TwoFactorServiceImpl.h"
#include "Poco/OSP/Auth/TwoFactor/TwoFactorService.h"
#include "Poco/OSP/Auth/TwoFactor/HOTPGenerator.h"
#include "Poco/OSP/Auth/TwoFactor/SecretGenerator.h"
#include "Poco/OSP/Auth/AuthAdminService.h"
#include "Poco/Crypto/Cipher.h"
#include "Poco/Crypto/CipherFactory.h"
#include "Poco/Crypto/CryptoException.h"
#include "Poco/AutoPtr.h"
#include "Poco/Channel.h"
#include "Poco/DateTime.h"
#include "Poco/Exception.h"
#include "Poco/Logger.h"
#include "Poco/Message.h"
#include "Poco/Mutex.h"
#include "Poco/SHA1Engine.h"
#include "Poco/Timestamp.h"
#include "Poco/Types.h"
#include <cstddef>
#include <ctime>
#include <set>
#include <string>
#include <thread>
#include <utility>
#include <vector>


using Poco::OSP::Auth::AuthAdminService;
using Poco::OSP::Auth::TwoFactor::HOTPGenerator;
using Poco::OSP::Auth::TwoFactor::SecretGenerator;
using Poco::OSP::Auth::TwoFactor::TwoFactorService;
using Poco::OSP::Auth::TwoFactor::TwoFactorServiceImpl;
using namespace std::string_literals;


namespace
{
	class CapturingChannel: public Poco::Channel
		/// Keeps the messages logged to it.
	{
	public:
		using Ptr = Poco::AutoPtr<CapturingChannel>;

		CapturingChannel() = default;
		CapturingChannel(const CapturingChannel&) = delete;
		CapturingChannel(CapturingChannel&&) = delete;
		CapturingChannel& operator = (const CapturingChannel&) = delete;
		CapturingChannel& operator = (CapturingChannel&&) = delete;

		void log(const Poco::Message& msg) override
		{
			Poco::FastMutex::ScopedLock lock(_mutex);
			_messages.push_back(msg);
		}

		[[nodiscard]] std::vector<Poco::Message> messages() const
		{
			Poco::FastMutex::ScopedLock lock(_mutex);
			return _messages;
		}

	protected:
		~CapturingChannel() override = default;

	private:
		std::vector<Poco::Message> _messages;
		mutable Poco::FastMutex _mutex;
	};


	class FixedTimeTwoFactorService: public TwoFactorServiceImpl
		/// Validates codes at a time set by the test.
	{
	public:
		using Ptr = Poco::AutoPtr<FixedTimeTwoFactorService>;

		FixedTimeTwoFactorService(const AuthAdminService::Ptr& pAuthService, const Config& config,
			Poco::Logger& logger = Poco::Logger::get("TwoFactorServiceImplTest"s)):
			TwoFactorServiceImpl(pAuthService, config, logger)
		{
		}

		FixedTimeTwoFactorService() = delete;
		FixedTimeTwoFactorService(const FixedTimeTwoFactorService&) = delete;
		FixedTimeTwoFactorService(FixedTimeTwoFactorService&&) = delete;
		FixedTimeTwoFactorService& operator = (const FixedTimeTwoFactorService&) = delete;
		FixedTimeTwoFactorService& operator = (FixedTimeTwoFactorService&&) = delete;
		~FixedTimeTwoFactorService() override = default;

		void setTimeStep(Poco::UInt64 step)
		{
			_now = Poco::Timestamp::fromEpochTime(static_cast<std::time_t>(step*TIME_STEP_SECONDS));
		}

		Poco::UInt64 timeStep() const
		{
			return static_cast<Poco::UInt64>(_now.epochTime())/TIME_STEP_SECONDS;
		}

		Poco::Timestamp currentTime() const
		{
			return _now;
		}

		static const std::string& lastStepAttribute()
		{
			return ATTR_LAST_STEP;
		}

		static const std::string& secretAttribute()
		{
			return ATTR_SECRET;
		}

		static std::vector<std::string> attributes()
		{
			return {ATTR_SECRET, ATTR_LAST_STEP, ATTR_FAILURES, ATTR_LAST_TOTP};
		}

		static const std::string& lastCodeAttribute()
		{
			return ATTR_LAST_TOTP;
		}

		void store(const std::string& username, const std::string& secret)
		{
			storeSecret(username, secret);
		}

		std::string load(const std::string& username) const
		{
			return loadSecret(username);
		}

		std::string encryptInFormatWithoutVersion(const std::string& secret) const
		{
			Poco::Crypto::Cipher::Ptr pCipher =
				Poco::Crypto::CipherFactory::defaultFactory().createCipher(generateKey());
			return pCipher->encryptString(secret, Poco::Crypto::Cipher::ENC_BASE64_NO_LF);
		}

	protected:
		Poco::Timestamp now() const override
		{
			return _now;
		}

	private:
		static constexpr Poco::UInt64 TIME_STEP_SECONDS = 30;

		Poco::Timestamp _now;
	};


	TwoFactorServiceImpl::Config testConfig()
	{
		TwoFactorServiceImpl::Config config;
		config.passphrase = "test passphrase"s;
		config.issuer = "Example"s;
		config.keySalt = "0123456789abcdef0123456789abcdef"s;
		return config;
	}


	std::string codeFor(const TwoFactorServiceImpl& service, const std::string& username, Poco::UInt64 step)
	{
		HOTPGenerator<Poco::SHA1Engine> generator(SecretGenerator::fromBase32(service.secret(username)), 6);
		return generator.generate(step);
	}


	std::string wrongCodeFor(const TwoFactorServiceImpl& service, const std::string& username, Poco::UInt64 step)
		/// Returns a code that is not valid for the given step and the steps next to it.
	{
		const std::set<std::string> valid = {codeFor(service, username, step - 1), codeFor(service, username, step),
			codeFor(service, username, step + 1)};
		constexpr int FIRST_CANDIDATE = 100000;
		int candidate = FIRST_CANDIDATE;
		while (valid.contains(std::to_string(candidate)))
		{
			++candidate;
		}
		return std::to_string(candidate);
	}
}


TwoFactorServiceImplTest::TwoFactorServiceImplTest(const std::string& name): CppUnit::TestCase(name)
{
}


TwoFactorServiceImplTest::~TwoFactorServiceImplTest() = default;


void TwoFactorServiceImplTest::testEnableAndDisable()
{
	MemoryAuthAdminService::Ptr pAuth = new MemoryAuthAdminService;
	FixedTimeTwoFactorService::Ptr pService = new FixedTimeTwoFactorService(pAuth, testConfig());

	assertFalse (pService->is2FAEnabled("alice"s));
	pService->enable2FA("alice"s);
	assertTrue (pService->is2FAEnabled("alice"s));
	assertEqual (std::size_t{16}, SecretGenerator::fromBase32(pService->secret("alice"s)).size());
	assertTrue (pService->generateAuthenticatorURI("alice"s).find("otpauth://totp/Example:alice?secret="s) == 0);

	pService->disable2FA("alice"s);
	assertFalse (pService->is2FAEnabled("alice"s));
}


void TwoFactorServiceImplTest::testCodeOfCurrentStepIsAccepted()
{
	MemoryAuthAdminService::Ptr pAuth = new MemoryAuthAdminService;
	FixedTimeTwoFactorService::Ptr pService = new FixedTimeTwoFactorService(pAuth, testConfig());
	pService->setTimeStep(55000000);
	pService->enable2FA("alice"s);

	assertTrue (pService->validateTOTP("alice"s, codeFor(*pService, "alice"s, pService->timeStep())) ==
		TwoFactorService::VALIDATION_SUCCEEDED);
}


void TwoFactorServiceImplTest::testCodesOfAdjacentStepsAreAccepted()
{
	MemoryAuthAdminService::Ptr pAuth = new MemoryAuthAdminService;
	FixedTimeTwoFactorService::Ptr pService = new FixedTimeTwoFactorService(pAuth, testConfig());
	pService->setTimeStep(55000000);
	pService->enable2FA("alice"s);

	assertTrue (pService->validateTOTP("alice"s, codeFor(*pService, "alice"s, 55000000 - 1)) ==
		TwoFactorService::VALIDATION_SUCCEEDED);
	assertTrue (pService->validateTOTP("alice"s, codeFor(*pService, "alice"s, 55000000 + 1)) ==
		TwoFactorService::VALIDATION_SUCCEEDED);
}


void TwoFactorServiceImplTest::testCodeOutsideWindowIsRejected()
{
	MemoryAuthAdminService::Ptr pAuth = new MemoryAuthAdminService;
	FixedTimeTwoFactorService::Ptr pService = new FixedTimeTwoFactorService(pAuth, testConfig());
	pService->setTimeStep(55000000);
	pService->enable2FA("alice"s);

	assertTrue (pService->validateTOTP("alice"s, codeFor(*pService, "alice"s, 55000000 - 2)) ==
		TwoFactorService::VALIDATION_FAILED_CODE_INCORRECT);
	assertTrue (pService->validateTOTP("alice"s, codeFor(*pService, "alice"s, 55000000 + 2)) ==
		TwoFactorService::VALIDATION_FAILED_CODE_INCORRECT);
}


void TwoFactorServiceImplTest::testCodeOfWrongLengthIsRejected()
{
	MemoryAuthAdminService::Ptr pAuth = new MemoryAuthAdminService;
	FixedTimeTwoFactorService::Ptr pService = new FixedTimeTwoFactorService(pAuth, testConfig());
	pService->setTimeStep(55000000);
	pService->enable2FA("alice"s);

	const std::string code = codeFor(*pService, "alice"s, pService->timeStep());
	assertTrue (pService->validateTOTP("alice"s, code.substr(1)) == TwoFactorService::VALIDATION_FAILED_CODE_INCORRECT);
	assertTrue (pService->validateTOTP("alice"s, code + "0"s) == TwoFactorService::VALIDATION_FAILED_CODE_INCORRECT);
}


void TwoFactorServiceImplTest::testValidationWithoutSecretThrows()
{
	MemoryAuthAdminService::Ptr pAuth = new MemoryAuthAdminService;
	FixedTimeTwoFactorService::Ptr pService = new FixedTimeTwoFactorService(pAuth, testConfig());

	try
	{
		[[maybe_unused]] const auto result = pService->validateTOTP("alice"s, "123456"s);
		fail("validation without a secret must throw");
	}
	catch (Poco::IllegalStateException&)
	{
	}
}


void TwoFactorServiceImplTest::testAcceptedCodeCannotBeReused()
{
	MemoryAuthAdminService::Ptr pAuth = new MemoryAuthAdminService;
	FixedTimeTwoFactorService::Ptr pService = new FixedTimeTwoFactorService(pAuth, testConfig());
	pService->setTimeStep(55000000);
	pService->enable2FA("alice"s);

	assertTrue (pService->validateTOTP("alice"s, codeFor(*pService, "alice"s, 55000000)) ==
		TwoFactorService::VALIDATION_SUCCEEDED);
	assertTrue (pService->validateTOTP("alice"s, codeFor(*pService, "alice"s, 55000000)) ==
		TwoFactorService::VALIDATION_FAILED_CODE_ALREADY_USED);
	assertTrue (pService->validateTOTP("alice"s, codeFor(*pService, "alice"s, 55000000 - 1)) ==
		TwoFactorService::VALIDATION_FAILED_CODE_ALREADY_USED);
	assertTrue (pService->validateTOTP("alice"s, codeFor(*pService, "alice"s, 55000000 + 1)) ==
		TwoFactorService::VALIDATION_SUCCEEDED);
}


void TwoFactorServiceImplTest::testConcurrentValidationAcceptsCodeOnce()
{
	MemoryAuthAdminService::Ptr pAuth = new MemoryAuthAdminService;
	FixedTimeTwoFactorService::Ptr pService = new FixedTimeTwoFactorService(pAuth, testConfig());
	pService->setTimeStep(55000000);
	pService->enable2FA("alice"s);
	const std::string code = codeFor(*pService, "alice"s, 55000000);
	pAuth->holdReadsOf(FixedTimeTwoFactorService::lastStepAttribute());

	std::vector<TwoFactorService::ValidationResult> results(2, TwoFactorService::VALIDATION_FAILED_CODE_INCORRECT);
	std::thread first([&]() { results[0] = pService->validateTOTP("alice"s, code); });
	const bool firstReadHeld = pAuth->waitForHeldRead();
	std::thread second([&]() { results[1] = pService->validateTOTP("alice"s, code); });
	first.join();
	second.join();

	assertTrue (firstReadHeld);
	const int succeeded = (results[0] == TwoFactorService::VALIDATION_SUCCEEDED) +
		(results[1] == TwoFactorService::VALIDATION_SUCCEEDED);
	assertEqual (1, succeeded);
}


void TwoFactorServiceImplTest::testFailuresLockOutUser()
{
	MemoryAuthAdminService::Ptr pAuth = new MemoryAuthAdminService;
	auto config = testConfig();
	config.maxFailures = 3;
	config.lockOutPeriod = Poco::Timespan(600, 0);
	FixedTimeTwoFactorService::Ptr pService = new FixedTimeTwoFactorService(pAuth, config);
	pService->setTimeStep(55000000);
	pService->enable2FA("alice"s);
	const std::string wrongCode = wrongCodeFor(*pService, "alice"s, 55000000);

	assertTrue (pService->validateTOTP("alice"s, wrongCode) == TwoFactorService::VALIDATION_FAILED_CODE_INCORRECT);
	assertTrue (pService->validateTOTP("alice"s, "1"s) == TwoFactorService::VALIDATION_FAILED_CODE_INCORRECT);
	assertFalse (pAuth->isUserLockedOut("alice"s));
	assertTrue (pService->validateTOTP("alice"s, wrongCode) == TwoFactorService::VALIDATION_FAILED_CODE_INCORRECT);
	assertTrue (pAuth->isUserLockedOut("alice"s));
	assertTrue (pAuth->lockOutUntil("alice"s) == Poco::DateTime(pService->currentTime()) + Poco::Timespan(600, 0));

	assertTrue (pService->validateTOTP("alice"s, codeFor(*pService, "alice"s, 55000000)) ==
		TwoFactorService::VALIDATION_FAILED_CODE_INCORRECT);
}


void TwoFactorServiceImplTest::testLockOutIsLogged()
{
	CapturingChannel::Ptr pChannel = new CapturingChannel;
	Poco::Logger& logger = Poco::Logger::get("TwoFactorServiceImplTest.lockOut"s);
	logger.setChannel(pChannel);
	logger.setLevel(Poco::Message::PRIO_NOTICE);

	MemoryAuthAdminService::Ptr pAuth = new MemoryAuthAdminService;
	auto config = testConfig();
	config.maxFailures = 2;
	FixedTimeTwoFactorService::Ptr pService = new FixedTimeTwoFactorService(pAuth, config, logger);
	pService->setTimeStep(55000000);
	pService->enable2FA("alice"s);
	const std::string wrongCode = wrongCodeFor(*pService, "alice"s, 55000000);

	assertTrue (pService->validateTOTP("alice"s, wrongCode) == TwoFactorService::VALIDATION_FAILED_CODE_INCORRECT);
	assertTrue (pChannel->messages().empty());
	assertTrue (pService->validateTOTP("alice"s, wrongCode) == TwoFactorService::VALIDATION_FAILED_CODE_INCORRECT);
	logger.setChannel(nullptr);

	const auto messages = pChannel->messages();
	assertEqual (std::size_t{1}, messages.size());
	assertTrue (messages[0].getPriority() == Poco::Message::PRIO_NOTICE);
}


void TwoFactorServiceImplTest::testSuccessResetsFailureCount()
{
	MemoryAuthAdminService::Ptr pAuth = new MemoryAuthAdminService;
	auto config = testConfig();
	config.maxFailures = 3;
	FixedTimeTwoFactorService::Ptr pService = new FixedTimeTwoFactorService(pAuth, config);
	pService->setTimeStep(55000000);
	pService->enable2FA("alice"s);
	const std::string wrongCode = wrongCodeFor(*pService, "alice"s, 55000000);

	assertTrue (pService->validateTOTP("alice"s, wrongCode) == TwoFactorService::VALIDATION_FAILED_CODE_INCORRECT);
	assertTrue (pService->validateTOTP("alice"s, wrongCode) == TwoFactorService::VALIDATION_FAILED_CODE_INCORRECT);
	assertTrue (pService->validateTOTP("alice"s, codeFor(*pService, "alice"s, 55000000)) ==
		TwoFactorService::VALIDATION_SUCCEEDED);
	assertTrue (pService->validateTOTP("alice"s, wrongCode) == TwoFactorService::VALIDATION_FAILED_CODE_INCORRECT);
	assertTrue (pService->validateTOTP("alice"s, wrongCode) == TwoFactorService::VALIDATION_FAILED_CODE_INCORRECT);
	assertFalse (pAuth->isUserLockedOut("alice"s));
}


void TwoFactorServiceImplTest::testFailureAfterLockOutLocksOutAgain()
{
	MemoryAuthAdminService::Ptr pAuth = new MemoryAuthAdminService;
	auto config = testConfig();
	constexpr long LOCK_OUT_SECONDS = 600;
	constexpr Poco::UInt64 FIRST_STEP = 55000000;
	config.maxFailures = 3;
	config.lockOutPeriod = Poco::Timespan(LOCK_OUT_SECONDS, 0);
	FixedTimeTwoFactorService::Ptr pService = new FixedTimeTwoFactorService(pAuth, config);
	Poco::UInt64 step = FIRST_STEP;
	pService->setTimeStep(step);
	pService->enable2FA("alice"s);
	std::string wrongCode = wrongCodeFor(*pService, "alice"s, step);
	for (std::size_t i = 0; i < config.maxFailures; ++i)
	{
		assertTrue (pService->validateTOTP("alice"s, wrongCode) == TwoFactorService::VALIDATION_FAILED_CODE_INCORRECT);
	}
	assertTrue (pAuth->isUserLockedOut("alice"s));

	// 21 steps of 30 seconds are longer than the lock-out period.
	constexpr Poco::UInt64 STEPS_AFTER_LOCK_OUT = 21;
	step += STEPS_AFTER_LOCK_OUT;
	pService->setTimeStep(step);
	pAuth->setTime(Poco::DateTime(pService->currentTime()));
	assertFalse (pAuth->isUserLockedOut("alice"s));
	wrongCode = wrongCodeFor(*pService, "alice"s, step);
	assertTrue (pService->validateTOTP("alice"s, wrongCode) == TwoFactorService::VALIDATION_FAILED_CODE_INCORRECT);
	assertTrue (pAuth->isUserLockedOut("alice"s));
	assertTrue (pAuth->lockOutUntil("alice"s) == Poco::DateTime(pService->currentTime()) + config.lockOutPeriod);

	step += STEPS_AFTER_LOCK_OUT;
	pService->setTimeStep(step);
	pAuth->setTime(Poco::DateTime(pService->currentTime()));
	assertTrue (pService->validateTOTP("alice"s, codeFor(*pService, "alice"s, step)) ==
		TwoFactorService::VALIDATION_SUCCEEDED);
	wrongCode = wrongCodeFor(*pService, "alice"s, step);
	assertTrue (pService->validateTOTP("alice"s, wrongCode) == TwoFactorService::VALIDATION_FAILED_CODE_INCORRECT);
	assertTrue (pService->validateTOTP("alice"s, wrongCode) == TwoFactorService::VALIDATION_FAILED_CODE_INCORRECT);
	assertFalse (pAuth->isUserLockedOut("alice"s));
}


void TwoFactorServiceImplTest::testSecretIsBoundToUser()
{
	MemoryAuthAdminService::Ptr pAuth = new MemoryAuthAdminService;
	FixedTimeTwoFactorService::Ptr pService = new FixedTimeTwoFactorService(pAuth, testConfig());
	pService->enable2FA("alice"s);
	const std::string stored = pAuth->getUserAttribute("alice"s, FixedTimeTwoFactorService::secretAttribute(), ""s);
	assertTrue (stored.compare(0, 4, "v=2;"s) == 0);

	pAuth->setAuthAttribute("mallory"s, FixedTimeTwoFactorService::secretAttribute(), stored);
	try
	{
		[[maybe_unused]] const std::string result = pService->secret("mallory"s);
		fail("a secret copied from another user must not be readable");
	}
	catch (Poco::Crypto::CryptoException&)
	{
	}
}


void TwoFactorServiceImplTest::testEqualSecretsAreStoredDifferently()
{
	MemoryAuthAdminService::Ptr pAuth = new MemoryAuthAdminService;
	FixedTimeTwoFactorService::Ptr pService = new FixedTimeTwoFactorService(pAuth, testConfig());
	const std::string secret = "12345678901234567890"s;

	pService->store("alice"s, secret);
	const std::string first = pAuth->getUserAttribute("alice"s, FixedTimeTwoFactorService::secretAttribute(), ""s);
	pService->store("alice"s, secret);
	const std::string second = pAuth->getUserAttribute("alice"s, FixedTimeTwoFactorService::secretAttribute(), ""s);

	assertTrue (first != second);
	assertEqual (secret, pService->load("alice"s));
}


void TwoFactorServiceImplTest::testOtherPassphraseCannotReadSecret()
{
	MemoryAuthAdminService::Ptr pAuth = new MemoryAuthAdminService;
	FixedTimeTwoFactorService::Ptr pService = new FixedTimeTwoFactorService(pAuth, testConfig());
	pService->enable2FA("alice"s);

	auto config = testConfig();
	config.passphrase = "other passphrase"s;
	FixedTimeTwoFactorService::Ptr pOtherService = new FixedTimeTwoFactorService(pAuth, config);
	try
	{
		[[maybe_unused]] const std::string result = pOtherService->secret("alice"s);
		fail("a secret must not be readable with another passphrase");
	}
	catch (Poco::Crypto::CryptoException&)
	{
	}
}


void TwoFactorServiceImplTest::testSecretWithoutVersionIsReadAndStoredInCurrentFormat()
{
	MemoryAuthAdminService::Ptr pAuth = new MemoryAuthAdminService;
	FixedTimeTwoFactorService::Ptr pService = new FixedTimeTwoFactorService(pAuth, testConfig());
	const std::string secret = "12345678901234567890"s;
	pAuth->setAuthAttribute("alice"s, FixedTimeTwoFactorService::secretAttribute(),
		pService->encryptInFormatWithoutVersion(secret));

	assertEqual (secret, pService->load("alice"s));
	pService->store("alice"s, pService->load("alice"s));
	assertTrue (pAuth->getUserAttribute("alice"s, FixedTimeTwoFactorService::secretAttribute(), ""s).compare(0, 4,
		"v=2;"s) == 0);
	assertEqual (secret, pService->load("alice"s));
}


void TwoFactorServiceImplTest::testSecretWithoutVersionIsStoredInCurrentFormatAfterValidation()
{
	MemoryAuthAdminService::Ptr pAuth = new MemoryAuthAdminService;
	FixedTimeTwoFactorService::Ptr pService = new FixedTimeTwoFactorService(pAuth, testConfig());
	pService->setTimeStep(55000000);
	const std::string secret = "12345678901234567890"s;
	const std::string oldFormat = pService->encryptInFormatWithoutVersion(secret);
	pAuth->setAuthAttribute("alice"s, FixedTimeTwoFactorService::secretAttribute(), oldFormat);

	const std::string wrongCode = wrongCodeFor(*pService, "alice"s, 55000000);
	assertTrue (pService->validateTOTP("alice"s, wrongCode) == TwoFactorService::VALIDATION_FAILED_CODE_INCORRECT);
	assertEqual (oldFormat, pAuth->getUserAttribute("alice"s, FixedTimeTwoFactorService::secretAttribute(), ""s));

	assertTrue (pService->validateTOTP("alice"s, codeFor(*pService, "alice"s, 55000000)) ==
		TwoFactorService::VALIDATION_SUCCEEDED);
	assertTrue (pAuth->getUserAttribute("alice"s, FixedTimeTwoFactorService::secretAttribute(), ""s).compare(0, 4,
		"v=2;"s) == 0);
	assertEqual (secret, pService->load("alice"s));

	pAuth->setAuthAttribute("mallory"s, FixedTimeTwoFactorService::secretAttribute(),
		pAuth->getUserAttribute("alice"s, FixedTimeTwoFactorService::secretAttribute(), ""s));
	try
	{
		[[maybe_unused]] const std::string result = pService->secret("mallory"s);
		fail("a re-encrypted secret copied from another user must not be readable");
	}
	catch (Poco::Crypto::CryptoException&)
	{
	}
}


void TwoFactorServiceImplTest::testValidationRemovesLastCodeOfOlderVersion()
{
	MemoryAuthAdminService::Ptr pAuth = new MemoryAuthAdminService;
	FixedTimeTwoFactorService::Ptr pService = new FixedTimeTwoFactorService(pAuth, testConfig());
	pService->setTimeStep(55000000);
	pService->enable2FA("alice"s);
	pAuth->setAuthAttribute("alice"s, FixedTimeTwoFactorService::lastCodeAttribute(), "123456"s);

	assertTrue (pService->validateTOTP("alice"s, codeFor(*pService, "alice"s, 55000000)) ==
		TwoFactorService::VALIDATION_SUCCEEDED);
	assertEqual (""s, pAuth->getUserAttribute("alice"s, FixedTimeTwoFactorService::lastCodeAttribute(), ""s));
}


void TwoFactorServiceImplTest::testDisableRemovesEveryTwoFactorAttribute()
{
	MemoryAuthAdminService::Ptr pAuth = new MemoryAuthAdminService;
	FixedTimeTwoFactorService::Ptr pService = new FixedTimeTwoFactorService(pAuth, testConfig());
	pService->enable2FA("alice"s);
	for (const auto& attribute: FixedTimeTwoFactorService::attributes())
	{
		pAuth->setAuthAttribute("alice"s, attribute, "1"s);
	}

	pService->disable2FA("alice"s);
	for (const auto& attribute: FixedTimeTwoFactorService::attributes())
	{
		assertEqual (""s, pAuth->getUserAttribute("alice"s, attribute, ""s));
	}
}


void TwoFactorServiceImplTest::testEmptyKeySaltIsRejected()
{
	MemoryAuthAdminService::Ptr pAuth = new MemoryAuthAdminService;
	auto config = testConfig();
	config.keySalt.clear();
	try
	{
		FixedTimeTwoFactorService::Ptr pService = new FixedTimeTwoFactorService(pAuth, config);
		fail("an empty key salt must be rejected");
	}
	catch (Poco::InvalidArgumentException&)
	{
	}
}


void TwoFactorServiceImplTest::testCodeDigitsAndSecretLengthAreChecked()
{
	MemoryAuthAdminService::Ptr pAuth = new MemoryAuthAdminService;
	const std::vector<std::pair<std::size_t, std::size_t>> invalid = {{5, 128}, {9, 128}, {6, 72}, {6, 84}};
	for (const auto& [codeDigits, secretLengthBits]: invalid)
	{
		auto config = testConfig();
		config.codeDigits = codeDigits;
		config.secretLengthBits = secretLengthBits;
		try
		{
			FixedTimeTwoFactorService::Ptr pService = new FixedTimeTwoFactorService(pAuth, config);
			fail("an invalid configuration must be rejected");
		}
		catch (Poco::InvalidArgumentException&)
		{
		}
	}

	auto config = testConfig();
	config.codeDigits = 8;
	config.secretLengthBits = 80;
	FixedTimeTwoFactorService::Ptr pService = new FixedTimeTwoFactorService(pAuth, config);
	pService->setTimeStep(55000000);
	pService->enable2FA("alice"s);
	assertEqual (std::size_t{10}, SecretGenerator::fromBase32(pService->secret("alice"s)).size());
	HOTPGenerator<Poco::SHA1Engine> generator(SecretGenerator::fromBase32(pService->secret("alice"s)), 8);
	assertTrue (pService->validateTOTP("alice"s, generator.generate(55000000)) ==
		TwoFactorService::VALIDATION_SUCCEEDED);
}


void TwoFactorServiceImplTest::setUp()
{
}


void TwoFactorServiceImplTest::tearDown()
{
}


CppUnit::Test* TwoFactorServiceImplTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("TwoFactorServiceImplTest");

	CppUnit_addTest(pSuite, TwoFactorServiceImplTest, testEnableAndDisable);
	CppUnit_addTest(pSuite, TwoFactorServiceImplTest, testCodeOfCurrentStepIsAccepted);
	CppUnit_addTest(pSuite, TwoFactorServiceImplTest, testCodesOfAdjacentStepsAreAccepted);
	CppUnit_addTest(pSuite, TwoFactorServiceImplTest, testCodeOutsideWindowIsRejected);
	CppUnit_addTest(pSuite, TwoFactorServiceImplTest, testCodeOfWrongLengthIsRejected);
	CppUnit_addTest(pSuite, TwoFactorServiceImplTest, testValidationWithoutSecretThrows);
	CppUnit_addTest(pSuite, TwoFactorServiceImplTest, testAcceptedCodeCannotBeReused);
	CppUnit_addTest(pSuite, TwoFactorServiceImplTest, testConcurrentValidationAcceptsCodeOnce);
	CppUnit_addTest(pSuite, TwoFactorServiceImplTest, testFailuresLockOutUser);
	CppUnit_addTest(pSuite, TwoFactorServiceImplTest, testLockOutIsLogged);
	CppUnit_addTest(pSuite, TwoFactorServiceImplTest, testSuccessResetsFailureCount);
	CppUnit_addTest(pSuite, TwoFactorServiceImplTest, testFailureAfterLockOutLocksOutAgain);
	CppUnit_addTest(pSuite, TwoFactorServiceImplTest, testSecretIsBoundToUser);
	CppUnit_addTest(pSuite, TwoFactorServiceImplTest, testEqualSecretsAreStoredDifferently);
	CppUnit_addTest(pSuite, TwoFactorServiceImplTest, testOtherPassphraseCannotReadSecret);
	CppUnit_addTest(pSuite, TwoFactorServiceImplTest, testSecretWithoutVersionIsReadAndStoredInCurrentFormat);
	CppUnit_addTest(pSuite, TwoFactorServiceImplTest, testSecretWithoutVersionIsStoredInCurrentFormatAfterValidation);
	CppUnit_addTest(pSuite, TwoFactorServiceImplTest, testValidationRemovesLastCodeOfOlderVersion);
	CppUnit_addTest(pSuite, TwoFactorServiceImplTest, testDisableRemovesEveryTwoFactorAttribute);
	CppUnit_addTest(pSuite, TwoFactorServiceImplTest, testEmptyKeySaltIsRejected);
	CppUnit_addTest(pSuite, TwoFactorServiceImplTest, testCodeDigitsAndSecretLengthAreChecked);

	return pSuite;
}
