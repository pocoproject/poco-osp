//
// LoginThrottleTest.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "LoginThrottleTest.h"
#include "LogCapture.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "CppUnit/TestCase.h"
#include "CppUnit/Test.h"
#include "Poco/OSP/Auth/LoginThrottle.h"
#include "Poco/Event.h"
#include "Poco/Exception.h"
#include "Poco/Logger.h"
#include "Poco/Message.h"
#include <atomic>
#include <chrono>
#include <cstddef>
#include <string>
#include <thread>
#include <utility>
#include <vector>


using Poco::OSP::Auth::LoginThrottle;
using namespace std::chrono_literals;


namespace
{
	using TimePoint = std::chrono::steady_clock::time_point;

	[[nodiscard]] LoginThrottle::Settings limitedTo(int maxAttempts)
	{
		LoginThrottle::Settings settings;
		settings.maxAttempts = maxAttempts;
		return settings;
	}

	[[nodiscard]] int admittedChecks(LoginThrottle& throttle, const std::string& key, int checks)
		/// Begins the given number of checks of the key, which all fail, and returns how many were admitted.
	{
		int admitted = 0;
		for (int i = 0; i < checks; ++i)
		{
			if (throttle.begin(key).admitted()) ++admitted;
		}
		return admitted;
	}

	[[nodiscard]] std::vector<Poco::Message> loggedMessages(LogCapture& capture)
	{
		return capture.channel().waitForMessages(0);
	}
}


LoginThrottleTest::LoginThrottleTest(const std::string& name): CppUnit::TestCase(name)
{
}


LoginThrottleTest::~LoginThrottleTest() = default;


void LoginThrottleTest::testDefaultSettings()
{
	const LoginThrottle::Settings settings;
	assertEqual (10, settings.maxAttempts);
	assertTrue (settings.cooldown == 60s);

	LoginThrottle throttle(settings);
	assertTrue (throttle.enabled());
	assertEqual (10, admittedChecks(throttle, "192.0.2.1", 11));
}


void LoginThrottleTest::testChecksAreRefusedAfterMaxAttempts()
{
	TimePoint now;
	LoginThrottle throttle(limitedTo(3), [&now]() { return now; });

	assertEqual (3, admittedChecks(throttle, "192.0.2.1", 3));

	const LoginThrottle::Attempt refusal = throttle.begin("192.0.2.1");
	assertFalse (refusal.admitted());
	assertEqual ("192.0.2.1", refusal.key());
	assertTrue (refusal.retryAfter() == 60s);

	assertEqual (3, admittedChecks(throttle, "192.0.2.2", 4));
}


void LoginThrottleTest::testRefusalIsNotCounted()
{
	TimePoint now;
	LoginThrottle throttle(limitedTo(2), [&now]() { return now; });

	LoginThrottle::Attempt success = throttle.begin("192.0.2.1");
	const LoginThrottle::Attempt failure = throttle.begin("192.0.2.1");
	assertFalse (throttle.begin("192.0.2.1").admitted());
	success.succeeded();

	assertTrue (throttle.begin("192.0.2.1").admitted());
}


void LoginThrottleTest::testFirstRefusalIsReportedOncePerWindow()
{
	TimePoint now;
	LoginThrottle throttle(limitedTo(1), [&now]() { return now; });

	const LoginThrottle::Attempt admitted = throttle.begin("192.0.2.1");
	assertFalse (admitted.firstRefusal());
	assertTrue (throttle.begin("192.0.2.1").firstRefusal());
	assertFalse (throttle.begin("192.0.2.1").firstRefusal());

	now += 60s;
	assertTrue (throttle.begin("192.0.2.1").admitted());
	assertTrue (throttle.begin("192.0.2.1").firstRefusal());
}


