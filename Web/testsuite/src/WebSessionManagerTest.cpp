//
// WebSessionManagerTest.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "WebSessionManagerTest.h"
#include "ServerRequestStub.h"
#include "TemporaryBundle.h"
#include "LogCapture.h"
#include "SessionStoreStub.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "CppUnit/TestCase.h"
#include "CppUnit/Test.h"
#include "Poco/OSP/Web/WebSessionManager.h"
#include "Poco/OSP/Web/WebSession.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/BundleEvent.h"
#include "Poco/Net/HTTPCookie.h"
#include "Poco/Net/NameValueCollection.h"
#include "Poco/Exception.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Message.h"
#include <atomic>
#include <thread>
#include <vector>
#include <string>
#include <memory>


using Poco::OSP::Web::WebSession;
using Poco::OSP::Web::WebSessionManager;
using Poco::OSP::BundleEvent;
using Poco::Net::HTTPCookie;


namespace
{
	constexpr int SESSION_TIMEOUT_SECONDS = 600;
	constexpr int ITERATIONS = 2000;
	const std::string CLIENT_ADDRESS("127.0.0.1:50000");
	const std::string CSRF_COOKIE("XSRF-TOKEN");

	HTTPCookie responseCookie(const Poco::Net::HTTPServerRequest& request, const std::string& name)
		/// Returns the cookie with the given name from the
		/// Set-Cookie headers of the request's response.
	{
		std::vector<HTTPCookie> cookies;
		request.response().getCookies(cookies);
		for (const auto& cookie: cookies)
		{
			if (cookie.getName() == name) return cookie;
		}
		throw Poco::NotFoundException(name);
	}

}


WebSessionManagerTest::WebSessionManagerTest(const std::string& name): CppUnit::TestCase(name)
{
}


WebSessionManagerTest::~WebSessionManagerTest() = default;


void WebSessionManagerTest::setUp()
{
	_pBundle = std::make_unique<TemporaryBundle>("");
}


void WebSessionManagerTest::tearDown()
{
	_pBundle.reset();
}


void WebSessionManagerTest::testSessionCookieAttributes()
{
	WebSessionManager::Ptr pManager(new WebSessionManager(_pBundle->context()));
	pManager->setCookieSameSite(HTTPCookie::SAME_SITE_LAX);
	ServerRequestStub request(CLIENT_ADDRESS, false);

	WebSession::Ptr pSession = pManager->create("app@.example.com/app/", request, SESSION_TIMEOUT_SECONDS, _pBundle->context());

	HTTPCookie cookie = responseCookie(request, "osp.web.session.app");
	assertEqual (pSession->id(), cookie.getValue());
	assertEqual (std::string("/app/"), cookie.getPath());
	assertEqual (std::string(".example.com"), cookie.getDomain());
	assertTrue (cookie.getHttpOnly());
	assertFalse (cookie.getSecure());
	assertTrue (cookie.getSameSite() == HTTPCookie::SAME_SITE_LAX);
}


void WebSessionManagerTest::testCookiePathFallsBackToDefaultPath()
{
	WebSessionManager::Ptr pManager(new WebSessionManager(_pBundle->context()));
	ServerRequestStub request(CLIENT_ADDRESS, false);
	pManager->create("app", request, SESSION_TIMEOUT_SECONDS, _pBundle->context());
	assertEqual (std::string("/"), responseCookie(request, "osp.web.session.app").getPath());

	pManager->setDefaultPath("/default/");
	ServerRequestStub otherRequest(CLIENT_ADDRESS, false);
	pManager->create("app", otherRequest, SESSION_TIMEOUT_SECONDS, _pBundle->context());
	assertEqual (std::string("/default/"), responseCookie(otherRequest, "osp.web.session.app").getPath());
}


void WebSessionManagerTest::testCookiesSecureOverHTTPS()
{
	WebSessionManager::Ptr pManager(new WebSessionManager(_pBundle->context()));
	pManager->setCSRFCookie(CSRF_COOKIE);
	ServerRequestStub request(CLIENT_ADDRESS, true);

	pManager->create("app", request, SESSION_TIMEOUT_SECONDS, _pBundle->context());

	assertTrue (responseCookie(request, "osp.web.session.app").getSecure());
	assertTrue (responseCookie(request, CSRF_COOKIE).getSecure());
}


