//
// WebSessionSampleTest.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "WebSessionSampleTest.h"
#include "TestApplication.h"
#include "ServerRequestStub.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "CppUnit/TestCase.h"
#include "CppUnit/Test.h"
#include "Poco/OSP/Web/WebSession.h"
#include "Poco/OSP/Web/WebSessionManager.h"
#include "Poco/OSP/Bundle.h"
#include "Poco/OSP/BundleLoader.h"
#include "Poco/OSP/ServiceRef.h"
#include "Poco/Net/HTMLForm.h"
#include "Poco/Net/HTTPCookie.h"
#include "Poco/Net/HTTPRequest.h"
#include "Poco/Net/HTTPResponse.h"
#include "Poco/Net/NameValueCollection.h"
#include "Poco/NumberFormatter.h"
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <sstream>
#include <string>
#include <thread>
#include <vector>


using Poco::OSP::Web::WebSession;
using Poco::OSP::Web::WebSessionManager;
using Poco::Net::HTTPRequest;
using Poco::Net::HTTPResponse;
using namespace std::string_literals;


namespace
{
	const std::string CLIENT_ADDRESS("127.0.0.1:40000"s);
	const std::string SESSION_PATH("/session"s);
	const std::string SAMPLE_BUNDLE("com.appinf.osp.samples.websession"s);
	const std::string ENTRY_PREFIX("sample."s);
	constexpr int MAX_TRIED_ENTRIES = 1000;
	constexpr int THREADS = 4;
	constexpr int CHANGES_PER_THREAD = 25;
	constexpr int LISTINGS = 100;
	constexpr int CONCURRENT_POSTS = 8;
	constexpr int ROUNDS = 100;
	// Longer than the sample allows for the name and the value of an entry.
	constexpr std::size_t TOO_LONG_NAME_LENGTH = 1000;
	constexpr std::size_t TOO_LONG_VALUE_LENGTH = 10000;

	struct Result
	{
		HTTPResponse::HTTPStatus status = HTTPResponse::HTTP_INTERNAL_SERVER_ERROR;
		std::string body;
	};

	class SessionClient
		/// Sends requests to the WebSession sample page, with the
		/// cookies that the previous responses have set.
		/// A copy uses the same session.
	{
	public:
		SessionClient() = delete;

		explicit SessionClient(TestApplication& app):
			_pApp(&app)
		{
		}

		SessionClient(const SessionClient&) = default;
		SessionClient(SessionClient&&) = delete;
		SessionClient& operator = (const SessionClient&) = delete;
		SessionClient& operator = (SessionClient&&) = delete;
		~SessionClient() = default;

		Result get(const std::string& query = std::string())
		{
			ServerRequestStub request(CLIENT_ADDRESS, false);
			request.setURI(query.empty() ? SESSION_PATH : SESSION_PATH + "?"s + query);
			return send(request);
		}

		Result post(const std::string& name, const std::string& value, const std::string& token)
		{
			Poco::Net::HTMLForm form;
			form.set("name"s, name);
			form.set("value"s, value);
			if (!token.empty()) form.set("token"s, token);
			std::ostringstream body;
			form.write(body);
			ServerRequestStub request(CLIENT_ADDRESS, false, body.str());
			request.setMethod(HTTPRequest::HTTP_POST);
			request.setURI(SESSION_PATH);
			request.setContentType(Poco::Net::HTMLForm::ENCODING_URL);
			return send(request);
		}

		[[nodiscard]] std::string token()
			/// Returns the CSRF token of the session, from the form of the page.
		{
			const std::string body = get().body;
			const std::string field("name=\"token\" value=\""s);
			const std::size_t pos = body.find(field);
			if (pos == std::string::npos) return std::string();
			const std::size_t begin = pos + field.size();
			return body.substr(begin, body.find('"', begin) - begin);
		}

		[[nodiscard]] WebSession::Ptr session()
			/// Returns the session of the client.
		{
			ServerRequestStub request(CLIENT_ADDRESS, false);
			request.setURI(SESSION_PATH);
			request.setCookies(_cookies);
			Poco::OSP::ServiceRef::Ptr pManagerRef = _pApp->serviceRegistry().findByName(WebSessionManager::SERVICE_NAME);
			return pManagerRef->castedInstance<WebSessionManager>()->find("sample"s, request);
		}

	private:
		Result send(ServerRequestStub& request)
		{
			request.setCookies(_cookies);
			_pApp->dispatch(request);
			std::vector<Poco::Net::HTTPCookie> cookies;
			request.response().getCookies(cookies);
			for (const auto& cookie: cookies)
			{
				_cookies.set(cookie.getName(), cookie.getValue());
			}
			return Result{request.response().getStatus(), request.response().body()};
		}

		TestApplication* _pApp;
		Poco::Net::NameValueCollection _cookies;
	};