void LoginThrottleTest::testRefusalIsLogged()
{
	LogCapture capture("LoginThrottleTest");
	Poco::Logger& logger = Poco::Logger::get("LoginThrottleTest");
	logger.setLevel(Poco::Message::PRIO_DEBUG);
	TimePoint now;
	LoginThrottle throttle(limitedTo(1), [&now]() { return now; });
	assertTrue (throttle.begin("192.0.2.1").admitted());
	now += 18s;

	// A user name with a quote and a letter outside ASCII, in UTF-8.
	for (int i = 0; i < 2; ++i)
	{
		LoginThrottle::logRefusal(logger, throttle.begin("192.0.2.1"), "q\"\xc5\xbe");
	}

	const auto messages = loggedMessages(capture);
	assertEqual (3, static_cast<int>(messages.size()));
	assertTrue (messages[0].getPriority() == Poco::Message::PRIO_WARNING);
	assertEqual (std::string("Too many failed password checks from 192.0.2.1: checks are refused for 42 s."),
		messages[0].getText());
	for (std::size_t i = 1; i < messages.size(); ++i)
	{
		assertTrue (messages[i].getPriority() == Poco::Message::PRIO_DEBUG);
		assertEqual (std::string("Password check of user q\\\"\\u017E from 192.0.2.1 refused for 42 s."),
			messages[i].getText());
	}
}


void LoginThrottleTest::testRetryAfterIsRoundedUpToWholeSeconds()
{
	TimePoint now;
	LoginThrottle throttle(limitedTo(1), [&now]() { return now; });
	assertTrue (throttle.begin("192.0.2.1").admitted());
	assertTrue (throttle.begin("192.0.2.1").retryAfter() == 60s);

	now += 250ms;
	assertTrue (throttle.begin("192.0.2.1").retryAfter() == 60s);

	now += 59s;
	assertTrue (throttle.begin("192.0.2.1").retryAfter() == 1s);

	now += 749ms;
	assertTrue (throttle.begin("192.0.2.1").retryAfter() == 1s);

	now += 1ms;
	const LoginThrottle::Attempt admitted = throttle.begin("192.0.2.1");
	assertTrue (admitted.admitted());
	assertTrue (admitted.retryAfter() == 0s);
}


void LoginThrottleTest::testWindowStartsAtFirstCountedCheck()
{
	TimePoint now;
	LoginThrottle throttle(limitedTo(2), [&now]() { return now; });
	assertTrue (throttle.begin("192.0.2.1").admitted());

	now += 50s;
	assertTrue (throttle.begin("192.0.2.1").admitted());

	now += 9s;
	const LoginThrottle::Attempt refusal = throttle.begin("192.0.2.1");
	assertFalse (refusal.admitted());
	assertTrue (refusal.retryAfter() == 1s);

	now += 1s;
	assertEqual (2, admittedChecks(throttle, "192.0.2.1", 3));
}


void LoginThrottleTest::testSuccessRefundsCheck()
{
	TimePoint now;
	LoginThrottle throttle(limitedTo(2), [&now]() { return now; });

	LoginThrottle::Attempt success = throttle.begin("192.0.2.1");
	assertTrue (success.admitted());
	success.succeeded();

	assertEqual (2, admittedChecks(throttle, "192.0.2.1", 3));
}


void LoginThrottleTest::testSuccessRefundsCheckOnce()
{
	TimePoint now;
	LoginThrottle throttle(limitedTo(2), [&now]() { return now; });

	LoginThrottle::Attempt success = throttle.begin("192.0.2.1");
	const LoginThrottle::Attempt failure = throttle.begin("192.0.2.1");
	success.succeeded();
	success.succeeded();

	assertEqual (1, admittedChecks(throttle, "192.0.2.1", 2));
}


void LoginThrottleTest::testSuccessDoesNotClearFailures()
{
	TimePoint now;
	LoginThrottle throttle(limitedTo(3), [&now]() { return now; });

	assertEqual (2, admittedChecks(throttle, "192.0.2.1", 2));
	LoginThrottle::Attempt success = throttle.begin("192.0.2.1");
	success.succeeded();

	assertEqual (1, admittedChecks(throttle, "192.0.2.1", 2));
}


void LoginThrottleTest::testRefundFromEarlierWindowIsIgnored()
{
	TimePoint now;
	LoginThrottle throttle(limitedTo(1), [&now]() { return now; });

	LoginThrottle::Attempt earlier = throttle.begin("192.0.2.1");
	assertTrue (earlier.admitted());

	now += 60s;
	const LoginThrottle::Attempt later = throttle.begin("192.0.2.1");
	assertTrue (later.admitted());
	earlier.succeeded();

	assertFalse (throttle.begin("192.0.2.1").admitted());
}