void WebSessionManagerTest::testCookiesSecureWhenConfigured()
{
	WebSessionManager::Ptr pManager(new WebSessionManager(_pBundle->context()));
	pManager->setCSRFCookie(CSRF_COOKIE);
	pManager->setCookieSecure(true);
	ServerRequestStub request(CLIENT_ADDRESS, false);

	pManager->create("app", request, SESSION_TIMEOUT_SECONDS, _pBundle->context());

	assertTrue (responseCookie(request, "osp.web.session.app").getSecure());
	assertTrue (responseCookie(request, CSRF_COOKIE).getSecure());
}


void WebSessionManagerTest::testGetCookieSameSite()
{
	WebSessionManager::Ptr pManager(new WebSessionManager(_pBundle->context()));
	assertTrue (pManager->getCookieSameSite() == HTTPCookie::SAME_SITE_NOT_SPECIFIED);

	pManager->setCookieSameSite(HTTPCookie::SAME_SITE_STRICT);

	assertTrue (pManager->getCookieSameSite() == HTTPCookie::SAME_SITE_STRICT);
}


#ifdef POCO_TEST_DEPRECATED


void WebSessionManagerTest::testDeprecatedSameSiteGetter()
{
	WebSessionManager::Ptr pManager(new WebSessionManager(_pBundle->context()));
	pManager->setCookieSameSite(HTTPCookie::SAME_SITE_STRICT);

	assertTrue (pManager->setCookieSameSite() == HTTPCookie::SAME_SITE_STRICT);
}


#endif // POCO_TEST_DEPRECATED


void WebSessionManagerTest::testAddressMismatchRemovesSession()
{
	WebSessionManager::Ptr pManager(new WebSessionManager(_pBundle->context()));
	ServerRequestStub request(CLIENT_ADDRESS, false);
	WebSession::Ptr pSession = pManager->create("app", request, SESSION_TIMEOUT_SECONDS, _pBundle->context());

	Poco::Net::NameValueCollection cookies;
	cookies.add("osp.web.session.app", pSession->id());
	ServerRequestStub sameClient(CLIENT_ADDRESS, false);
	sameClient.setCookies(cookies);
	ServerRequestStub otherClient("127.0.0.2:50000", false);
	otherClient.setCookies(cookies);

	assertTrue (pManager->find("app", sameClient) == pSession);
	LogCapture capture("osp.web.session");
	assertTrue (pManager->find("app", otherClient) == nullptr);
	assertTrue (pManager->findById(pSession->id()) == nullptr);
	const std::vector<Poco::Message> messages = capture.channel().waitForMessages(1);
	assertEqual (1, static_cast<int>(messages.size()));
	assertTrue (messages[0].getText().find(pSession->id()) == std::string::npos);
}


void WebSessionManagerTest::testRenewReplacesSession()
{
	WebSessionManager::Ptr pManager(new WebSessionManager(_pBundle->context()));
	pManager->setCSRFCookie(CSRF_COOKIE);
	ServerRequestStub request(CLIENT_ADDRESS, false);
	WebSession::Ptr pSession = pManager->create("app", request, SESSION_TIMEOUT_SECONDS, _pBundle->context());
	pSession->setValue<std::string>("cart", "item");

	ServerRequestStub loginRequest(CLIENT_ADDRESS, false);
	WebSession::Ptr pRenewed = pManager->renew("app", loginRequest, pSession);
	pRenewed->setValue<std::string>("username", "alice");

	assertTrue (pRenewed->id() != pSession->id());
	assertTrue (pRenewed->csrfToken() != pSession->csrfToken());
	assertEqual (std::string("item"), pRenewed->getValue<std::string>("cart"));
	assertEqual (SESSION_TIMEOUT_SECONDS, pRenewed->timeout());
	assertTrue (pManager->findById(pSession->id()) == nullptr);
	assertTrue (pManager->findById(pRenewed->id()) == pRenewed);
	assertEqual (pRenewed->id(), responseCookie(loginRequest, "osp.web.session.app").getValue());
	assertEqual (pRenewed->csrfToken(), responseCookie(loginRequest, CSRF_COOKIE).getValue());
}