	[[nodiscard]] TestApplication::Ptr startSample()
	{
		TestApplication::Ptr pApp = new TestApplication({"poco.net"s, "osp.web"s, SAMPLE_BUNDLE});
		pApp->start();
		return pApp;
	}

	[[nodiscard]] int storeEntriesUntilRejected(SessionClient& client, const std::string& token)
		/// Stores new entries until the sample rejects one, and returns
		/// the number of entries stored.
	{
		int stored = 0;
		while (stored < MAX_TRIED_ENTRIES && client.post("entry"s + Poco::NumberFormatter::format(stored), "value"s, token).status == HTTPResponse::HTTP_OK)
		{
			++stored;
		}
		return stored;
	}

	[[nodiscard]] bool listsEntry(const std::string& body, const std::string& name, const std::string& value)
	{
		return body.find("<LI>"s + name + ": "s + value + "</LI>"s) != std::string::npos;
	}
}


WebSessionSampleTest::WebSessionSampleTest(const std::string& name): CppUnit::TestCase(name)
{
}


WebSessionSampleTest::~WebSessionSampleTest() = default;


void WebSessionSampleTest::testGetDoesNotChangeSession()
{
	TestApplication::Ptr pApp = startSample();
	SessionClient client(*pApp);
	client.get();

	assertTrue (client.get("name=color&value=blue"s).status == HTTPResponse::HTTP_OK);
	assertFalse (listsEntry(client.get().body, "color"s, "blue"s));
}


void WebSessionSampleTest::testPostWithoutTokenIsRejected()
{
	TestApplication::Ptr pApp = startSample();
	SessionClient client(*pApp);
	client.get();

	assertTrue (client.post("color"s, "blue"s, ""s).status == HTTPResponse::HTTP_FORBIDDEN);
	assertTrue (client.post("color"s, "blue"s, "wrong"s).status == HTTPResponse::HTTP_FORBIDDEN);
	assertFalse (listsEntry(client.get().body, "color"s, "blue"s));
}


void WebSessionSampleTest::testPostWithTokenSetsAndRemovesEntry()
{
	TestApplication::Ptr pApp = startSample();
	SessionClient client(*pApp);
	const std::string token = client.token();
	assertFalse (token.empty());

	assertTrue (client.post("color"s, "<blue>"s, token).status == HTTPResponse::HTTP_OK);
	assertTrue (listsEntry(client.get().body, "color"s, "&lt;blue&gt;"s));

	assertTrue (client.post("color"s, ""s, token).status == HTTPResponse::HTTP_OK);
	assertTrue (client.get().body.find("color"s) == std::string::npos);
}


void WebSessionSampleTest::testServerAttributesCannotBeSet()
{
	TestApplication::Ptr pApp = startSample();
	SessionClient client(*pApp);
	const std::string token = client.token();

	client.post(WebSession::CSRF_TOKEN, "known"s, token);
	client.post("username"s, "admin"s, token);

	WebSession::Ptr pSession = client.session();
	assertNotNull (pSession.get());
	assertEqual (token, pSession->csrfToken());
	assertFalse (pSession->has("username"s));
}


void WebSessionSampleTest::testNumberOfEntriesIsLimited()
{
	TestApplication::Ptr pApp = startSample();
	SessionClient client(*pApp);
	const std::string token = client.token();

	const int stored = storeEntriesUntilRejected(client, token);
	assertTrue (stored > 0);
	assertTrue (stored < MAX_TRIED_ENTRIES);
	assertTrue (client.post("entry"s + Poco::NumberFormatter::format(stored), "value"s, token).status == HTTPResponse::HTTP_BAD_REQUEST);
	assertTrue (client.post(std::string(TOO_LONG_NAME_LENGTH, 'n'), "value"s, token).status == HTTPResponse::HTTP_BAD_REQUEST);
	assertTrue (client.post("entry0"s, std::string(TOO_LONG_VALUE_LENGTH, 'v'), token).status == HTTPResponse::HTTP_BAD_REQUEST);
}