void LoginThrottleTest::testRefusalRefundsNothing()
{
	TimePoint now;
	LoginThrottle throttle(limitedTo(1), [&now]() { return now; });

	const LoginThrottle::Attempt failure = throttle.begin("192.0.2.1");
	LoginThrottle::Attempt refusal = throttle.begin("192.0.2.1");
	assertFalse (refusal.admitted());
	refusal.succeeded();

	assertFalse (throttle.begin("192.0.2.1").admitted());
}


void LoginThrottleTest::testMovedFromAttemptRefundsNothing()
{
	// The key is empty, so that a moved-from Attempt that kept the throttle would refund the counted window
	// under its moved-from key, which is usually empty.
	TimePoint now;
	LoginThrottle throttle(limitedTo(2), [&now]() { return now; });

	LoginThrottle::Attempt first = throttle.begin("");
	LoginThrottle::Attempt constructed(std::move(first));
	const LoginThrottle::Attempt failure = throttle.begin("");
	constructed.succeeded();
	first.succeeded(); // NOLINT(bugprone-use-after-move): the moved-from state is documented
	assertTrue (constructed.admitted());
	assertEqual ("", constructed.key());
	assertEqual (1, admittedChecks(throttle, "", 2));

	now += 60s;
	LoginThrottle::Attempt second = throttle.begin("");
	LoginThrottle::Attempt assigned = throttle.begin("");
	assigned = std::move(second);
	assigned.succeeded();
	second.succeeded(); // NOLINT(bugprone-use-after-move): the moved-from state is documented
	assertEqual (1, admittedChecks(throttle, "", 2));
}


void LoginThrottleTest::testExemptAttemptCountsNothing()
{
	TimePoint now;
	LoginThrottle throttle(limitedTo(1), [&now]() { return now; });
	assertTrue (throttle.begin("192.0.2.1").admitted());

	LoginThrottle::Attempt exempt = LoginThrottle::exempt("192.0.2.1");
	assertTrue (exempt.admitted());
	assertFalse (exempt.firstRefusal());
	assertTrue (exempt.retryAfter() == 0s);
	assertEqual ("192.0.2.1", exempt.key());
	exempt.succeeded();

	assertFalse (throttle.begin("192.0.2.1").admitted());
}


void LoginThrottleTest::testInvalidSettingsAreRejected()
{
	const std::vector<LoginThrottle::Settings> settings = {
		{.maxAttempts = -1, .cooldown = 60s},
		{.maxAttempts = 3, .cooldown = 0s},
		{.maxAttempts = 3, .cooldown = -5s},
		{.maxAttempts = 3, .cooldown = LoginThrottle::MAX_COOLDOWN + 1s},
		{.maxAttempts = 3, .cooldown = std::chrono::seconds::max()}
	};
	for (const auto& setting: settings)
	{
		try
		{
			const LoginThrottle throttle(setting);
			failmsg("the settings maxAttempts " + std::to_string(setting.maxAttempts) + ", cooldown " +
				std::to_string(setting.cooldown.count()) + " s must be rejected");
		}
		catch (const Poco::InvalidArgumentException&)
		{
		}
	}

	assertTrue (LoginThrottle({.maxAttempts = 3, .cooldown = 1s}).enabled());
	assertTrue (LoginThrottle({.maxAttempts = 3, .cooldown = LoginThrottle::MAX_COOLDOWN}).enabled());
	assertFalse (LoginThrottle({.maxAttempts = 0, .cooldown = 0s}).enabled());
}


void LoginThrottleTest::testZeroMaxAttemptsTurnsLimitOff()
{
	TimePoint now;
	LoginThrottle throttle(limitedTo(0), [&now]() { return now; });
	assertFalse (throttle.enabled());
	assertEqual (0, throttle.settings().maxAttempts);

	assertEqual (100, admittedChecks(throttle, "192.0.2.1", 100));
}