void WebSessionManagerTest::testCSRFCookieCreatesMissingToken()
{
	WebSessionManager::Ptr pManager(new WebSessionManager(_pBundle->context()));
	pManager->setCSRFCookie(CSRF_COOKIE);
	ServerRequestStub request(CLIENT_ADDRESS, false);
	WebSession::Ptr pSession = pManager->create("app", request, SESSION_TIMEOUT_SECONDS, _pBundle->context());
	// Like a session loaded from a session store that has no token.
	pSession->erase(WebSession::CSRF_TOKEN);

	Poco::Net::NameValueCollection cookies;
	cookies.add("osp.web.session.app", pSession->id());
	ServerRequestStub nextRequest(CLIENT_ADDRESS, false);
	nextRequest.setCookies(cookies);

	assertTrue (pManager->find("app", nextRequest) == pSession);
	const std::string csrfToken = responseCookie(nextRequest, CSRF_COOKIE).getValue();
	assertFalse (csrfToken.empty());
	assertEqual (csrfToken, pSession->csrfToken());
}


void WebSessionManagerTest::testRemoveForUserRemovesOnlyThatUser()
{
	WebSessionManager::Ptr pManager(new WebSessionManager(_pBundle->context()));
	ServerRequestStub request(CLIENT_ADDRESS, false);
	WebSession::Ptr pAlice = pManager->create("app", request, SESSION_TIMEOUT_SECONDS, _pBundle->context());
	pAlice->setValue<std::string>("username", "alice");
	WebSession::Ptr pBob = pManager->create("app", request, SESSION_TIMEOUT_SECONDS, _pBundle->context());
	pBob->setValue<std::string>("username", "bob");
	WebSession::Ptr pAnonymous = pManager->create("app", request, SESSION_TIMEOUT_SECONDS, _pBundle->context());
	WebSession::Ptr pOtherType = pManager->create("app", request, SESSION_TIMEOUT_SECONDS, _pBundle->context());
	pOtherType->setValue<int>("username", 1);

	pManager->removeForUser("alice");

	assertTrue (pManager->findById(pAlice->id()) == nullptr);
	assertTrue (pManager->findById(pBob->id()) != nullptr);
	assertTrue (pManager->findById(pAnonymous->id()) != nullptr);
	assertTrue (pManager->findById(pOtherType->id()) != nullptr);
}


void WebSessionManagerTest::testRemoveForUserWhileAttributesChange()
{
	// Run under ThreadSanitizer: removeForUser() reads the attributes
	// of every session while request threads modify them.
	WebSessionManager::Ptr pManager(new WebSessionManager(_pBundle->context()));
	ServerRequestStub request(CLIENT_ADDRESS, false);
	std::vector<WebSession::Ptr> sessions;
	for (int i = 0; i < 4; ++i)
	{
		WebSession::Ptr pSession = pManager->create("app", request, SESSION_TIMEOUT_SECONDS, _pBundle->context());
		pSession->setValue<std::string>("username", "bob");
		sessions.push_back(pSession);
	}
	WebSession::Ptr pAlice = pManager->create("app", request, SESSION_TIMEOUT_SECONDS, _pBundle->context());
	pAlice->setValue<std::string>("username", "alice");

	std::atomic<bool> stop(false);
	std::thread writer([&sessions, &stop]()
		{
			int i = 0;
			while (!stop)
			{
				const std::string key("key" + std::to_string(i % 100));
				for (auto& pSession: sessions)
				{
					pSession->setValue<int>(key, i);
				}
				for (auto& pSession: sessions)
				{
					pSession->erase(key);
				}
				++i;
			}
		});

	for (int i = 0; i < ITERATIONS; ++i)
	{
		pManager->removeForUser("carol");
	}
	pManager->removeForUser("alice");
	stop = true;
	writer.join();

	assertTrue (pManager->findById(pAlice->id()) == nullptr);
	for (const auto& pSession: sessions)
	{
		assertTrue (pManager->findById(pSession->id()) != nullptr);
	}
}


void WebSessionManagerTest::testBundleStoppingClearsCachedSessions()
{
	WebSessionManager::Ptr pManager(new WebSessionManager(_pBundle->context()));
	ServerRequestStub request(CLIENT_ADDRESS, false);
	WebSession::Ptr pSession = pManager->create("app", request, SESSION_TIMEOUT_SECONDS, _pBundle->context());
	pSession->setValue<std::string>("username", "alice");

	BundleEvent stopping(_pBundle->bundle(), BundleEvent::EV_BUNDLE_STOPPING);
	_pBundle->events().bundleStopping(this, stopping);

	assertTrue (pSession->attributes().empty());
}