void WebSessionSampleTest::testConcurrentRequestsInOneSession()
{
	TestApplication::Ptr pApp = startSample();
	SessionClient client(*pApp);
	const std::string token = client.token();

	// Every response lists the entries while other requests of the
	// same session change them, through the sample page and through
	// the WebSession interface.
	WebSession::Ptr pSession = client.session();
	std::atomic<int> failures(0);
	std::vector<std::thread> threads;
	threads.reserve(THREADS);
	for (int t = 0; t < THREADS; ++t)
	{
		threads.emplace_back([&failures, client, pSession, token, t]() mutable
		{
			const std::string name("entry"s + Poco::NumberFormatter::format(t));
			for (int i = 0; i < CHANGES_PER_THREAD; ++i)
			{
				pSession->set(ENTRY_PREFIX + name, "x"s);
				pSession->erase(ENTRY_PREFIX + name);
				if (client.post(name, "x"s, token).status != HTTPResponse::HTTP_OK) ++failures;
				if (client.post(name, ""s, token).status != HTTPResponse::HTTP_OK) ++failures;
			}
		});
	}
	for (int i = 0; i < LISTINGS; ++i)
	{
		if (client.get().status != HTTPResponse::HTTP_OK) ++failures;
	}
	for (auto& thread: threads) thread.join();
	assertEqual (0, failures.load());
}


void WebSessionSampleTest::testConcurrentPostsKeepNumberOfEntriesLimited()
{
	TestApplication::Ptr pApp = startSample();
	SessionClient client(*pApp);
	const std::string token = client.token();
	const int stored = storeEntriesUntilRejected(client, token);
	assertTrue (stored < MAX_TRIED_ENTRIES);
	WebSession::Ptr pSession = client.session();

	// Every round frees one place, and several requests try to take it at once.
	for (int round = 0; round < ROUNDS; ++round)
	{
		pSession->erase(ENTRY_PREFIX + "entry0"s);
		std::atomic<bool> started(false);
		std::vector<std::thread> threads;
		threads.reserve(CONCURRENT_POSTS);
		for (int t = 0; t < CONCURRENT_POSTS; ++t)
		{
			threads.emplace_back([&started, client, token, t]() mutable
			{
				while (!started.load()) std::this_thread::yield();
				client.post("new"s + Poco::NumberFormatter::format(t), "x"s, token);
			});
		}
		started = true;
		for (auto& thread: threads) thread.join();

		const WebSession::Attributes attributes = pSession->attributes();
		const auto entries = std::count_if(attributes.begin(), attributes.end(), [](const auto& attribute)
		{
			return attribute.first.compare(0, ENTRY_PREFIX.size(), ENTRY_PREFIX) == 0;
		});
		assertTrue (entries <= stored);

		for (int t = 0; t < CONCURRENT_POSTS; ++t)
		{
			pSession->erase(ENTRY_PREFIX + "new"s + Poco::NumberFormatter::format(t));
		}
		pSession->set(ENTRY_PREFIX + "entry0"s, "value"s);
	}
}


void WebSessionSampleTest::testStoppingSampleReleasesEntries()
{
	TestApplication::Ptr pApp = startSample();
	SessionClient client(*pApp);
	const std::string token = client.token();
	assertTrue (client.post("color"s, "blue"s, token).status == HTTPResponse::HTTP_OK);
	WebSession::Ptr pSession = client.session();
	assertNotNull (pSession.get());

	// The values of the entries were created by the library of the sample,
	// which must still be loaded when the session releases them.
	Poco::OSP::Bundle::Ptr pSample = pApp->bundleLoader().findBundle(SAMPLE_BUNDLE);
	pSample->stop();

	assertFalse (pSession->has(ENTRY_PREFIX + "color"s));
}


CppUnit::Test* WebSessionSampleTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("WebSessionSampleTest");

	CppUnit_addTest(pSuite, WebSessionSampleTest, testGetDoesNotChangeSession);
	CppUnit_addTest(pSuite, WebSessionSampleTest, testPostWithoutTokenIsRejected);
	CppUnit_addTest(pSuite, WebSessionSampleTest, testPostWithTokenSetsAndRemovesEntry);
	CppUnit_addTest(pSuite, WebSessionSampleTest, testServerAttributesCannotBeSet);
	CppUnit_addTest(pSuite, WebSessionSampleTest, testNumberOfEntriesIsLimited);
	CppUnit_addTest(pSuite, WebSessionSampleTest, testConcurrentRequestsInOneSession);
	CppUnit_addTest(pSuite, WebSessionSampleTest, testConcurrentPostsKeepNumberOfEntriesLimited);
	CppUnit_addTest(pSuite, WebSessionSampleTest, testStoppingSampleReleasesEntries);

	return pSuite;
}