void LoginThrottleTest::testParallelChecksCannotExceedLimit()
{
	constexpr int MAX_ATTEMPTS = 5;
	constexpr int THREADS = 8;
	constexpr int CHECKS_PER_THREAD = 20;

	LoginThrottle throttle(limitedTo(MAX_ATTEMPTS));
	Poco::Event start(Poco::Event::EVENT_MANUALRESET);
	std::atomic<int> admitted = 0;
	std::vector<std::thread> threads;
	threads.reserve(THREADS);
	for (int i = 0; i < THREADS; ++i)
	{
		threads.emplace_back([&]()
		{
			// The checks do not end before all threads are done, so that every check runs in parallel.
			std::vector<LoginThrottle::Attempt> checks;
			start.wait();
			for (int j = 0; j < CHECKS_PER_THREAD; ++j)
			{
				checks.push_back(throttle.begin("192.0.2.1"));
				if (checks.back().admitted()) ++admitted;
			}
		});
	}
	start.set();
	for (auto& thread: threads)
	{
		thread.join();
	}

	assertEqual (MAX_ATTEMPTS, admitted.load());
}


void LoginThrottleTest::testParallelRefundsAreCounted()
{
	constexpr int THREADS = 8;
	constexpr int CHECKS_PER_THREAD = 200;

	LoginThrottle throttle(limitedTo(THREADS));
	Poco::Event start(Poco::Event::EVENT_MANUALRESET);
	std::vector<std::thread> threads;
	threads.reserve(THREADS);
	for (int i = 0; i < THREADS; ++i)
	{
		threads.emplace_back([&]()
		{
			start.wait();
			for (int j = 0; j < CHECKS_PER_THREAD; ++j)
			{
				throttle.begin("192.0.2.1").succeeded();
			}
		});
	}
	start.set();
	for (auto& thread: threads)
	{
		thread.join();
	}

	assertEqual (THREADS, admittedChecks(throttle, "192.0.2.1", THREADS + 1));
}


void LoginThrottleTest::testFullKeyTableRemovesWindowThatEndsFirst()
{
	TimePoint now;
	LoginThrottle throttle(limitedTo(1), [&now]() { return now; });
	assertTrue (throttle.begin("first").admitted());

	now += 1s;
	for (std::size_t i = 1; i < LoginThrottle::MAX_KEYS; ++i)
	{
		assertTrue (throttle.begin("key" + std::to_string(i)).admitted());
	}
	assertFalse (throttle.begin("first").admitted());

	now += 1s;
	assertTrue (throttle.begin("new").admitted());
	assertFalse (throttle.begin("key1").admitted());
	assertFalse (throttle.begin("new").admitted());
	assertTrue (throttle.begin("first").admitted());
}


void LoginThrottleTest::testRenewedWindowIsRemovedLast()
{
	TimePoint now;
	LoginThrottle throttle(limitedTo(1), [&now]() { return now; });
	assertTrue (throttle.begin("first").admitted());

	now += 1s;
	for (std::size_t i = 1; i < LoginThrottle::MAX_KEYS; ++i)
	{
		assertTrue (throttle.begin("key" + std::to_string(i)).admitted());
	}

	now += 59s;
	assertTrue (throttle.begin("first").admitted());
	assertTrue (throttle.begin("new").admitted());
	assertFalse (throttle.begin("first").admitted());
}


void LoginThrottleTest::testDroppedWindowIsLoggedOncePerMinute()
{
	LogCapture capture("osp.core.LoginThrottle");
	TimePoint now;
	LoginThrottle throttle({.maxAttempts = 1, .cooldown = 300s}, [&now]() { return now; });
	for (std::size_t i = 0; i < LoginThrottle::MAX_KEYS; ++i)
	{
		assertTrue (throttle.begin("key" + std::to_string(i)).admitted());
	}
	assertTrue (loggedMessages(capture).empty());

	assertTrue (throttle.begin("new1").admitted());
	auto messages = loggedMessages(capture);
	assertEqual (1, static_cast<int>(messages.size()));
	assertTrue (messages[0].getPriority() == Poco::Message::PRIO_WARNING);
	assertEqual (std::string("The limit of password checks holds the maximum of 16384 client addresses; the window "
		"of key0, which had not ended, was removed, so the checks counted for it are lost. Further removals are not "
		"logged for 60 s."),
		messages[0].getText());

	assertTrue (throttle.begin("new2").admitted());
	now += 59s;
	assertTrue (throttle.begin("new3").admitted());
	assertEqual (1, static_cast<int>(loggedMessages(capture).size()));

	now += 1s;
	assertTrue (throttle.begin("new4").admitted());
	messages = loggedMessages(capture);
	assertEqual (2, static_cast<int>(messages.size()));
	assertTrue (messages[1].getText().find("the window of key3, which had not ended, was removed")
		!= std::string::npos);

	now += 300s;
	assertTrue (throttle.begin("new5").admitted());
	assertEqual (2, static_cast<int>(loggedMessages(capture).size()));
}