void WebSessionManagerTest::testBundleStoppingWhileAttributesChange()
{
	// Run under ThreadSanitizer: the event handler and set() must
	// access the attributes under the same lock.
	WebSessionManager::Ptr pManager(new WebSessionManager(_pBundle->context()));
	ServerRequestStub request(CLIENT_ADDRESS, false);
	WebSession::Ptr pSession = pManager->create("app", request, SESSION_TIMEOUT_SECONDS, _pBundle->context());
	std::atomic<bool> stop(false);
	std::thread writer([&pSession, &stop]()
		{
			int i = 0;
			while (!stop)
			{
				pSession->setValue<int>("key" + std::to_string(i % 100), i);
				++i;
			}
		});

	BundleEvent stopping(_pBundle->bundle(), BundleEvent::EV_BUNDLE_STOPPING);
	for (int i = 0; i < ITERATIONS; ++i)
	{
		_pBundle->events().bundleStopping(this, stopping);
	}
	stop = true;
	writer.join();

	_pBundle->events().bundleStopping(this, stopping);
	assertTrue (pSession->attributes().empty());
}


void WebSessionManagerTest::testBundleStoppingClearsSessionsNoLongerCached()
{
	WebSessionManager::Ptr pManager(new WebSessionManager(_pBundle->context()));
	ServerRequestStub request(CLIENT_ADDRESS, false);
	WebSession::Ptr pRemoved = pManager->create("app", request, SESSION_TIMEOUT_SECONDS, _pBundle->context());
	pRemoved->setValue<std::string>("username", "alice");
	pManager->remove(pRemoved);
	WebSession::Ptr pReplaced = pManager->create("app", request, SESSION_TIMEOUT_SECONDS, _pBundle->context());
	pReplaced->setValue<std::string>("cart", "item");
	ServerRequestStub loginRequest(CLIENT_ADDRESS, false);
	WebSession::Ptr pRenewed = pManager->renew("app", loginRequest, pReplaced);

	BundleEvent stopping(_pBundle->bundle(), BundleEvent::EV_BUNDLE_STOPPING);
	_pBundle->events().bundleStopping(this, stopping);

	assertTrue (pRemoved->attributes().empty());
	assertTrue (pReplaced->attributes().empty());
	assertTrue (pRenewed->attributes().empty());
}


void WebSessionManagerTest::testSessionOutlivesManagerAndBundleLoader()
{
	// Run under Valgrind or AddressSanitizer: a session that is destroyed after the
	// session manager and the bundle loader must use neither of them.
	WebSession::Ptr pSession;
	{
		WebSessionManager::Ptr pManager(new WebSessionManager(_pBundle->context()));
		ServerRequestStub request(CLIENT_ADDRESS, false);
		pSession = pManager->create("app", request, SESSION_TIMEOUT_SECONDS, _pBundle->context());
	}
	_pBundle.reset();

	assertTrue (pSession->has(WebSession::CSRF_TOKEN));
	pSession.reset();
}


void WebSessionManagerTest::testBundleStoppingWhileSessionsComeAndGo()
{
	// Run under ThreadSanitizer: sessions are created and destroyed
	// while the attributes of all sessions are cleared.
	WebSessionManager::Ptr pManager(new WebSessionManager(_pBundle->context()));
	std::atomic<bool> stop(false);
	std::thread worker([&pManager, &stop, this]()
		{
			while (!stop)
			{
				ServerRequestStub request(CLIENT_ADDRESS, false);
				WebSession::Ptr pSession = pManager->create("app", request, SESSION_TIMEOUT_SECONDS, _pBundle->context());
				pSession->setValue<std::string>("username", "alice");
				pManager->remove(pSession);
			}
		});

	BundleEvent stopping(_pBundle->bundle(), BundleEvent::EV_BUNDLE_STOPPING);
	for (int i = 0; i < ITERATIONS; ++i)
	{
		_pBundle->events().bundleStopping(this, stopping);
	}
	stop = true;
	worker.join();

	assertEqual (0, static_cast<int>(pManager->countSessions()));
}


