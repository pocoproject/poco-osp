//
// WebSessionTest.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "WebSessionTest.h"
#include "TemporaryBundle.h"
#include "SessionStoreStub.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "CppUnit/TestCase.h"
#include "CppUnit/Test.h"
#include "Poco/OSP/Web/WebSession.h"
#include "Poco/Timestamp.h"
#include "Poco/Any.h"
#include "Poco/Types.h"
#include <atomic>
#include <map>
#include <thread>
#include <string>
#include <memory>


using Poco::OSP::Web::WebSession;


namespace
{
	constexpr int SESSION_TIMEOUT_SECONDS = 600;
	constexpr int ITERATIONS = 2000;
}


WebSessionTest::WebSessionTest(const std::string& name): CppUnit::TestCase(name)
{
}


WebSessionTest::~WebSessionTest() = default;


void WebSessionTest::setUp()
{
	_pBundle = std::make_unique<TemporaryBundle>("");
}


void WebSessionTest::tearDown()
{
	_pBundle.reset();
}


void WebSessionTest::testAttributesReturnsCopy()
{
	WebSession session("id", "token", 1, SESSION_TIMEOUT_SECONDS, Poco::Net::IPAddress(), nullptr, _pBundle->context());
	session.setValue<std::string>("username", "alice");

	WebSession::Attributes attrs = session.attributes();
	session.erase("username");

	assertEqual (2, static_cast<int>(attrs.size()));
	assertEqual (std::string("alice"), Poco::AnyCast<std::string>(attrs["username"]));
	assertEqual (std::string("token"), Poco::AnyCast<std::string>(attrs[WebSession::CSRF_TOKEN]));
	assertFalse (session.has("username"));
}


void WebSessionTest::testTimeoutUpdatesExpiration()
{
	WebSession session("id", "token", 1, 1, Poco::Net::IPAddress(), nullptr, _pBundle->context());
	Poco::Timestamp before;

	session.timeout(SESSION_TIMEOUT_SECONDS);

	assertEqual (SESSION_TIMEOUT_SECONDS, session.timeout());
	assertTrue (session.getExpiration() >= before + Poco::Timespan(SESSION_TIMEOUT_SECONDS, 0).totalMicroseconds());
}


void WebSessionTest::testClearKeepsCSRFToken()
{
	WebSession session("id", "token", 1, SESSION_TIMEOUT_SECONDS, Poco::Net::IPAddress(), nullptr, _pBundle->context());
	session.setValue<std::string>("username", "alice");

	session.clear();

	assertFalse (session.has("username"));
	assertEqual (std::string("token"), session.csrfToken());
}


void WebSessionTest::testClearKeepsCSRFTokenInStore()
{
	SessionStoreStub::Ptr pStore(new SessionStoreStub);
	WebSession session("id", "token", 1, SESSION_TIMEOUT_SECONDS, Poco::Net::IPAddress(), pStore, _pBundle->context());
	session.setValue<std::string>("username", "alice");

	session.clear();

	WebSession::Attributes stored = pStore->attributes("id");
	assertEqual (1, static_cast<int>(stored.size()));
	assertEqual (std::string("token"), Poco::AnyCast<std::string>(stored[WebSession::CSRF_TOKEN]));
	assertEqual (std::string("token"), session.csrfToken());
}


void WebSessionTest::testSessionOutlivesBundleLoader()
{
	// Run under Valgrind or AddressSanitizer: a session that is destroyed after the bundle
	// loader must not use the bundle events of the loader.
	WebSession::Ptr pSession(new WebSession("id", "token", 1, SESSION_TIMEOUT_SECONDS, Poco::Net::IPAddress(), nullptr, _pBundle->context()));
	_pBundle.reset();

	assertEqual (std::string("token"), pSession->csrfToken());
	pSession.reset();
}


void WebSessionTest::testVersionReadWhileAttributesChange()
{
	// Run under ThreadSanitizer: version() is read by the session
	// manager while set() updates the version under the session lock.
	SessionStoreStub::Ptr pStore(new SessionStoreStub);
	WebSession session("id", "token", 1, SESSION_TIMEOUT_SECONDS, Poco::Net::IPAddress(), pStore, _pBundle->context());
	std::atomic<bool> done(false);
	std::thread writer([&session, &done]()
		{
			for (int i = 0; i < ITERATIONS; ++i)
			{
				session.setValue<int>("counter", i);
			}
			done = true;
		});

	Poco::Int64 lastVersion = 0;
	bool increasing = true;
	while (!done)
	{
		Poco::Int64 version = session.version();
		increasing = increasing && version >= lastVersion;
		lastVersion = version;
	}
	writer.join();

	assertTrue (increasing);
	assertEqual (static_cast<Poco::Int64>(ITERATIONS + 1), session.version());
}


CppUnit::Test* WebSessionTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("WebSessionTest");

	CppUnit_addTest(pSuite, WebSessionTest, testAttributesReturnsCopy);
	CppUnit_addTest(pSuite, WebSessionTest, testTimeoutUpdatesExpiration);
	CppUnit_addTest(pSuite, WebSessionTest, testClearKeepsCSRFToken);
	CppUnit_addTest(pSuite, WebSessionTest, testClearKeepsCSRFTokenInStore);
	CppUnit_addTest(pSuite, WebSessionTest, testSessionOutlivesBundleLoader);
	CppUnit_addTest(pSuite, WebSessionTest, testVersionReadWhileAttributesChange);

	return pSuite;
}