void LoginThrottleTest::testDroppedWindowWithoutCountedChecksIsNotLogged()
{
	LogCapture capture("osp.core.LoginThrottle");
	TimePoint now;
	LoginThrottle throttle(limitedTo(1), [&now]() { return now; });
	LoginThrottle::Attempt success = throttle.begin("succeeded");
	success.succeeded();
	for (std::size_t i = 1; i < LoginThrottle::MAX_KEYS; ++i)
	{
		assertTrue (throttle.begin("key" + std::to_string(i)).admitted());
	}

	assertTrue (throttle.begin("new1").admitted());
	assertTrue (loggedMessages(capture).empty());

	assertTrue (throttle.begin("new2").admitted());
	const auto messages = loggedMessages(capture);
	assertEqual (1, static_cast<int>(messages.size()));
	assertTrue (messages[0].getText().find("the window of key1, which had not ended, was removed")
		!= std::string::npos);
}


void LoginThrottleTest::setUp()
{
}


void LoginThrottleTest::tearDown()
{
}


CppUnit::Test* LoginThrottleTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("LoginThrottleTest");

	CppUnit_addTest(pSuite, LoginThrottleTest, testDefaultSettings);
	CppUnit_addTest(pSuite, LoginThrottleTest, testChecksAreRefusedAfterMaxAttempts);
	CppUnit_addTest(pSuite, LoginThrottleTest, testRefusalIsNotCounted);
	CppUnit_addTest(pSuite, LoginThrottleTest, testFirstRefusalIsReportedOncePerWindow);
	CppUnit_addTest(pSuite, LoginThrottleTest, testRefusalIsLogged);
	CppUnit_addTest(pSuite, LoginThrottleTest, testRetryAfterIsRoundedUpToWholeSeconds);
	CppUnit_addTest(pSuite, LoginThrottleTest, testWindowStartsAtFirstCountedCheck);
	CppUnit_addTest(pSuite, LoginThrottleTest, testSuccessRefundsCheck);
	CppUnit_addTest(pSuite, LoginThrottleTest, testSuccessRefundsCheckOnce);
	CppUnit_addTest(pSuite, LoginThrottleTest, testSuccessDoesNotClearFailures);
	CppUnit_addTest(pSuite, LoginThrottleTest, testRefundFromEarlierWindowIsIgnored);
	CppUnit_addTest(pSuite, LoginThrottleTest, testRefusalRefundsNothing);
	CppUnit_addTest(pSuite, LoginThrottleTest, testMovedFromAttemptRefundsNothing);
	CppUnit_addTest(pSuite, LoginThrottleTest, testExemptAttemptCountsNothing);
	CppUnit_addTest(pSuite, LoginThrottleTest, testInvalidSettingsAreRejected);
	CppUnit_addTest(pSuite, LoginThrottleTest, testZeroMaxAttemptsTurnsLimitOff);
	CppUnit_addTest(pSuite, LoginThrottleTest, testParallelChecksCannotExceedLimit);
	CppUnit_addTest(pSuite, LoginThrottleTest, testParallelRefundsAreCounted);
	CppUnit_addTest(pSuite, LoginThrottleTest, testFullKeyTableRemovesWindowThatEndsFirst);
	CppUnit_addTest(pSuite, LoginThrottleTest, testRenewedWindowIsRemovedLast);
	CppUnit_addTest(pSuite, LoginThrottleTest, testDroppedWindowIsLoggedOncePerMinute);
	CppUnit_addTest(pSuite, LoginThrottleTest, testDroppedWindowWithoutCountedChecksIsNotLogged);

	return pSuite;
}