void WebSessionManagerTest::testBundleStoppingClearsSessionsLoadedFromStore()
{
	SessionStoreStub::Ptr pStore(new SessionStoreStub);
	WebSessionManager::Ptr pCreatingManager(new WebSessionManager(_pBundle->context()));
	pCreatingManager->setSessionStore(pStore);
	ServerRequestStub request(CLIENT_ADDRESS, false);
	WebSession::Ptr pCreated = pCreatingManager->create("app", request, SESSION_TIMEOUT_SECONDS, _pBundle->context());
	pCreated->setValue<std::string>("username", "alice");
	WebSessionManager::Ptr pLoadingManager(new WebSessionManager(_pBundle->context()));
	pLoadingManager->setSessionStore(pStore);

	WebSession::Ptr pLoaded = pLoadingManager->findById(pCreated->id());
	assertTrue (pLoaded != nullptr);
	assertTrue (pLoaded != pCreated);
	assertEqual (std::string("alice"), pLoaded->getValue<std::string>("username"));
	pLoadingManager->remove(pLoaded);
	assertTrue (pLoadingManager->findById(pCreated->id()) == nullptr);

	BundleEvent stopping(_pBundle->bundle(), BundleEvent::EV_BUNDLE_STOPPING);
	_pBundle->events().bundleStopping(this, stopping);

	assertTrue (pLoaded->attributes().empty());
}


void WebSessionManagerTest::testReplacedAttributeReleasesSessionOutsideLock()
{
	// Run under ThreadSanitizer: a bundle stop locks the registry, then a session.
	// Replacing an attribute that holds the last reference to another session
	// destroys that session, which locks the registry; this must not happen
	// while the first session is locked.
	WebSessionManager::Ptr pManager(new WebSessionManager(_pBundle->context()));
	ServerRequestStub request(CLIENT_ADDRESS, false);
	WebSession::Ptr pHolder = pManager->create("app", request, SESSION_TIMEOUT_SECONDS, _pBundle->context());
	const auto holdSession = [&]()
	{
		WebSession::Ptr pHeld = pManager->create("app", request, SESSION_TIMEOUT_SECONDS, _pBundle->context());
		pManager->remove(pHeld);
		pHolder->setValue<WebSession::Ptr>("held", pHeld);
	};

	holdSession();
	BundleEvent stopping(_pBundle->bundle(), BundleEvent::EV_BUNDLE_STOPPING);
	_pBundle->events().bundleStopping(this, stopping);
	holdSession();
	pHolder->setValue<std::string>("held", "nothing");
	holdSession();
	pHolder->erase("held");
	holdSession();
	pHolder->clear();

	assertFalse (pHolder->has("held"));
	assertEqual (1, static_cast<int>(pManager->countSessions()));
}


CppUnit::Test* WebSessionManagerTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("WebSessionManagerTest");

	CppUnit_addTest(pSuite, WebSessionManagerTest, testSessionCookieAttributes);
	CppUnit_addTest(pSuite, WebSessionManagerTest, testCookiePathFallsBackToDefaultPath);
	CppUnit_addTest(pSuite, WebSessionManagerTest, testCookiesSecureOverHTTPS);
	CppUnit_addTest(pSuite, WebSessionManagerTest, testCookiesSecureWhenConfigured);
	CppUnit_addTest(pSuite, WebSessionManagerTest, testGetCookieSameSite);
#ifdef POCO_TEST_DEPRECATED
	CppUnit_addTest(pSuite, WebSessionManagerTest, testDeprecatedSameSiteGetter);
#endif
	CppUnit_addTest(pSuite, WebSessionManagerTest, testAddressMismatchRemovesSession);
	CppUnit_addTest(pSuite, WebSessionManagerTest, testRenewReplacesSession);
	CppUnit_addTest(pSuite, WebSessionManagerTest, testCSRFCookieCreatesMissingToken);
	CppUnit_addTest(pSuite, WebSessionManagerTest, testRemoveForUserRemovesOnlyThatUser);
	CppUnit_addTest(pSuite, WebSessionManagerTest, testRemoveForUserWhileAttributesChange);
	CppUnit_addTest(pSuite, WebSessionManagerTest, testBundleStoppingClearsCachedSessions);
	CppUnit_addTest(pSuite, WebSessionManagerTest, testBundleStoppingWhileAttributesChange);
	CppUnit_addTest(pSuite, WebSessionManagerTest, testBundleStoppingClearsSessionsNoLongerCached);
	CppUnit_addTest(pSuite, WebSessionManagerTest, testBundleStoppingClearsSessionsLoadedFromStore);
	CppUnit_addTest(pSuite, WebSessionManagerTest, testSessionOutlivesManagerAndBundleLoader);
	CppUnit_addTest(pSuite, WebSessionManagerTest, testBundleStoppingWhileSessionsComeAndGo);
	CppUnit_addTest(pSuite, WebSessionManagerTest, testReplacedAttributeReleasesSessionOutsideLock);

	return pSuite;
}
