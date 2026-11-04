//
// WebServerDispatcherTest.cpp
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "WebServerDispatcherTest.h"
#include "TemporaryBundle.h"
#include "ServerRequestStub.h"
#include "LogCapture.h"
#include "Poco/OSP/Web/WebServerDispatcher.h"
#include "Poco/OSP/Web/MediaTypeMapper.h"
#include "Poco/OSP/Web/WebServerRequestHandlerFactory.h"
#include "Poco/OSP/Web/WebFilter.h"
#include "Poco/OSP/Web/WebFilterFactory.h"
#include "Poco/OSP/Web/TokenValidator.h"
#include "Poco/OSP/Web/WebSessionManager.h"
#include "Poco/OSP/Web/WebSession.h"
#include "Poco/OSP/Web/WebRequestHandlerFactory.h"
#include "Poco/OSP/Web/LibraryRequestHandlerFactory.h"
#include "Poco/OSP/Web/LibraryWebFilterFactory.h"
#include "Poco/OSP/Bundle.h"
#include "Poco/OSP/BundleEvent.h"
#include "Poco/OSP/ServiceRegistry.h"
#include "Poco/OSP/Auth/AuthService.h"
#include "Poco/OSP/Auth/RequestIdentity.h"
#include "Poco/OSP/Properties.h"
#include "Poco/OSP/ServiceRef.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/Net/HTTPServer.h"
#include "Poco/Net/HTTPServerParams.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Net/HTTPServerResponse.h"
#include "Poco/Net/HTTPRequestHandler.h"
#include "Poco/Net/HTTPRequestHandlerFactory.h"
#include "Poco/Net/HTTPBasicCredentials.h"
#include "Poco/Net/HTTPCookie.h"
#include "Poco/Net/NameValueCollection.h"
#include "Poco/Net/HTTPClientSession.h"
#include "Poco/Net/HTTPMessage.h"
#include "Poco/Net/Socket.h"
#include "Poco/Net/HTTPRequest.h"
#include "Poco/Net/HTTPResponse.h"
#include "Poco/Net/ServerSocket.h"
#include "Poco/Net/SocketAddress.h"
#include "Poco/JSON/Parser.h"
#include "Poco/JSON/Object.h"
#include "Poco/StreamCopier.h"
#include "Poco/AutoPtr.h"
#include "Poco/SharedPtr.h"
#include "Poco/Exception.h"
#include "Poco/Ascii.h"
#include "Poco/Types.h"
#include "Poco/Timespan.h"
#include "Poco/Timestamp.h"
#include "Poco/DateTimeFormat.h"
#include "Poco/DateTimeFormatter.h"
#include "Poco/InflatingStream.h"
#include "Poco/URI.h"
#include "Poco/Event.h"
#include "Poco/RegularExpression.h"
#include "Poco/Channel.h"
#include "Poco/Logger.h"
#include "Poco/Message.h"
#include "Poco/Environment.h"
#include "Poco/FileStream.h"
#include "Poco/Path.h"
#include "Poco/File.h"
#if defined(OSPWEB_TEST_LIBRARY)
#include "LibraryReference.h"
#endif
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestSuite.h"
#include "CppUnit/TestCase.h"
#include "CppUnit/Test.h"
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <ios>
#include <istream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>


using namespace Poco::OSP::Web;
using namespace Poco::OSP;
using Poco::Path;
using Poco::Net::HTTPRequest;
using Poco::Net::HTTPResponse;
using namespace std::string_literals;


namespace
{
	const std::string BUNDLE_PROPERTIES(
		"message.secure    = A secure connection is required to access $1 on this server.\n"
		"message.method    = Method $1 is not allowed for $2.\n"
		"message.invalid   = Invalid characters found in request URI.\n"
		"message.redirect  = Redirecting to $1.\n"
		"message.notfound  = The requested URL $1 was not found on this server.\n"
		"message.notauth   = You are not authorized to access $1 on this server.\n"
		"message.forbidden = Listing the contents of directory $1 is not allowed.\n"
		"message.internal  = The request failed with an exception (reference: $1).\n"
		"message.unavailable = Authentication for $1 is not available.\n"
		"web.static.resource = persist\n");

	const Poco::Timespan CLIENT_TIMEOUT(10, 0);
	constexpr int SESSION_TIMEOUT_SECONDS = 600;
	const std::string AUTH_SERVICE_NAME("osp.web.test.auth");
	const std::string TOKEN_VALIDATOR_NAME("osp.web.test.tokenvalidator");
	const std::string AUTHORIZED_USER_HEADER("X-OSP-Authorized-User");
	const std::string AUTHORIZED_SCOPE_HEADER("X-OSP-Authorized-Scope");
	class TestDispatcher: public WebServerDispatcher
		/// Gives the tests access to protected members of the dispatcher.
	{
	public:
		using WebServerDispatcher::WebServerDispatcher;
		using WebServerDispatcher::cleanPath;
		using WebServerDispatcher::isResourcePath;
		using WebServerDispatcher::findResource;
	};

	class TestAuthService: public Poco::OSP::Auth::AuthService
		/// Knows the user "alice" with the password "secret" and the permission "read".
	{
	public:
		[[nodiscard]] bool authenticate(const std::string& userName, const std::string& credentials) const override
		{
			return userName == "alice" && credentials == "secret";
		}

		[[nodiscard]] bool authorize(const std::string& userName, const std::string& permission) const override
		{
			return userName == "alice" && permission == "read";
		}

		[[nodiscard]] bool authorize(const std::string& userName, const std::string& /*scope*/,
			const std::string& permission) const override
		{
			return authorize(userName, permission);
		}

		[[nodiscard]] bool userExists(const std::string& userName) const override
		{
			return userName == "alice";
		}
	};

	class AccountNameAuthService: public TestAuthService
		/// Accepts the password "secret" of "ALICE@example.com" as a login of the user "alice".
	{
	public:
		AccountNameAuthService() = default;
		AccountNameAuthService(const AccountNameAuthService&) = delete;
		AccountNameAuthService(AccountNameAuthService&&) = delete;
		AccountNameAuthService& operator = (const AccountNameAuthService&) = delete;
		AccountNameAuthService& operator = (AccountNameAuthService&&) = delete;
		~AccountNameAuthService() override = default;

		bool authenticateAccount(const std::string& userName, const std::string& credentials,
			std::string& accountName) const override
		{
			if (userName != "ALICE@example.com" || credentials != "secret") return false;
			accountName = "alice";
			return true;
		}
	};

	class TestTokenValidator: public TokenValidator
		/// Accepts the token "alice-token" for the user "alice" in the scope "api".
	{
	public:
		[[nodiscard]] bool validateToken(const std::string& token, std::string& username, std::string& scope) override
		{
			if (token != "alice-token") return false;
			username = "alice";
			scope = "api";
			return true;
		}
	};

	class DisablingAuthService: public TestAuthService
		/// Reports the user "alice" as enabled until disableAlice() is called.
	{
	public:
		[[nodiscard]] bool userEnabled(const std::string& userName) const override
		{
			return userName != "alice" || !_aliceDisabled;
		}

		void disableAlice()
		{
			_aliceDisabled = true;
		}

	private:
		std::atomic<bool> _aliceDisabled{false};
	};

	class BlockingAuthService: public TestAuthService
		/// Blocks in authenticate() until released, to keep a request inside authorization.
	{
	public:
		[[nodiscard]] bool authenticate(const std::string& userName, const std::string& credentials) const override
		{
			_entered.set();
			_released.tryWait(WAIT_TIMEOUT_MS);
			return TestAuthService::authenticate(userName, credentials);
		}

		[[nodiscard]] bool waitUntilEntered()
		{
			return _entered.tryWait(WAIT_TIMEOUT_MS);
		}

		void unblock()
			/// Named so as not to hide RefCountedObject::release().
		{
			_released.set();
		}

	private:
		static constexpr long WAIT_TIMEOUT_MS = 20000;

		mutable Poco::Event _entered;
		mutable Poco::Event _released{Poco::Event::EVENT_MANUALRESET};
	};

	class AuthorizedUserHandler: public Poco::Net::HTTPRequestHandler
		/// Answers with the user and the scope that the dispatcher passed in the request,
		/// as "<user>|<scope>", with "-" for a missing value.
	{
	public:
		void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) override
		{
			const std::string body(
				request.get(AUTHORIZED_USER_HEADER, "-"s) + "|" +
				request.get(AUTHORIZED_SCOPE_HEADER, "-"s));
			response.sendBuffer(body.data(), body.size());
		}
	};

	class RequestIdentityHandler: public Poco::Net::HTTPRequestHandler
		/// Answers with the user and the scope of the RequestIdentity of the thread,
		/// as "<user>|<scope>", with "-" for a missing value.
	{
	public:
		void handleRequest(Poco::Net::HTTPServerRequest&, Poco::Net::HTTPServerResponse& response) override
		{
			const Poco::OSP::Auth::RequestIdentity* pIdentity = Poco::OSP::Auth::RequestIdentity::current();
			std::string body("-|-");
			if (pIdentity != nullptr)
			{
				body = pIdentity->user() + "|" + (pIdentity->scope().empty() ? "-"s : pIdentity->scope());
			}
			response.sendBuffer(body.data(), body.size());
		}
	};

	class ThrowingIdentityHandlerFactory: public Poco::Net::HTTPRequestHandlerFactory
		/// Creates handlers that store the user of the RequestIdentity of the thread,
		/// or "-", in the given string, and then throw.
	{
	public:
		explicit ThrowingIdentityHandlerFactory(std::string& user):
			_pUser(&user)
		{
		}

		[[nodiscard]] Poco::Net::HTTPRequestHandler* createRequestHandler(const Poco::Net::HTTPServerRequest&) override
		{
			return new Handler(*_pUser);
		}

	private:
		class Handler: public Poco::Net::HTTPRequestHandler
		{
		public:
			explicit Handler(std::string& user):
				_pUser(&user)
			{
			}

			void handleRequest(Poco::Net::HTTPServerRequest&, Poco::Net::HTTPServerResponse&) override
			{
				const Poco::OSP::Auth::RequestIdentity* pIdentity = Poco::OSP::Auth::RequestIdentity::current();
				*_pUser = pIdentity != nullptr ? pIdentity->user() : "-"s;
				throw Poco::IllegalStateException("the handler failed"s);
			}

		private:
			std::string* _pUser;
		};

		std::string* _pUser;
	};

	class BodyHandler: public Poco::Net::HTTPRequestHandler
		/// Answers with the request body.
	{
	public:
		void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) override
		{
			std::string body;
			Poco::StreamCopier::copyToString(request.stream(), body);
			response.sendBuffer(body.data(), body.size());
		}
	};

	class ControlCharacterResponseHandler: public Poco::Net::HTTPRequestHandler
		/// Answers with a reason phrase and a header field that hold control characters.
	{
	public:
		void handleRequest(Poco::Net::HTTPServerRequest&, Poco::Net::HTTPServerResponse& response) override
		{
			response.setStatusAndReason(HTTPResponse::HTTP_OK, "OK\x1b[2J"s);
			response.set("X-Note"s, "a\r\nInjected: b"s);
			response.sendBuffer("ok", 2);
		}
	};

	class CredentialAndNonTokenFieldResponseHandler: public Poco::Net::HTTPRequestHandler
		/// Answers with header fields named by a credential field name in either letter case,
		/// by names that are not HTTP tokens, and by X-Note.
	{
	public:
		void handleRequest(Poco::Net::HTTPServerRequest&, Poco::Net::HTTPServerResponse& response) override
		{
			response.add("Set-Cookie:sid"s, "response-colon"s);
			response.add("X\r\nSet-Cookie"s, "response-crlf"s);
			response.add(" Cookie "s, "response-padded"s);
			response.add("Cookie"s, "response-cookie"s);
			response.add("cookie"s, "response-lower"s);
			response.add("X-Note"s, "response-note"s);
			response.sendBuffer("ok", 2);
		}
	};

	template <class Handler>
	class TestHandlerFactory: public Poco::Net::HTTPRequestHandlerFactory
	{
	public:
		[[nodiscard]] Poco::Net::HTTPRequestHandler* createRequestHandler(const Poco::Net::HTTPServerRequest&) override
		{
			return new Handler;
		}
	};

	class LoginHandler: public Poco::Net::HTTPRequestHandler
		/// Creates a session of the application "app" for the user "alice" and answers
		/// with its CSRF token. With the query "withoutToken", the session has no token;
		/// with the query "withoutUser", it has no user.
	{
	public:
		LoginHandler(const WebSessionManager::Ptr& pSessionManager, const BundleContext::Ptr& pContext):
			_pSessionManager(pSessionManager),
			_pContext(pContext)
		{
		}

		void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) override
		{
			WebSession::Ptr pSession = _pSessionManager->create("app"s, request, SESSION_TIMEOUT_SECONDS, _pContext);
			const std::string query(Poco::URI(request.getURI()).getQuery());
			if (query != "withoutUser") pSession->setValue<std::string>("username"s, "alice"s);
			if (query == "withoutToken") pSession->erase(WebSession::CSRF_TOKEN);
			const std::string token(pSession->getValue<std::string>(WebSession::CSRF_TOKEN, ""s));
			response.sendBuffer(token.data(), token.size());
		}

	private:
		WebSessionManager::Ptr _pSessionManager;
		BundleContext::Ptr _pContext;
	};

	class LoginHandlerFactory: public Poco::Net::HTTPRequestHandlerFactory
	{
	public:
		LoginHandlerFactory(const WebSessionManager::Ptr& pSessionManager, const BundleContext::Ptr& pContext):
			_pSessionManager(pSessionManager),
			_pContext(pContext)
		{
		}

		[[nodiscard]] Poco::Net::HTTPRequestHandler* createRequestHandler(const Poco::Net::HTTPServerRequest&) override
		{
			return new LoginHandler(_pSessionManager, _pContext);
		}

	private:
		WebSessionManager::Ptr _pSessionManager;
		BundleContext::Ptr _pContext;
	};

	class NamedFilter: public WebFilter
		/// Answers with its name instead of the resource.
	{
	public:
		explicit NamedFilter(const std::string& name):
			_name(name)
		{
		}

		void process(Poco::Net::HTTPServerRequest&, Poco::Net::HTTPServerResponse& response, const std::string&, std::istream&, Bundle::ConstPtr) override
		{
			response.send() << _name;
		}

	private:
		std::string _name;
	};

	class NamedFilterFactory: public WebFilterFactory
		/// Creates NamedFilter objects with the given name.
	{
	public:
		explicit NamedFilterFactory(const std::string& name):
			_name(name)
		{
		}

		[[nodiscard]] WebFilter* createFilter(const WebFilter::Args&) override
		{
			return new NamedFilter(_name);
		}

	private:
		std::string _name;
	};

	[[nodiscard]] WebServerDispatcher::VirtualPath handlerPath(const std::string& path, const std::string& permission,
		const Bundle::Ptr& pOwner, int authMethods = 0)
		/// Returns a mapping of path to an AuthorizedUserHandler that requires the given permission.
	{
		WebServerDispatcher::VirtualPath vPath(path, new TestHandlerFactory<AuthorizedUserHandler>, WebServerDispatcher::PathSecurity(WebServerDispatcher::SM_OWNER, ""s, permission), pOwner);
		vPath.security.authMethods = authMethods;
		return vPath;
	}

	class TestServer
		/// An HTTP server on 127.0.0.1, on a port chosen by the system,
		/// that passes every request to the given dispatcher.
	{
	public:
		explicit TestServer(WebServerDispatcher& dispatcher):
			_server(
				new WebServerRequestHandlerFactory(dispatcher, false),
				Poco::Net::ServerSocket(Poco::Net::SocketAddress("127.0.0.1"s, 0)),
				new Poco::Net::HTTPServerParams)
		{
			_server.start();
		}

		~TestServer()
		{
			_server.stopAll(true);
		}

		TestServer(const TestServer&) = delete;
		TestServer(TestServer&&) = delete;
		TestServer& operator = (const TestServer&) = delete;
		TestServer& operator = (TestServer&&) = delete;

		[[nodiscard]] Poco::UInt16 port() const
		{
			return _server.port();
		}

		std::string send(HTTPRequest& request, HTTPResponse& response, const std::string& body = std::string())
			/// Sends the request on a new connection and returns the response body.
		{
			Poco::Net::HTTPClientSession session("127.0.0.1"s, _server.port());
			session.setTimeout(CLIENT_TIMEOUT);
			request.setContentLength(static_cast<std::streamsize>(body.size()));
			session.sendRequest(request) << body;
			std::string responseBody;
			Poco::StreamCopier::copyToString(session.receiveResponse(response), responseBody);
			return responseBody;
		}

	private:
		Poco::Net::HTTPServer _server;
	};

	struct Session
		/// The cookies and the CSRF token that the LoginHandler answered with.
	{
		Poco::Net::NameValueCollection cookies;
		std::string csrfToken;
	};

	[[nodiscard]] Session loginSession(TestServer& server, const std::string& uri)
		/// Sends a GET request for uri, which must be mapped to a LoginHandlerFactory.
	{
		HTTPRequest request(HTTPRequest::HTTP_GET, uri);
		HTTPResponse response;
		Session session;
		session.csrfToken = server.send(request, response);
		std::vector<Poco::Net::HTTPCookie> cookies;
		response.getCookies(cookies);
		for (const auto& cookie: cookies) session.cookies.add(cookie.getName(), cookie.getValue());
		return session;
	}

	[[nodiscard]] bool hasControlCharacter(const std::string& text)
		/// Returns true if text has a control character other than the line feed
		/// and the tab that separate the header fields in the full request log.
	{
		return std::any_of(text.begin(), text.end(), [](char ch) { return ch != '\n' && ch != '\t'
			&& Poco::Ascii::hasProperties(ch, Poco::Ascii::ACP_CONTROL); });
	}
}


struct WebServerDispatcherTest::Fixture
	/// The bundles of a TemporaryBundle and a media type mapper for them.
{
	Fixture():
		bundles(BUNDLE_PROPERTIES),
		pMediaTypeMapper(new MediaTypeMapper)
	{
		pMediaTypeMapper->add("txt"s, "text/plain"s);
		pMediaTypeMapper->add("html"s, "text/html"s);
	}

	[[nodiscard]] WebServerDispatcher::Ptr createDispatcher(int options = 0) const
		/// Returns a dispatcher that maps /dir to www/ of the directory
		/// bundle and /zip to www/ of the ZIP bundle.
	{
		WebServerDispatcher::Ptr pDispatcher = new WebServerDispatcher(dispatcherConfig(options));
		pDispatcher->addVirtualPath(WebServerDispatcher::VirtualPath("/dir"s, "www"s, WebServerDispatcher::PathSecurity(), bundles.bundle()));
		pDispatcher->addVirtualPath(WebServerDispatcher::VirtualPath("/zip"s, "www"s, WebServerDispatcher::PathSecurity(), bundles.zipBundle()));
		return pDispatcher;
	}

	void writePersistentFiles() const
		/// Writes persist/file.txt to the persistent directory of the directory
		/// bundle, and files next to persist/ that must never be served.
	{
		const Path persistentDirectory(bundles.context()->persistentDirectory());
		TemporaryBundle::writeFile(persistentDirectory, "persist/file.txt"s, "persistent"s);
		TemporaryBundle::writeFile(persistentDirectory, "secret.txt"s, "secret"s);
		TemporaryBundle::writeFile(persistentDirectory, "index.html"s, "secret"s);
	}

	[[nodiscard]] WebServerDispatcher::Config dispatcherConfig(int options = 0) const
	{
		WebServerDispatcher::Config config;
		config.pContext = bundles.context();
		config.pMediaTypeMapper = pMediaTypeMapper;
		config.authServiceName = AUTH_SERVICE_NAME;
		config.tokenValidatorName = TOKEN_VALIDATOR_NAME;
		config.compressedMediaTypes = {"text/*"s};
		config.options = options;
		return config;
	}

	TemporaryBundle bundles;
	MediaTypeMapper::Ptr pMediaTypeMapper;
};


WebServerDispatcherTest::WebServerDispatcherTest(const std::string& name): CppUnit::TestCase(name)
{
}


WebServerDispatcherTest::~WebServerDispatcherTest() = default;


void WebServerDispatcherTest::testVirtualDirectoryRoot()
{
	WebServerDispatcher disp(_pFixture->dispatcherConfig());
	WebServerDispatcher::VirtualPath vPath;
	vPath.path = "/";
	vPath.resource = "/res";
	vPath.security.mode = WebServerDispatcher::SM_NONE;
	vPath.pOwnerBundle = _pFixture->bundles.bundle();
	disp.addVirtualPath(vPath);
	try
	{
		disp.addVirtualPath(vPath);
		fail("must fail");
	}
	catch(Poco::InvalidArgumentException&)
	{
	}
}


void WebServerDispatcherTest::testVirtualDirectoryAllow()
{
	WebServerDispatcher disp(_pFixture->dispatcherConfig());
	WebServerDispatcher::VirtualPath vPath;
	vPath.path = "/";
	vPath.resource = "res";
	vPath.security.mode = WebServerDispatcher::SM_ALL;
	vPath.pOwnerBundle = _pFixture->bundles.bundle();
	disp.addVirtualPath(vPath);

	vPath.path = "/sub";
	vPath.resource = "res2";
	vPath.security.mode = WebServerDispatcher::SM_OWNER;
	disp.addVirtualPath(vPath);

	vPath.path = "/sub/subsub";
	vPath.resource = "res3";
	vPath.security.mode = WebServerDispatcher::SM_NONE;
	disp.addVirtualPath(vPath);

	try
	{
		vPath.path = "/sub/subsub/subsubsub";
		vPath.resource = "res4";
		vPath.security.mode = WebServerDispatcher::SM_NONE;
		disp.addVirtualPath(vPath);

		fail("must fail");
	}
	catch(Poco::InvalidArgumentException&)
	{
	}
}


void WebServerDispatcherTest::testVirtualDirectoryFail()
{
	WebServerDispatcher disp(_pFixture->dispatcherConfig());

	WebServerDispatcher::VirtualPath vPath;
	vPath.path = "/sub/subsub";
	vPath.resource = "res";
	vPath.security.mode = WebServerDispatcher::SM_OWNER;
	vPath.pOwnerBundle = _pFixture->bundles.bundle();
	disp.addVirtualPath(vPath);

	vPath.path = "/sub";
	vPath.resource = "res2";
	vPath.security.mode = WebServerDispatcher::SM_ALL;
	disp.addVirtualPath(vPath);

	try
	{
		vPath.path = "/";
		vPath.resource = "res3";
		vPath.security.mode = WebServerDispatcher::SM_OWNER;
		disp.addVirtualPath(vPath);

		// must fail because we have a child with a less stricter restriction SM_ALL
		fail("must fail");
	}
	catch(Poco::InvalidArgumentException&)
	{
	}
}


void WebServerDispatcherTest::testChildInOtherCaseConflictsWithParent()
{
	WebServerDispatcher disp(_pFixture->dispatcherConfig());
	WebServerDispatcher::VirtualPath child("/ADMIN/sub"s, "res"s, WebServerDispatcher::PathSecurity(),
		_pFixture->bundles.zipBundle());
	child.security.mode = WebServerDispatcher::SM_ALL;
	disp.addVirtualPath(child);

	WebServerDispatcher::VirtualPath parent("/admin"s, "res"s, WebServerDispatcher::PathSecurity(),
		_pFixture->bundles.bundle());
	parent.security.mode = WebServerDispatcher::SM_NONE;
	try
	{
		disp.addVirtualPath(parent);
		fail("a parent mapping that allows no children must conflict with a child in another case");
	}
	catch (Poco::InvalidArgumentException&)
	{
	}
}


void WebServerDispatcherTest::testRemoveDir()
{
	WebServerDispatcher disp(_pFixture->dispatcherConfig());

	WebServerDispatcher::VirtualPath vPath;
	vPath.path = "/sub/";
	vPath.resource = "res";
	vPath.security.mode = WebServerDispatcher::SM_OWNER;
	vPath.pOwnerBundle = _pFixture->bundles.bundle();
	disp.addVirtualPath(vPath);

	vPath.path = "/sub/subsub";
	vPath.resource = "res2";
	vPath.security.mode = WebServerDispatcher::SM_NONE;
	disp.addVirtualPath(vPath);

	WebServerDispatcher::PathMap mappings;
	disp.virtualPathMappings(mappings);
	assertTrue (mappings.contains("/sub/subsub/"s));

	disp.removeVirtualPath("/sub/subsub");
	disp.virtualPathMappings(mappings);
	assertFalse (mappings.contains("/sub/subsub/"s));
	assertTrue (mappings.contains("/sub/"s));

	disp.addVirtualPath(vPath);
	disp.virtualPathMappings(mappings);
	assertTrue (mappings.contains("/sub/subsub/"s));
}


void WebServerDispatcherTest::testRemovePatternRemovesEveryMapping()
{
	WebServerDispatcher::Ptr pDispatcher = new WebServerDispatcher(_pFixture->dispatcherConfig());
	for (const auto& method: {HTTPRequest::HTTP_GET, HTTPRequest::HTTP_POST})
	{
		WebServerDispatcher::VirtualPath vPath(handlerPath("^/p/.*"s, ""s, _pFixture->bundles.bundle()));
		vPath.pPattern = new Poco::RegularExpression(vPath.path);
		vPath.methods.insert(method);
		pDispatcher->addVirtualPath(vPath);
	}
	TestServer server(*pDispatcher);

	HTTPRequest before(HTTPRequest::HTTP_POST, "/p/x"s);
	HTTPResponse response;
	server.send(before, response);
	assertTrue (response.getStatus() == HTTPResponse::HTTP_OK);

	pDispatcher->removeVirtualPath("^/p/.*"s);
	for (const auto& method: {HTTPRequest::HTTP_GET, HTTPRequest::HTTP_POST})
	{
		HTTPRequest after(method, "/p/x"s);
		server.send(after, response);
		assertTrue (response.getStatus() == HTTPResponse::HTTP_NOT_FOUND);
	}
	pDispatcher->removeVirtualPath("/not/mapped"s);
}


void WebServerDispatcherTest::testParseRange()
{
	using FileSize = Poco::File::FileSize;
	struct SatisfiableRange
	{
		std::string range;
		FileSize start;
		FileSize length;
	};
	const std::vector<SatisfiableRange> satisfiable = {
		{"bytes=0-99"s, 0, 100},
		{"bytes=10-19"s, 10, 10},
		{"bytes=0-999"s, 0, 100},
		{"bytes=90-"s, 90, 10},
		{"bytes=-10"s, 90, 10},
		{"bytes=-200"s, 0, 100},
		{"bytes=0-4, 10-20"s, 0, 5},
		{"bytes=500-, 10-19"s, 10, 10},
		{"BYTES=0-0"s, 0, 1}
	};
	for (const auto& expected: satisfiable)
	{
		FileSize rangeStart = 0;
		FileSize rangeLength = 0;
		assertTrue (WebServerDispatcher::parseRange(expected.range, 100, rangeStart, rangeLength));
		assertEqual (expected.start, rangeStart);
		assertEqual (expected.length, rangeLength);
	}

	for (const auto& range: {"bytes=50-10"s, "bytes=500-"s, "bytes=100-"s, "bytes=-"s, "bytes=-0"s, "bytes="s, "bytes=x-5"s,
		"bytes=0-5x"s, "bytes=0-5,x"s, "bytes=0-99999999999999999999999"s, "bytes=+1-5"s, "items=0-5"s, "bytes 0-5"s, "bytes=0 -5"s})
	{
		FileSize rangeStart = 0;
		FileSize rangeLength = 0;
		assertFalse (WebServerDispatcher::parseRange(range, 100, rangeStart, rangeLength));
	}

	FileSize rangeStart = 0;
	FileSize rangeLength = 0;
	assertFalse (WebServerDispatcher::parseRange("bytes=2048-4096"s, 1024, rangeStart, rangeLength));
	assertFalse (WebServerDispatcher::parseRange("bytes=0-"s, 0, rangeStart, rangeLength));
	assertFalse (WebServerDispatcher::parseRange("bytes=-5"s, 0, rangeStart, rangeLength));
}


void WebServerDispatcherTest::testCanonicalPathWithQuery()
{
	// No query in original - canonical path returned unchanged.
	assertEqual(std::string("/foo/"), WebServerDispatcher::canonicalPathWithQuery("/foo/", "/foo"));

	// Query preserved verbatim across the trailing-slash redirect.
	assertEqual(std::string("/foo/?bar=1"), WebServerDispatcher::canonicalPathWithQuery("/foo/", "/foo?bar=1"));

	// '+' in the source query value survives intact (regression: it must
	// not be re-encoded or stripped during canonicalization).
	assertEqual(std::string("/foo/?q=hello+world"), WebServerDispatcher::canonicalPathWithQuery("/foo/", "/foo?q=hello+world"));

	// Multi-parameter query preserved.
	assertEqual(std::string("/foo/?a=1&b=2"), WebServerDispatcher::canonicalPathWithQuery("/foo/", "/foo?a=1&b=2"));
}


void WebServerDispatcherTest::testRedirectLocationOn401()
{
	// Plain case: original path appended as percent-encoded redirect= parameter.
	{
		std::string loc = WebServerDispatcher::redirectLocationOn401("/login", "/protected/page");
		assertEqual(std::string("/login?redirect=%2Fprotected%2Fpage"), loc);
	}

	// '+' in the request query MUST be percent-encoded (HTMLForm decoders
	// treat raw '+' as space, which would corrupt the value on round-trip).
	{
		std::string loc = WebServerDispatcher::redirectLocationOn401("/login", "/search?q=hello+world");
		assertTrue(loc.find("%2B") != std::string::npos);
		assertTrue(loc.find("hello+world") == std::string::npos);
	}

	// Loop guard: request already targets the login path -> no redirect= appended,
	// even when the request carries an unrelated query.
	{
		std::string loc = WebServerDispatcher::redirectLocationOn401("/login", "/login");
		assertEqual(std::string("/login"), loc);
	}
	{
		std::string loc = WebServerDispatcher::redirectLocationOn401("/login", "/login?other=foo");
		assertEqual(std::string("/login"), loc);
	}

	// An absolute request target is passed on in origin form only.
	{
		std::string loc = WebServerDispatcher::redirectLocationOn401("/login", "http://other.example/x?y=1");
		assertEqual(std::string("/login?redirect=%2Fx%3Fy%3D1"), loc);
	}

	// A path that a browser would take for a reference to another host is passed on
	// with a single leading slash.
	for (const auto& target: {"//other.example/x"s, "/\\other.example/x"s, "/\\/other.example/x"s})
	{
		std::string loc = WebServerDispatcher::redirectLocationOn401("/login", target);
		assertEqual(std::string("/login?redirect=%2Fother.example%2Fx"), loc);
	}

	// Loop guard: request already carries its own redirect= -> don't nest another.
	{
		std::string loc = WebServerDispatcher::redirectLocationOn401("/login", "/login?redirect=%2Ffoo");
		assertEqual(std::string("/login"), loc);
	}
}


void WebServerDispatcherTest::testStaticResourceHasExactLength()
{
	for (const int options: {0, static_cast<int>(WebServerDispatcher::CONF_OPT_CACHE_RESOURCES)})
	{
		WebServerDispatcher::Ptr pDispatcher = _pFixture->createDispatcher(options);
		TestServer server(*pDispatcher);
		for (const auto& uri: {"/dir/hello.txt"s, "/zip/hello.txt"s})
		{
			HTTPRequest get(HTTPRequest::HTTP_GET, uri);
			HTTPResponse response;
			const std::string body = server.send(get, response);
			assertTrue (response.getStatus() == HTTPResponse::HTTP_OK);
			assertEqual(static_cast<Poco::Int64>(TemporaryBundle::HELLO_TXT.size()), response.getContentLength64());
			assertEqual(TemporaryBundle::HELLO_TXT, body);

			HTTPRequest head(HTTPRequest::HTTP_HEAD, uri);
			server.send(head, response);
			assertTrue (response.getStatus() == HTTPResponse::HTTP_OK);
			assertEqual(static_cast<Poco::Int64>(TemporaryBundle::HELLO_TXT.size()), response.getContentLength64());
		}
	}
}


void WebServerDispatcherTest::testCachedResourceOutlivesBundleStop()
{
	TestDispatcher dispatcher(_pFixture->dispatcherConfig(WebServerDispatcher::CONF_OPT_CACHE_RESOURCES));
	std::string mediaType;
	std::string resolvedPath;
	// The first stream is created with the cache entry, the second from the existing one.
	std::unique_ptr<std::istream> pStream(dispatcher.findResource(_pFixture->bundles.bundle(), "www"s, "hello.txt"s,
		"index.html"s, mediaType, resolvedPath, true));
	assertTrue (pStream != nullptr);
	std::unique_ptr<std::istream> pCachedStream(dispatcher.findResource(_pFixture->bundles.bundle(), "www"s,
		"hello.txt"s, "index.html"s, mediaType, resolvedPath, true));
	assertTrue (pCachedStream != nullptr);

	BundleEvent stopping(_pFixture->bundles.bundle(), BundleEvent::EV_BUNDLE_STOPPING);
	_pFixture->bundles.events().bundleStopping(this, stopping);

	for (auto* pResourceStream: {pStream.get(), pCachedStream.get()})
	{
		std::string content;
		Poco::StreamCopier::copyToString(*pResourceStream, content);
		assertEqual (TemporaryBundle::HELLO_TXT, content);
	}
}


void WebServerDispatcherTest::testCleanPathRejectsDotSegments()
{
	for (const auto& rejected: {"/a/../b"s, "/./x"s, "/a/b/.."s, "/a/b/."s, "/a\\..\\b"s, Poco::URI("/a/%2e%2e/b"s).getPath(), ""s})
	{
		std::string path(rejected);
		assertFalse (TestDispatcher::cleanPath(path));
	}

	std::string path("/a\\b/c.txt.");
	assertTrue (TestDispatcher::cleanPath(path));
	assertEqual ("/a/b/c.txt"s, path);

	path = "/a/.b/c...d/";
	assertTrue (TestDispatcher::cleanPath(path));
	assertEqual ("/a/.b/c...d/"s, path);
}


void WebServerDispatcherTest::testCleanPathRejectsControlCharacters()
{
	for (const auto& rejected: {"/a.txt\0.html"s, "/a\x1b[2J.txt"s, "/a\x7f.txt"s,
		Poco::URI("/a.txt%00.html"s).getPath()})
	{
		std::string path(rejected);
		assertFalse (TestDispatcher::cleanPath(path));
	}
}


void WebServerDispatcherTest::testCleanPathRejectsSegmentsEndingWithDotOrSpace()
{
	for (const auto& rejected: {"/admin./secret.txt"s, "/admin../secret.txt"s, "/admin /secret.txt"s, "/hello.txt "s,
		"/a/b /"s})
	{
		std::string path(rejected);
		assertFalse (TestDispatcher::cleanPath(path));
	}
}


void WebServerDispatcherTest::testCaseVariantOfProtectedPathIsProtected()
{
	_pFixture->bundles.registry().registerService(AUTH_SERVICE_NAME, new TestAuthService, Properties());
	TemporaryBundle::writeFile(Path(_pFixture->bundles.bundle()->path()), "www/admin/secret.txt"s, "secret"s);
	WebServerDispatcher::Ptr pDispatcher = new WebServerDispatcher(_pFixture->dispatcherConfig());
	pDispatcher->addVirtualPath(WebServerDispatcher::VirtualPath("/"s, "www"s, WebServerDispatcher::PathSecurity(),
		_pFixture->bundles.bundle()));
	pDispatcher->addVirtualPath(WebServerDispatcher::VirtualPath("/admin"s, "www/admin"s,
		WebServerDispatcher::PathSecurity(WebServerDispatcher::SM_OWNER, ""s, "read"s), _pFixture->bundles.bundle()));
	try
	{
		pDispatcher->addVirtualPath(WebServerDispatcher::VirtualPath("/Admin"s, "www"s,
			WebServerDispatcher::PathSecurity(), _pFixture->bundles.bundle()));
		fail("a mapping that differs from another one only in case must be rejected");
	}
	catch (Poco::InvalidArgumentException&)
	{
	}
	TestServer server(*pDispatcher);

	const std::vector<std::pair<std::string, HTTPResponse::HTTPStatus>> expected{
		{"/admin/secret.txt"s, HTTPResponse::HTTP_UNAUTHORIZED},
		{"/Admin/secret.txt"s, HTTPResponse::HTTP_UNAUTHORIZED},
		{"/ADMIN/secret.txt"s, HTTPResponse::HTTP_UNAUTHORIZED},
		{"/admin./secret.txt"s, HTTPResponse::HTTP_BAD_REQUEST}
	};
	for (const auto& [uri, status]: expected)
	{
		HTTPRequest request(HTTPRequest::HTTP_GET, uri);
		HTTPResponse response;
		const std::string body = server.send(request, response);
		assertTrue (response.getStatus() == status);
		assertTrue (body.find("secret"s) == std::string::npos);
	}

	HTTPRequest request(HTTPRequest::HTTP_GET, "/Admin/secret.txt"s);
	Poco::Net::HTTPBasicCredentials("alice"s, "secret"s).authenticate(request);
	HTTPResponse response;
	assertEqual ("secret"s, server.send(request, response));
}


void WebServerDispatcherTest::testResourcePathRejectsOtherNamesOfFiles()
{
	for (const auto& rejected: {"/page.psp::$DATA"s, "/a:b/page.psp"s, "/c:/page.psp"s})
	{
		assertFalse (TestDispatcher::isResourcePath(rejected));
	}
#if defined(POCO_OS_FAMILY_WINDOWS)
	assertFalse (TestDispatcher::isResourcePath("/ADMINI~1/secret.txt"s));
#else
	assertTrue (TestDispatcher::isResourcePath("/ADMINI~1/secret.txt"s));
#endif
	assertTrue (TestDispatcher::isResourcePath("/backup~/page~a.psp"s));
}


void WebServerDispatcherTest::testColonAnswersBadRequestForResourcesOnly()
{
	WebServerDispatcher::Ptr pDispatcher = _pFixture->createDispatcher();
	pDispatcher->addVirtualPath(handlerPath("/api"s, ""s, _pFixture->bundles.bundle()));
	TestServer server(*pDispatcher);

	HTTPRequest resource(HTTPRequest::HTTP_GET, "/dir/hello.txt::$DATA"s);
	HTTPResponse response;
	server.send(resource, response);
	assertTrue (response.getStatus() == HTTPResponse::HTTP_BAD_REQUEST);

	HTTPRequest handler(HTTPRequest::HTTP_GET, "/api/urn:item:1"s);
	assertEqual ("-|-"s, server.send(handler, response));
	assertTrue (response.getStatus() == HTTPResponse::HTTP_OK);
}


void WebServerDispatcherTest::testNonASCIIVariantOfProtectedPathIsProtected()
{
	// The protected path is U+00FC "ber" in normalization form C; the requests name it in other cases and forms.
	_pFixture->bundles.registry().registerService(AUTH_SERVICE_NAME, new TestAuthService, Properties());
	TemporaryBundle::writeFile(Path(_pFixture->bundles.bundle()->path()), "www/\xC3\xBC" "ber/secret.txt"s, "secret"s);
	WebServerDispatcher::Ptr pDispatcher = new WebServerDispatcher(_pFixture->dispatcherConfig());
	pDispatcher->addVirtualPath(WebServerDispatcher::VirtualPath("/"s, "www"s, WebServerDispatcher::PathSecurity(),
		_pFixture->bundles.bundle()));
	pDispatcher->addVirtualPath(WebServerDispatcher::VirtualPath("/\xC3\xBC" "ber"s, "www/\xC3\xBC" "ber"s,
		WebServerDispatcher::PathSecurity(WebServerDispatcher::SM_OWNER, ""s, "read"s), _pFixture->bundles.bundle()));
	for (const auto& variant: {"/\xC3\x9C" "BER"s, "/u\xCC\x88" "ber"s})
	{
		try
		{
			pDispatcher->addVirtualPath(WebServerDispatcher::VirtualPath(variant, "www"s,
				WebServerDispatcher::PathSecurity(), _pFixture->bundles.bundle()));
			fail("a mapping that differs from another one only in case or normalization form must be rejected");
		}
		catch (Poco::InvalidArgumentException&)
		{
		}
	}
	TestServer server(*pDispatcher);

	for (const auto& uri: {"/%C3%9CBER/secret.txt"s, "/u%CC%88ber/secret.txt"s, "/U%CC%88BER/secret.txt"s})
	{
		HTTPRequest request(HTTPRequest::HTTP_GET, uri);
		HTTPResponse response;
		const std::string body = server.send(request, response);
		assertTrue (response.getStatus() == HTTPResponse::HTTP_UNAUTHORIZED);
		assertTrue (body.find("secret"s) == std::string::npos);

		Poco::Net::HTTPBasicCredentials("alice"s, "secret"s).authenticate(request);
		assertEqual ("secret"s, server.send(request, response));
	}
}


void WebServerDispatcherTest::testLongerFormOfProtectedPathIsProtected()
{
	// U+212A KELVIN SIGN is 'K' in normalization form C, and two bytes longer in UTF-8.
	_pFixture->bundles.registry().registerService(AUTH_SERVICE_NAME, new TestAuthService, Properties());
	TemporaryBundle::writeFile(Path(_pFixture->bundles.bundle()->path()), "www/kelvin/secret.txt"s, "secret"s);
	WebServerDispatcher::Ptr pDispatcher = new WebServerDispatcher(_pFixture->dispatcherConfig());
	pDispatcher->addVirtualPath(WebServerDispatcher::VirtualPath("/"s, "www"s, WebServerDispatcher::PathSecurity(),
		_pFixture->bundles.bundle()));
	pDispatcher->addVirtualPath(WebServerDispatcher::VirtualPath("/kelvin"s, "www/kelvin"s,
		WebServerDispatcher::PathSecurity(WebServerDispatcher::SM_OWNER, ""s, "read"s), _pFixture->bundles.bundle()));
	TestServer server(*pDispatcher);

	HTTPRequest request(HTTPRequest::HTTP_GET, "/%E2%84%AAelvin/secret.txt"s);
	HTTPResponse response;
	const std::string body = server.send(request, response);
	assertTrue (response.getStatus() == HTTPResponse::HTTP_UNAUTHORIZED);
	assertTrue (body.find("secret"s) == std::string::npos);

	Poco::Net::HTTPBasicCredentials("alice"s, "secret"s).authenticate(request);
	assertEqual ("secret"s, server.send(request, response));
}


void WebServerDispatcherTest::testFoldedSpellingOfProtectedPathIsNotFound()
{
	// U+017F is its own lower-case form; case-insensitive APFS opens it as 's'.
	_pFixture->bundles.registry().registerService(AUTH_SERVICE_NAME, new TestAuthService, Properties());
	TemporaryBundle::writeFile(Path(_pFixture->bundles.bundle()->path()), "www/secret/a.txt"s, "secret"s);
	WebServerDispatcher::Ptr pDispatcher = new WebServerDispatcher(_pFixture->dispatcherConfig());
	pDispatcher->addVirtualPath(WebServerDispatcher::VirtualPath("/"s, "www"s, WebServerDispatcher::PathSecurity(),
		_pFixture->bundles.bundle()));
	pDispatcher->addVirtualPath(WebServerDispatcher::VirtualPath("/secret"s, "www/secret"s,
		WebServerDispatcher::PathSecurity(WebServerDispatcher::SM_OWNER, ""s, "read"s), _pFixture->bundles.bundle()));
	TestServer server(*pDispatcher);

	HTTPRequest request(HTTPRequest::HTTP_GET, "/%C5%BFecret/a.txt"s);
	HTTPResponse response;
	server.send(request, response);
	assertTrue (response.getStatus() == HTTPResponse::HTTP_NOT_FOUND);
}


void WebServerDispatcherTest::testExactMatchIgnoresCase()
{
	WebServerDispatcher::Ptr pDispatcher = new WebServerDispatcher(_pFixture->dispatcherConfig());
	WebServerDispatcher::VirtualPath vPath(handlerPath("/exact"s, ""s, _pFixture->bundles.bundle()));
	vPath.exactMatch = true;
	pDispatcher->addVirtualPath(vPath);
	TestServer server(*pDispatcher);

	for (const auto& uri: {"/exact"s, "/EXACT"s, "/Exact/"s})
	{
		HTTPRequest request(HTTPRequest::HTTP_GET, uri);
		HTTPResponse response;
		server.send(request, response);
		assertTrue (response.getStatus() == HTTPResponse::HTTP_OK);
	}
	HTTPRequest below(HTTPRequest::HTTP_GET, "/EXACT/below"s);
	HTTPResponse response;
	server.send(below, response);
	assertTrue (response.getStatus() == HTTPResponse::HTTP_NOT_FOUND);
}


void WebServerDispatcherTest::testControlCharacterInPathAnswersBadRequest()
{
	_pFixture->writePersistentFiles();
	WebServerDispatcher::Ptr pDispatcher = _pFixture->createDispatcher();
	pDispatcher->addVirtualPath(WebServerDispatcher::VirtualPath("/persist"s, "persist"s,
		WebServerDispatcher::PathSecurity(), _pFixture->bundles.bundle()));
	TestServer server(*pDispatcher);

	for (const auto& uri: {"/dir/hello.txt%00.html"s, "/persist/file.txt%00.html"s, "/dir/hello.txt%0a"s})
	{
		HTTPRequest request(HTTPRequest::HTTP_GET, uri);
		HTTPResponse response;
		server.send(request, response);
		assertTrue (response.getStatus() == HTTPResponse::HTTP_BAD_REQUEST);
	}
}


void WebServerDispatcherTest::testDotSegmentsAnswerBadRequest()
{
	_pFixture->writePersistentFiles();
	WebServerDispatcher::Ptr pDispatcher = new WebServerDispatcher(_pFixture->dispatcherConfig());
	const WebServerDispatcher::PathSecurity security(WebServerDispatcher::SM_OWNER, ""s, ""s);
	pDispatcher->addVirtualPath(WebServerDispatcher::VirtualPath("/"s, "persist"s, security, _pFixture->bundles.bundle()));
	pDispatcher->addVirtualPath(WebServerDispatcher::VirtualPath("/dir"s, "www"s, security, _pFixture->bundles.bundle()));
	TestServer server(*pDispatcher);

	for (const auto& uri: {"/x/../../secret.txt"s, "/%2e%2e/secret.txt"s, "/dir/x/../hello.txt"s, "/dir/./hello.txt"s})
	{
		HTTPRequest request(HTTPRequest::HTTP_GET, uri);
		HTTPResponse response;
		server.send(request, response);
		assertTrue (response.getStatus() == HTTPResponse::HTTP_BAD_REQUEST);
	}

	HTTPRequest request(HTTPRequest::HTTP_GET, "/file.txt"s);
	HTTPResponse response;
	assertEqual ("persistent"s, server.send(request, response));
	assertTrue (response.getStatus() == HTTPResponse::HTTP_OK);
}


void WebServerDispatcherTest::testMalformedURIAnswersBadRequest()
{
	WebServerDispatcher::Ptr pDispatcher = _pFixture->createDispatcher();
	TestServer server(*pDispatcher);
	LogCapture capture(_pFixture->bundles.context()->logger().name());
	Poco::Logger::get(_pFixture->bundles.context()->logger().name()).setLevel(Poco::Message::PRIO_DEBUG);

	HTTPRequest request(HTTPRequest::HTTP_GET, "/dir/%zz"s);
	HTTPResponse response;
	server.send(request, response);

	assertTrue (response.getStatus() == HTTPResponse::HTTP_BAD_REQUEST);
	const std::vector<Poco::Message> messages = capture.channel().waitForMessages(1);
	assertFalse (messages.empty());
	for (const auto& message: messages)
	{
		assertTrue (message.getPriority() == Poco::Message::PRIO_DEBUG);
	}
}


void WebServerDispatcherTest::testPersistentResourceStaysInsideItsDirectory()
{
	_pFixture->writePersistentFiles();
	TestDispatcher dispatcher(_pFixture->dispatcherConfig());
	std::string mediaType;
	std::string resolvedPath;
	for (const auto& res: {"../secret.txt"s, "x/../../secret.txt"s, ".."s})
	{
		std::unique_ptr<std::istream> pStream(dispatcher.findResource(_pFixture->bundles.bundle(), "persist"s, res, "index.html"s, mediaType, resolvedPath, false));
		assertTrue (pStream == nullptr);
	}

	std::unique_ptr<std::istream> pStream(dispatcher.findResource(_pFixture->bundles.bundle(), "persist"s, "file.txt"s, "index.html"s, mediaType, resolvedPath, false));
	assertTrue (pStream != nullptr);
	std::string content;
	Poco::StreamCopier::copyToString(*pStream, content);
	assertEqual ("persistent"s, content);
}


void WebServerDispatcherTest::testPersistentResourceEndingWithSlash()
{
	_pFixture->writePersistentFiles();
	WebServerDispatcher::Ptr pDispatcher = _pFixture->createDispatcher();
	pDispatcher->addVirtualPath(WebServerDispatcher::VirtualPath("/persist"s, "persist/"s, WebServerDispatcher::PathSecurity(), _pFixture->bundles.bundle()));
	TestServer server(*pDispatcher);

	HTTPRequest request(HTTPRequest::HTTP_GET, "/persist/file.txt"s);
	HTTPResponse response;
	assertEqual ("persistent"s, server.send(request, response));
	assertTrue (response.getStatus() == HTTPResponse::HTTP_OK);
}


void WebServerDispatcherTest::testPersistentResourceWithEmptySegment()
{
	_pFixture->writePersistentFiles();
	WebServerDispatcher::Ptr pDispatcher = _pFixture->createDispatcher();
	pDispatcher->addVirtualPath(WebServerDispatcher::VirtualPath("/persist"s, "persist"s,
		WebServerDispatcher::PathSecurity(), _pFixture->bundles.bundle()));
	TestServer server(*pDispatcher);

	HTTPRequest request(HTTPRequest::HTTP_GET, "/persist//file.txt"s);
	HTTPResponse response;
	assertEqual ("persistent"s, server.send(request, response));
	assertTrue (response.getStatus() == HTTPResponse::HTTP_OK);
}


void WebServerDispatcherTest::testPersistentResourceIsFoundOnlyUnderItsName()
{
	_pFixture->writePersistentFiles();
	TestDispatcher dispatcher(_pFixture->dispatcherConfig());
	std::string mediaType;
	std::string resolvedPath;
	std::unique_ptr<std::istream> pStream(dispatcher.findResource(_pFixture->bundles.bundle(), "persist"s, "FILE.TXT"s,
		"index.html"s, mediaType, resolvedPath, false));
	assertTrue (pStream == nullptr);

	const Path persistentDirectory(_pFixture->bundles.context()->persistentDirectory());
	TemporaryBundle::writeFile(persistentDirectory, "persist/sub/index.html"s, "index"s);
	std::unique_ptr<std::istream> pIndex(dispatcher.findResource(_pFixture->bundles.bundle(), "persist"s, "sub"s,
		"index.html"s, mediaType, resolvedPath, false));
	assertTrue (pIndex != nullptr);
	if (!Poco::File(Path(persistentDirectory, Path("persist/SUB/index.html"s, Path::PATH_UNIX))).exists())
	{
		warnmsg("The file system distinguishes case, index case skipped.");
		return;
	}
	std::unique_ptr<std::istream> pOtherCase(dispatcher.findResource(_pFixture->bundles.bundle(), "persist"s, "SUB"s,
		"index.html"s, mediaType, resolvedPath, false));
	assertTrue (pOtherCase == nullptr);
}


void WebServerDispatcherTest::testRangeRequests()
{
	WebServerDispatcher::Ptr pDispatcher = _pFixture->createDispatcher(WebServerDispatcher::CONF_OPT_ENABLE_ACCEPT_RANGE);
	TestServer server(*pDispatcher);

	HTTPRequest head(HTTPRequest::HTTP_HEAD, "/dir/hello.txt"s);
	HTTPResponse response;
	server.send(head, response);
	assertEqual ("bytes"s, response.get("Accept-Ranges"s, ""s));

	HTTPRequest suffix(HTTPRequest::HTTP_GET, "/dir/hello.txt"s);
	suffix.set("Range"s, "bytes=-5"s);
	assertEqual ("vwxyz"s, server.send(suffix, response));
	assertTrue (response.getStatus() == HTTPResponse::HTTP_PARTIAL_CONTENT);
	assertEqual ("bytes 21-25/26"s, response.get("Content-Range"s, ""s));

	HTTPRequest unsatisfiable(HTTPRequest::HTTP_GET, "/dir/hello.txt"s);
	unsatisfiable.set("Range"s, "bytes=500-"s);
	assertEqual (""s, server.send(unsatisfiable, response));
	assertTrue (response.getStatus() == HTTPResponse::HTTP_REQUESTED_RANGE_NOT_SATISFIABLE);
	assertEqual ("bytes */26"s, response.get("Content-Range"s, ""s));

	for (const auto& uri: {"/dir/hello.txt"s, "/zip/hello.txt"s})
	{
		HTTPRequest range(HTTPRequest::HTTP_GET, uri);
		range.set("Range"s, "bytes=3-7"s);
		assertEqual ("defgh"s, server.send(range, response));
		assertTrue (response.getStatus() == HTTPResponse::HTTP_PARTIAL_CONTENT);
		assertEqual ("bytes 3-7/26"s, response.get("Content-Range"s, ""s));
		assertEqual (static_cast<Poco::Int64>(5), response.getContentLength64());
	}

	HTTPRequest otherUnit(HTTPRequest::HTTP_GET, "/dir/hello.txt"s);
	otherUnit.set("Range"s, "items=0-4"s);
	assertEqual (TemporaryBundle::HELLO_TXT, server.send(otherUnit, response));
	assertTrue (response.getStatus() == HTTPResponse::HTTP_OK);
}


void WebServerDispatcherTest::testClientAuthorizationHeadersAreRemoved()
{
	_pFixture->bundles.registry().registerService(AUTH_SERVICE_NAME, new TestAuthService, Properties());
	WebServerDispatcher::Ptr pDispatcher = new WebServerDispatcher(_pFixture->dispatcherConfig(WebServerDispatcher::CONF_OPT_ADD_AUTH_HEADER));
	pDispatcher->addVirtualPath(handlerPath("/public"s, ""s, _pFixture->bundles.bundle()));
	pDispatcher->addVirtualPath(handlerPath("/optional"s, "**"s, _pFixture->bundles.bundle()));
	pDispatcher->addVirtualPath(handlerPath("/protected"s, "read"s, _pFixture->bundles.bundle()));
	TestServer server(*pDispatcher);

	for (const auto& uri: {"/public"s, "/optional"s})
	{
		HTTPRequest request(HTTPRequest::HTTP_GET, uri);
		request.set(AUTHORIZED_USER_HEADER, "mallory"s);
		request.set(AUTHORIZED_SCOPE_HEADER, "admin"s);
		HTTPResponse response;
		assertEqual ("-|-"s, server.send(request, response));
	}

	HTTPRequest request(HTTPRequest::HTTP_GET, "/protected"s);
	Poco::Net::HTTPBasicCredentials("alice"s, "secret"s).authenticate(request);
	request.set(AUTHORIZED_SCOPE_HEADER, "admin"s);
	HTTPResponse response;
	assertEqual ("alice|-"s, server.send(request, response));
}


void WebServerDispatcherTest::testRequestIdentityIsSetWhileHandlerRuns()
{
	_pFixture->bundles.registry().registerService(AUTH_SERVICE_NAME, new TestAuthService, Properties());
	_pFixture->bundles.registry().registerService(TOKEN_VALIDATOR_NAME, new TestTokenValidator, Properties());
	WebServerDispatcher::Ptr pDispatcher = new WebServerDispatcher(_pFixture->dispatcherConfig());
	const WebServerDispatcher::PathSecurity protectedPath(WebServerDispatcher::SM_OWNER, ""s, "read"s);
	pDispatcher->addVirtualPath(WebServerDispatcher::VirtualPath("/public"s,
		new TestHandlerFactory<RequestIdentityHandler>, WebServerDispatcher::PathSecurity(),
		_pFixture->bundles.bundle()));
	pDispatcher->addVirtualPath(WebServerDispatcher::VirtualPath("/protected"s,
		new TestHandlerFactory<RequestIdentityHandler>, protectedPath, _pFixture->bundles.bundle()));
	TestServer server(*pDispatcher);
	HTTPResponse response;

	HTTPRequest basic(HTTPRequest::HTTP_GET, "/protected"s);
	Poco::Net::HTTPBasicCredentials("alice"s, "secret"s).authenticate(basic);
	assertEqual ("alice|-"s, server.send(basic, response));

	HTTPRequest bearer(HTTPRequest::HTTP_GET, "/protected"s);
	bearer.setCredentials("Bearer"s, "alice-token"s);
	assertEqual ("alice|api"s, server.send(bearer, response));

	HTTPRequest claimed(HTTPRequest::HTTP_GET, "/public"s);
	claimed.set(AUTHORIZED_USER_HEADER, "mallory"s);
	assertEqual ("-|-"s, server.send(claimed, response));

	// In the thread that runs the handler, the identity ends with the handler, also if it throws.
	std::string handlerUser;
	pDispatcher->addVirtualPath(WebServerDispatcher::VirtualPath("/failing"s,
		new ThrowingIdentityHandlerFactory(handlerUser), protectedPath, _pFixture->bundles.bundle()));
	ServerRequestStub failing("127.0.0.1:50000"s, false);
	failing.setURI("/failing"s);
	Poco::Net::HTTPBasicCredentials("alice"s, "secret"s).authenticate(failing);
	pDispatcher->handleRequest(failing, failing.response(), false);
	assertEqual ("alice"s, handlerUser);
	assertTrue (failing.response().getStatus() == HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
	assertTrue (Poco::OSP::Auth::RequestIdentity::current() == nullptr);
}


void WebServerDispatcherTest::testAuthorizationDoesNotBlockOtherRequests()
{
	Poco::AutoPtr<BlockingAuthService> pAuthService = new BlockingAuthService;
	_pFixture->bundles.registry().registerService(AUTH_SERVICE_NAME, pAuthService, Properties());
	WebServerDispatcher::Ptr pDispatcher = _pFixture->createDispatcher();
	pDispatcher->addVirtualPath(handlerPath("/protected"s, "read"s, _pFixture->bundles.bundle()));
	TestServer server(*pDispatcher);

	std::thread protectedRequest([&server]()
	{
		HTTPRequest request(HTTPRequest::HTTP_GET, "/protected"s);
		Poco::Net::HTTPBasicCredentials("alice"s, "secret"s).authenticate(request);
		HTTPResponse response;
		try
		{
			server.send(request, response);
		}
		catch (Poco::TimeoutException&)
		{
			// Only the request in the main thread is checked.
		}
	});

	const bool entered = pAuthService->waitUntilEntered();
	std::string body;
	bool timedOut = false;
	if (entered)
	{
		HTTPRequest request(HTTPRequest::HTTP_GET, "/dir/hello.txt"s);
		HTTPResponse response;
		try
		{
			body = server.send(request, response);
		}
		catch (Poco::TimeoutException&)
		{
			timedOut = true;
		}
	}
	pAuthService->unblock();
	protectedRequest.join();

	assertTrue (entered);
	assertFalse (timedOut);
	assertEqual (TemporaryBundle::HELLO_TXT, body);
}


void WebServerDispatcherTest::testUnavailableAuthServiceAnswers503()
{
	WebServerDispatcher::Ptr pDispatcher = new WebServerDispatcher(_pFixture->dispatcherConfig());
	pDispatcher->addVirtualPath(handlerPath("/protected"s, "read"s, _pFixture->bundles.bundle()));
	TestServer server(*pDispatcher);
	const auto statusOfAuthenticatedRequest = [&server]()
	{
		HTTPRequest request(HTTPRequest::HTTP_GET, "/protected"s);
		Poco::Net::HTTPBasicCredentials("alice"s, "secret"s).authenticate(request);
		HTTPResponse response;
		server.send(request, response);
		return response.getStatus();
	};

	assertTrue (statusOfAuthenticatedRequest() == HTTPResponse::HTTP_SERVICE_UNAVAILABLE);
	ServiceRef::Ptr pAuthRef = _pFixture->bundles.registry().registerService(AUTH_SERVICE_NAME, new TestAuthService, Properties());
	assertTrue (statusOfAuthenticatedRequest() == HTTPResponse::HTTP_OK);
	_pFixture->bundles.registry().unregisterService(pAuthRef);
	assertTrue (statusOfAuthenticatedRequest() == HTTPResponse::HTTP_SERVICE_UNAVAILABLE);
}


void WebServerDispatcherTest::testUnavailableAuthServiceIsLoggedOnce()
{
	const auto messagesAt = [](CaptureChannel& channel, Poco::Message::Priority priority, const std::string& text)
		// Returns the logged messages with the given priority that contain text.
	{
		std::vector<Poco::Message> result;
		for (const auto& message: channel.waitForMessages(0))
		{
			if (message.getPriority() == priority && message.getText().find(text) != std::string::npos) result.push_back(message);
		}
		return result;
	};
	const auto warnings = [](CaptureChannel& channel)
	{
		std::vector<Poco::Message> result;
		for (const auto& message: channel.waitForMessages(0))
		{
			if (message.getPriority() <= Poco::Message::PRIO_WARNING) result.push_back(message);
		}
		return result;
	};
	{
		LogCapture capture(_pFixture->bundles.context()->logger().name());
		WebServerDispatcher::Ptr pDispatcher = new WebServerDispatcher(_pFixture->dispatcherConfig());
		pDispatcher->addVirtualPath(handlerPath("/protected"s, "read"s, _pFixture->bundles.bundle()));
		TestServer server(*pDispatcher);
		const auto status = [&server]()
		{
			HTTPRequest request(HTTPRequest::HTTP_GET, "/protected"s);
			HTTPResponse response;
			server.send(request, response);
			return response.getStatus();
		};

		assertTrue (status() == HTTPResponse::HTTP_SERVICE_UNAVAILABLE);
		assertTrue (status() == HTTPResponse::HTTP_SERVICE_UNAVAILABLE);
		assertTrue (warnings(capture.channel()).empty());
		assertEqual (1, messagesAt(capture.channel(), Poco::Message::PRIO_INFORMATION, AUTH_SERVICE_NAME + " is not registered yet"s).size());

		ServiceRef::Ptr pAuthRef = _pFixture->bundles.registry().registerService(AUTH_SERVICE_NAME, new TestAuthService, Properties());
		assertEqual (1, messagesAt(capture.channel(), Poco::Message::PRIO_INFORMATION, AUTH_SERVICE_NAME + " has been registered"s).size());
		_pFixture->bundles.registry().unregisterService(pAuthRef);
		assertTrue (status() == HTTPResponse::HTTP_SERVICE_UNAVAILABLE);
		assertTrue (status() == HTTPResponse::HTTP_SERVICE_UNAVAILABLE);
		assertTrue (warnings(capture.channel()).empty());
		assertEqual (1, messagesAt(capture.channel(), Poco::Message::PRIO_NOTICE, AUTH_SERVICE_NAME + " has been unregistered"s).size());
	}

	LogCapture capture(_pFixture->bundles.context()->logger().name());
	WebServerDispatcher::Config config(_pFixture->dispatcherConfig());
	config.authServiceName.clear();
	WebServerDispatcher::Ptr pDispatcher = new WebServerDispatcher(config);
	const std::vector<Poco::Message> messages = warnings(capture.channel());
	assertEqual (1, messages.size());
	assertTrue (messages[0].getText().find("No auth service is configured"s) != std::string::npos);
}


void WebServerDispatcherTest::testOptionalCredentialsWithoutAuthService()
{
	WebServerDispatcher::Ptr pDispatcher = new WebServerDispatcher(_pFixture->dispatcherConfig());
	pDispatcher->addVirtualPath(handlerPath("/optional"s, "**"s, _pFixture->bundles.bundle()));
	TestServer server(*pDispatcher);

	HTTPRequest anonymousRequest(HTTPRequest::HTTP_GET, "/optional"s);
	HTTPResponse anonymousResponse;
	assertEqual ("-|-"s, server.send(anonymousRequest, anonymousResponse));
	assertTrue (anonymousResponse.getStatus() == HTTPResponse::HTTP_OK);

	HTTPRequest authenticatedRequest(HTTPRequest::HTTP_GET, "/optional"s);
	Poco::Net::HTTPBasicCredentials("alice"s, "secret"s).authenticate(authenticatedRequest);
	HTTPResponse authenticatedResponse;
	server.send(authenticatedRequest, authenticatedResponse);
	assertTrue (authenticatedResponse.getStatus() == HTTPResponse::HTTP_SERVICE_UNAVAILABLE);
}


void WebServerDispatcherTest::testOptionalCredentialsWithSessionWithoutAuthService()
{
	WebSessionManager::Ptr pSessionManager = new WebSessionManager(_pFixture->bundles.context());
	pSessionManager->setCSRFCookie("XSRF-TOKEN"s);
	_pFixture->bundles.registry().registerService(WebSessionManager::SERVICE_NAME, pSessionManager, Properties());
	WebServerDispatcher::Ptr pDispatcher = new WebServerDispatcher(_pFixture->dispatcherConfig());
	pDispatcher->addVirtualPath(WebServerDispatcher::VirtualPath("/login"s, new LoginHandlerFactory(pSessionManager, _pFixture->bundles.context()), WebServerDispatcher::PathSecurity(), _pFixture->bundles.bundle()));
	WebServerDispatcher::VirtualPath optional(handlerPath("/optional"s, "**"s, _pFixture->bundles.bundle(), WebServerDispatcher::AUTH_SESSION));
	optional.security.session = "app";
	pDispatcher->addVirtualPath(optional);
	WebServerDispatcher::VirtualPath basicOnly(handlerPath("/basic"s, "**"s, _pFixture->bundles.bundle(), WebServerDispatcher::AUTH_BASIC));
	basicOnly.security.session = "app";
	pDispatcher->addVirtualPath(basicOnly);
	TestServer server(*pDispatcher);
	const auto get = [&server](const std::string& uri, const Session& session, HTTPResponse& response)
	{
		HTTPRequest request(HTTPRequest::HTTP_GET, uri);
		request.setCookies(session.cookies);
		server.send(request, response);
		return response.getStatus();
	};
	const Session userSession = loginSession(server, "/login"s);
	const Session anonymousSession = loginSession(server, "/login?withoutUser"s);

	HTTPResponse response;
	assertTrue (get("/optional"s, userSession, response) == HTTPResponse::HTTP_SERVICE_UNAVAILABLE);
	HTTPResponse anonymousResponse;
	assertTrue (get("/optional"s, anonymousSession, anonymousResponse) == HTTPResponse::HTTP_OK);
	std::vector<Poco::Net::HTTPCookie> cookies;
	anonymousResponse.getCookies(cookies);
	std::map<std::string, int> cookieCount;
	for (const auto& cookie: cookies) ++cookieCount[cookie.getName()];
	assertEqual (2, cookieCount.size());
	for (const auto& [name, count]: cookieCount) assertEqual (1, count);
	HTTPResponse noSessionResponse;
	assertTrue (get("/optional"s, Session(), noSessionResponse) == HTTPResponse::HTTP_OK);
	HTTPResponse basicOnlyResponse;
	assertTrue (get("/basic"s, userSession, basicOnlyResponse) == HTTPResponse::HTTP_OK);
}


void WebServerDispatcherTest::testUnavailableAnswerHasBundleMessage()
{
	Poco::FileInputStream propertiesStream(Path(Poco::Environment::get("POCO_BASE"s), "Web/bundle/bundle.properties"s).toString());
	std::string properties;
	Poco::StreamCopier::copyToString(propertiesStream, properties);
	TemporaryBundle bundle(properties);
	WebServerDispatcher::Config config(_pFixture->dispatcherConfig());
	config.pContext = bundle.context();
	WebServerDispatcher::Ptr pDispatcher = new WebServerDispatcher(config);
	pDispatcher->addVirtualPath(handlerPath("/protected"s, "read"s, bundle.bundle()));
	TestServer server(*pDispatcher);

	HTTPRequest request(HTTPRequest::HTTP_GET, "/protected"s);
	HTTPResponse response;
	const std::string body = server.send(request, response);

	assertTrue (response.getStatus() == HTTPResponse::HTTP_SERVICE_UNAVAILABLE);
	assertTrue (body.find("Authentication for /protected/ is not available"s) != std::string::npos);
}


void WebServerDispatcherTest::testAuthorizationResponses()
{
	_pFixture->bundles.registry().registerService(AUTH_SERVICE_NAME, new TestAuthService, Properties());
	_pFixture->bundles.registry().registerService(TOKEN_VALIDATOR_NAME, new TestTokenValidator, Properties());
	WebServerDispatcher::Ptr pDispatcher = new WebServerDispatcher(_pFixture->dispatcherConfig(WebServerDispatcher::CONF_OPT_ADD_AUTH_HEADER));
	pDispatcher->addVirtualPath(handlerPath("/protected"s, "read"s, _pFixture->bundles.bundle()));
	pDispatcher->addVirtualPath(handlerPath("/either"s, "write, read"s, _pFixture->bundles.bundle()));
	pDispatcher->addVirtualPath(handlerPath("/admin"s, "admin,write"s, _pFixture->bundles.bundle()));
	pDispatcher->addVirtualPath(handlerPath("/optional"s, "**"s, _pFixture->bundles.bundle()));
	pDispatcher->addVirtualPath(handlerPath("/bearer"s, "read"s, _pFixture->bundles.bundle(), WebServerDispatcher::AUTH_BEARER));
	TestServer server(*pDispatcher);

	struct Result
	{
		HTTPResponse::HTTPStatus status;
		std::string body;
		std::string challenge;
	};
	const auto get = [&server](const std::string& uri, const std::string& scheme, const std::string& authInfo)
	{
		HTTPRequest request(HTTPRequest::HTTP_GET, uri);
		if (!scheme.empty()) request.setCredentials(scheme, authInfo);
		HTTPResponse response;
		const std::string body = server.send(request, response);
		return Result{response.getStatus(), body, response.get("WWW-Authenticate"s, ""s)};
	};
	const std::string basic(Poco::Net::HTTPBasicCredentials::SCHEME);
	const std::string bearer("Bearer");
	const std::string aliceSecret("YWxpY2U6c2VjcmV0"); // alice:secret
	const std::string aliceWrong("YWxpY2U6d3Jvbmc="); // alice:wrong

	Result result = get("/protected"s, ""s, ""s);
	assertTrue (result.status == HTTPResponse::HTTP_UNAUTHORIZED);
	assertEqual (0, result.challenge.find("Basic "s));

	result = get("/bearer"s, ""s, ""s);
	assertTrue (result.status == HTTPResponse::HTTP_UNAUTHORIZED);
	assertEqual (0, result.challenge.find("Bearer "s));

	result = get("/optional"s, ""s, ""s);
	assertTrue (result.status == HTTPResponse::HTTP_OK);
	assertEqual ("-|-"s, result.body);

	result = get("/protected"s, basic, aliceSecret);
	assertTrue (result.status == HTTPResponse::HTTP_OK);
	assertEqual ("alice|-"s, result.body);

	result = get("/either"s, basic, aliceSecret);
	assertTrue (result.status == HTTPResponse::HTTP_OK);

	result = get("/protected"s, bearer, "alice-token"s);
	assertTrue (result.status == HTTPResponse::HTTP_OK);
	assertEqual ("alice|api"s, result.body);

	assertTrue (get("/protected"s, basic, aliceWrong).status == HTTPResponse::HTTP_UNAUTHORIZED);
	assertTrue (get("/protected"s, bearer, "mallory-token"s).status == HTTPResponse::HTTP_UNAUTHORIZED);
	assertTrue (get("/protected"s, basic, "!!!"s).status == HTTPResponse::HTTP_UNAUTHORIZED);
	assertTrue (get("/admin"s, basic, aliceSecret).status == HTTPResponse::HTTP_FORBIDDEN);
	assertTrue (get("/admin"s, bearer, "alice-token"s).status == HTTPResponse::HTTP_FORBIDDEN);
}


void WebServerDispatcherTest::testBasicCredentialsAreAuthorizedUnderAccountName()
{
	_pFixture->bundles.registry().registerService(AUTH_SERVICE_NAME, new AccountNameAuthService, Properties());
	WebServerDispatcher::Ptr pDispatcher =
		new WebServerDispatcher(_pFixture->dispatcherConfig(WebServerDispatcher::CONF_OPT_ADD_AUTH_HEADER));
	pDispatcher->addVirtualPath(handlerPath("/protected"s, "read"s, _pFixture->bundles.bundle()));
	TestServer server(*pDispatcher);

	HTTPRequest request(HTTPRequest::HTTP_GET, "/protected"s);
	Poco::Net::HTTPBasicCredentials("ALICE@example.com"s, "secret"s).authenticate(request);
	HTTPResponse response;
	const std::string body = server.send(request, response);
	assertTrue (response.getStatus() == HTTPResponse::HTTP_OK);
	assertEqual ("alice|-"s, body);
}


void WebServerDispatcherTest::testSessionCSRFProtection()
{
	_pFixture->bundles.registry().registerService(AUTH_SERVICE_NAME, new TestAuthService, Properties());
	WebSessionManager::Ptr pSessionManager = new WebSessionManager(_pFixture->bundles.context());
	_pFixture->bundles.registry().registerService(WebSessionManager::SERVICE_NAME, pSessionManager, Properties());
	WebServerDispatcher::Ptr pDispatcher = new WebServerDispatcher(_pFixture->dispatcherConfig());
	pDispatcher->addVirtualPath(WebServerDispatcher::VirtualPath("/login"s, new LoginHandlerFactory(pSessionManager, _pFixture->bundles.context()), WebServerDispatcher::PathSecurity(), _pFixture->bundles.bundle()));
	WebServerDispatcher::VirtualPath api(handlerPath("/api"s, "read"s, _pFixture->bundles.bundle(), WebServerDispatcher::AUTH_SESSION));
	api.security.session = "app";
	api.security.csrfProtection = true;
	api.security.csrfTokenHeader = "X-XSRF-TOKEN";
	pDispatcher->addVirtualPath(api);
	TestServer server(*pDispatcher);

	const auto status = [&server](const Session& session, const std::string& method, const std::string* pToken)
	{
		HTTPRequest request(method, "/api"s);
		request.setCookies(session.cookies);
		if (pToken != nullptr) request.set("X-XSRF-TOKEN"s, *pToken);
		HTTPResponse response;
		server.send(request, response);
		return response.getStatus();
	};

	const Session session = loginSession(server, "/login"s);
	assertFalse (session.csrfToken.empty());
	const std::string wrongToken("wrong");
	assertTrue (status(session, HTTPRequest::HTTP_POST, nullptr) == HTTPResponse::HTTP_UNAUTHORIZED);
	assertTrue (status(session, HTTPRequest::HTTP_POST, &wrongToken) == HTTPResponse::HTTP_UNAUTHORIZED);
	assertTrue (status(session, HTTPRequest::HTTP_POST, &session.csrfToken) == HTTPResponse::HTTP_OK);
	assertTrue (status(session, HTTPRequest::HTTP_GET, nullptr) == HTTPResponse::HTTP_OK);

	const Session sessionWithoutToken = loginSession(server, "/login?withoutToken"s);
	assertTrue (sessionWithoutToken.csrfToken.empty());
	assertTrue (status(sessionWithoutToken, HTTPRequest::HTTP_POST, &sessionWithoutToken.csrfToken) == HTTPResponse::HTTP_UNAUTHORIZED);
}


void WebServerDispatcherTest::testSessionUserWithoutPermissionIsForbidden()
{
	_pFixture->bundles.registry().registerService(AUTH_SERVICE_NAME, new TestAuthService, Properties());
	WebSessionManager::Ptr pSessionManager = new WebSessionManager(_pFixture->bundles.context());
	_pFixture->bundles.registry().registerService(WebSessionManager::SERVICE_NAME, pSessionManager, Properties());
	WebServerDispatcher::Ptr pDispatcher = _pFixture->createDispatcher();
	pDispatcher->addVirtualPath(WebServerDispatcher::VirtualPath("/login"s, new LoginHandlerFactory(pSessionManager, _pFixture->bundles.context()), WebServerDispatcher::PathSecurity(), _pFixture->bundles.bundle()));
	WebServerDispatcher::VirtualPath api(handlerPath("/api"s, "admin"s, _pFixture->bundles.bundle(), WebServerDispatcher::AUTH_SESSION));
	api.security.session = "app";
	pDispatcher->addVirtualPath(api);
	WebServerDispatcher::VirtualPath directory("/admin"s, "www"s, WebServerDispatcher::PathSecurity(WebServerDispatcher::SM_OWNER, ""s, "admin"s), _pFixture->bundles.bundle());
	directory.security.authMethods = WebServerDispatcher::AUTH_SESSION;
	directory.security.session = "app";
	WebServerDispatcher::VirtualPath redirectingDirectory(directory);
	redirectingDirectory.path = "/redirecting"s;
	redirectingDirectory.redirectOn401 = "/login"s;
	pDispatcher->addVirtualPath(directory);
	pDispatcher->addVirtualPath(redirectingDirectory);
	TestServer server(*pDispatcher);
	const Session session = loginSession(server, "/login"s);
	const auto get = [&server, &session](const std::string& uri, HTTPResponse& response)
	{
		HTTPRequest request(HTTPRequest::HTTP_GET, uri);
		request.setCookies(session.cookies);
		server.send(request, response);
	};

	HTTPResponse apiResponse;
	get("/api"s, apiResponse);
	assertTrue (apiResponse.getStatus() == HTTPResponse::HTTP_FORBIDDEN);

	HTTPResponse directoryResponse;
	get("/admin/hello.txt"s, directoryResponse);
	assertTrue (directoryResponse.getStatus() == HTTPResponse::HTTP_FORBIDDEN);

	HTTPResponse redirectResponse;
	get("/redirecting/hello.txt"s, redirectResponse);
	assertTrue (redirectResponse.getStatus() == HTTPResponse::HTTP_FOUND);
	assertEqual ("/"s, redirectResponse.get("Location"s));
}


void WebServerDispatcherTest::testSessionUserWithoutPermissionForHomePageIsNotAuthorized()
{
	_pFixture->bundles.registry().registerService(AUTH_SERVICE_NAME, new TestAuthService, Properties());
	WebSessionManager::Ptr pSessionManager = new WebSessionManager(_pFixture->bundles.context());
	_pFixture->bundles.registry().registerService(WebSessionManager::SERVICE_NAME, pSessionManager, Properties());
	WebServerDispatcher::Ptr pDispatcher = new WebServerDispatcher(_pFixture->dispatcherConfig());
	WebServerDispatcher::VirtualPath home("/"s, "www"s,
		WebServerDispatcher::PathSecurity(WebServerDispatcher::SM_OWNER, ""s, "admin"s), _pFixture->bundles.bundle());
	home.security.authMethods = WebServerDispatcher::AUTH_SESSION;
	home.security.session = "app";
	home.redirectOn401 = "/login"s;
	pDispatcher->addVirtualPath(home);
	pDispatcher->addVirtualPath(WebServerDispatcher::VirtualPath("/login"s,
		new LoginHandlerFactory(pSessionManager, _pFixture->bundles.context()), WebServerDispatcher::PathSecurity(),
		_pFixture->bundles.bundle()));
	TestServer server(*pDispatcher);
	const Session session = loginSession(server, "/login"s);

	HTTPRequest request(HTTPRequest::HTTP_GET, "/"s);
	request.setCookies(session.cookies);
	HTTPResponse response;
	const std::string body = server.send(request, response);
	assertTrue (response.getStatus() == HTTPResponse::HTTP_FORBIDDEN);
	assertTrue (body.find("You are not authorized to access / on this server."s) != std::string::npos);
}


void WebServerDispatcherTest::testDisabledUserIsNotAuthenticated()
{
	Poco::AutoPtr<DisablingAuthService> pAuthService = new DisablingAuthService;
	_pFixture->bundles.registry().registerService(AUTH_SERVICE_NAME, pAuthService, Properties());
	_pFixture->bundles.registry().registerService(TOKEN_VALIDATOR_NAME, new TestTokenValidator, Properties());
	WebSessionManager::Ptr pSessionManager = new WebSessionManager(_pFixture->bundles.context());
	_pFixture->bundles.registry().registerService(WebSessionManager::SERVICE_NAME, pSessionManager, Properties());
	WebServerDispatcher::Ptr pDispatcher = new WebServerDispatcher(_pFixture->dispatcherConfig());
	pDispatcher->addVirtualPath(WebServerDispatcher::VirtualPath("/login"s,
		new LoginHandlerFactory(pSessionManager, _pFixture->bundles.context()), WebServerDispatcher::PathSecurity(),
		_pFixture->bundles.bundle()));
	WebServerDispatcher::VirtualPath api(handlerPath("/api"s, "*"s, _pFixture->bundles.bundle(),
		WebServerDispatcher::AUTH_SESSION | WebServerDispatcher::AUTH_BEARER));
	api.security.session = "app";
	pDispatcher->addVirtualPath(api);
	TestServer server(*pDispatcher);
	const Session session = loginSession(server, "/login"s);
	const auto sessionStatus = [&server, &session]()
	{
		HTTPRequest request(HTTPRequest::HTTP_GET, "/api"s);
		request.setCookies(session.cookies);
		HTTPResponse response;
		server.send(request, response);
		return response.getStatus();
	};
	const auto bearerStatus = [&server]()
	{
		HTTPRequest request(HTTPRequest::HTTP_GET, "/api"s);
		request.setCredentials("Bearer"s, "alice-token"s);
		HTTPResponse response;
		server.send(request, response);
		return response.getStatus();
	};

	assertTrue (sessionStatus() == HTTPResponse::HTTP_OK);
	assertTrue (bearerStatus() == HTTPResponse::HTTP_OK);
	pAuthService->disableAlice();
	assertTrue (sessionStatus() == HTTPResponse::HTTP_UNAUTHORIZED);
	assertTrue (bearerStatus() == HTTPResponse::HTTP_UNAUTHORIZED);
}


void WebServerDispatcherTest::testUnregisteredSessionManagerIsNotUsed()
{
	_pFixture->bundles.registry().registerService(AUTH_SERVICE_NAME, new TestAuthService, Properties());
	WebSessionManager::Ptr pSessionManager = new WebSessionManager(_pFixture->bundles.context());
	ServiceRef::Ptr pSessionManagerRef = _pFixture->bundles.registry().registerService(WebSessionManager::SERVICE_NAME, pSessionManager, Properties());
	WebServerDispatcher::Ptr pDispatcher = new WebServerDispatcher(_pFixture->dispatcherConfig());
	pDispatcher->addVirtualPath(WebServerDispatcher::VirtualPath("/login"s, new LoginHandlerFactory(pSessionManager, _pFixture->bundles.context()), WebServerDispatcher::PathSecurity(), _pFixture->bundles.bundle()));
	WebServerDispatcher::VirtualPath api(handlerPath("/api"s, "read"s, _pFixture->bundles.bundle(), WebServerDispatcher::AUTH_SESSION));
	api.security.session = "app";
	pDispatcher->addVirtualPath(api);
	TestServer server(*pDispatcher);
	const Session session = loginSession(server, "/login"s);
	const auto status = [&server, &session]()
	{
		HTTPRequest request(HTTPRequest::HTTP_GET, "/api"s);
		request.setCookies(session.cookies);
		HTTPResponse response;
		server.send(request, response);
		return response.getStatus();
	};

	assertTrue (status() == HTTPResponse::HTTP_OK);
	_pFixture->bundles.registry().unregisterService(pSessionManagerRef);
	assertTrue (status() == HTTPResponse::HTTP_UNAUTHORIZED);
}


void WebServerDispatcherTest::testSecondFilterForMediaTypeIsRejected()
{
	WebServerDispatcher::Ptr pDispatcher = _pFixture->createDispatcher();
	pDispatcher->addFilter("text/plain"s, new NamedFilterFactory("first"s), WebFilter::Args(),
		_pFixture->bundles.bundle());
	try
	{
		pDispatcher->addFilter("text/plain"s, new NamedFilterFactory("second"s), WebFilter::Args(),
			_pFixture->bundles.bundle());
		fail("a second filter for a media type must be rejected");
	}
	catch (Poco::ExistsException&)
	{
	}
	TestServer server(*pDispatcher);

	HTTPRequest request(HTTPRequest::HTTP_GET, "/dir/hello.txt"s);
	HTTPResponse response;
	assertEqual ("first"s, server.send(request, response));
}


void WebServerDispatcherTest::testLogMessagesHoldNoTokenAndNoControlCharacters()
{
	_pFixture->bundles.registry().registerService(AUTH_SERVICE_NAME, new TestAuthService, Properties());
	_pFixture->bundles.registry().registerService(TOKEN_VALIDATOR_NAME, new TestTokenValidator, Properties());
	WebServerDispatcher::Ptr pDispatcher = new WebServerDispatcher(_pFixture->dispatcherConfig());
	pDispatcher->addVirtualPath(handlerPath("/protected"s, "read"s, _pFixture->bundles.bundle()));
	TestServer server(*pDispatcher);
	LogCapture capture(_pFixture->bundles.context()->logger().name());

	HTTPRequest bearer(HTTPRequest::HTTP_GET, "/protected"s);
	bearer.setCredentials("Bearer"s, "rejected-token-value"s);
	HTTPResponse response;
	server.send(bearer, response);
	assertTrue (response.getStatus() == HTTPResponse::HTTP_UNAUTHORIZED);

	HTTPRequest basic(HTTPRequest::HTTP_GET, "/protected"s);
	basic.setCredentials(Poco::Net::HTTPBasicCredentials::SCHEME, "bWFsCmxvcnk6cHc="s); // "mal\nlory:pw"
	server.send(basic, response);
	assertTrue (response.getStatus() == HTTPResponse::HTTP_UNAUTHORIZED);

	const std::vector<Poco::Message> messages = capture.channel().waitForMessages(2);
	assertEqual (2, messages.size());
	for (const auto& message: messages)
	{
		assertTrue (message.getText().find("rejected-token-value"s) == std::string::npos);
		assertTrue (message.getText().find('\n') == std::string::npos);
	}
	assertTrue (messages[1].getText().find("mal\\nlory"s) != std::string::npos);
}


void WebServerDispatcherTest::testLoggedRequestDataHasNoControlCharacters()
{
	_pFixture->bundles.registry().registerService(AUTH_SERVICE_NAME, new TestAuthService, Properties());
	_pFixture->bundles.registry().registerService(TOKEN_VALIDATOR_NAME, new TestTokenValidator, Properties());
	// Not sent over a connection, as HTTPRequest::write() and read() can reject control characters.
	ServerRequestStub request("127.0.0.1:50000"s, false);
	request.setMethod("G\x1b[2JT"s);
	request.setURI("/protected?\x1b[2J\x1b[31m"s);
	request.setVersion("H\x1b[2J"s);
	request.setCredentials("Bearer"s, "rejected-token-value"s);
	request.set("Host"s, "\x1b[2Jhost"s);
	request.set("Referer"s, "\x1b[2Jreferer"s);
	request.set("User-Agent"s, "\x1b[2Jagent"s);
	// Poco::URI can reject control characters in the query, and the request is then answered with 400.
	bool parsesURI = true;
	try
	{
		const Poco::URI uri(request.getURI());
	}
	catch (Poco::SyntaxException&)
	{
		parsesURI = false;
		warnmsg("Poco::URI rejects control characters, token warning case skipped.");
	}
	WebServerDispatcher::Ptr pDispatcher = new WebServerDispatcher(
		_pFixture->dispatcherConfig(WebServerDispatcher::CONF_OPT_LOG_FULL_REQUEST));
	pDispatcher->addVirtualPath(handlerPath("/protected"s, "read"s, _pFixture->bundles.bundle()));
	LogCapture bundleLog(_pFixture->bundles.context()->logger().name());
	LogCapture accessLog("osp.web.access"s);

	pDispatcher->handleRequest(request, request.response(), false);
	assertTrue (request.response().getStatus() ==
		(parsesURI ? HTTPResponse::HTTP_UNAUTHORIZED : HTTPResponse::HTTP_BAD_REQUEST));

	// The full request log adds the request before and the response after the token warning.
	const std::size_t messageCount = parsesURI ? 3 : 2;
	const std::vector<Poco::Message> messages = bundleLog.channel().waitForMessages(messageCount);
	assertEqual (messageCount, messages.size());
	if (parsesURI)
	{
		const std::string& tokenWarning = messages.at(1).getText();
		assertTrue (tokenWarning.find("/protected?%1B[2J%1B[31m"s) != std::string::npos);
	}
	for (const auto& message: messages)
	{
		assertFalse (hasControlCharacter(message.getText()));
	}
	const std::vector<Poco::Message> accessMessages = accessLog.channel().waitForMessages(1);
	assertEqual (1, accessMessages.size());
	assertFalse (hasControlCharacter(accessMessages[0].getText()));
	for (const auto& property: {"host"s, "referer"s, "useragent"s})
	{
		assertFalse (hasControlCharacter(accessMessages[0].get(property)));
	}
}


void WebServerDispatcherTest::testFullRequestLogEscapesMethodAndFieldValue()
{
	WebServerDispatcher::Ptr pDispatcher = new WebServerDispatcher(
		_pFixture->dispatcherConfig(WebServerDispatcher::CONF_OPT_LOG_FULL_REQUEST));
	pDispatcher->addVirtualPath(handlerPath("/open"s, ""s, _pFixture->bundles.bundle()));
	LogCapture capture(_pFixture->bundles.context()->logger().name());

	// HTTPRequest::read() accepts a control character in the method and in a field value.
	// The log does not rely on a field value without CR LF.
	const std::vector<std::pair<std::string, std::string>> methodsAndAgents{
		{"GET"s, "agent"s}, {"G\x1b[2JET"s, "agent"s}, {"GET"s, "\x1b[2Jagent"s}, {"GET"s, "a\r\nInjected: b"s}};
	for (const auto& [method, userAgent]: methodsAndAgents)
	{
		ServerRequestStub request("127.0.0.1:50000"s, false);
		request.setMethod(method);
		request.setURI("/open"s);
		request.set("User-Agent"s, userAgent);
		pDispatcher->handleRequest(request, request.response(), false);
		assertTrue (request.response().getStatus() == HTTPResponse::HTTP_OK);
	}

	// A request message and a response message per request.
	const std::vector<Poco::Message> messages = capture.channel().waitForMessages(8);
	assertEqual (8, messages.size());
	for (std::size_t i = 0; i < messages.size(); i += 2)
	{
		assertTrue (messages.at(i).getText().rfind("Request from "s, 0) == 0);
	}
	assertTrue (messages.at(2).getText().find("\n\tG%1B[2JET /open HTTP/1.0\n\t"s) != std::string::npos);
	assertTrue (messages.at(4).getText().find("\n\tUser-Agent: %1B[2Jagent"s) != std::string::npos);
	assertTrue (messages.at(6).getText().find("\n\tUser-Agent: a%0D%0AInjected: b"s) != std::string::npos);
	for (const auto& message: messages)
	{
		assertFalse (hasControlCharacter(message.getText()));
	}
}


void WebServerDispatcherTest::testFullResponseLogEscapesFieldValue()
{
	WebServerDispatcher::Ptr pDispatcher = new WebServerDispatcher(_pFixture->dispatcherConfig(
		WebServerDispatcher::CONF_OPT_LOG_FULL_REQUEST | WebServerDispatcher::CONF_OPT_ENABLE_CORS));
	WebServerDispatcher::VirtualPath vPath(handlerPath("/reflect"s, ""s, _pFixture->bundles.bundle()));
	vPath.cors.enable = true;
	vPath.cors.allowOrigin = "**"s;
	pDispatcher->addVirtualPath(vPath);
	LogCapture bundleLog(_pFixture->bundles.context()->logger().name());
	LogCapture accessLog("osp.web.access"s);

	// HTTPRequest::read() accepts a control character in a field value, and the origin is copied into the response.
	ServerRequestStub request("127.0.0.1:50000"s, false);
	request.setURI("/reflect"s);
	request.set("Origin"s, "https://\x1b[2Ja.example"s);
	pDispatcher->handleRequest(request, request.response(), false);
	assertTrue (request.response().getStatus() == HTTPResponse::HTTP_OK);

	const std::vector<Poco::Message> messages = bundleLog.channel().waitForMessages(2);
	assertEqual (2, messages.size());
	const std::string& responseText = messages.at(1).getText();
	assertTrue (responseText.rfind("Response to "s, 0) == 0);
	assertTrue (responseText.find("\n\tAccess-Control-Allow-Origin: https://%1B[2Ja.example"s) != std::string::npos);
	for (const auto& message: messages)
	{
		assertFalse (hasControlCharacter(message.getText()));
	}
	assertEqual (1, accessLog.channel().waitForMessages(1).size());
}


void WebServerDispatcherTest::testFullResponseLogEscapesReasonAndFieldOfHandler()
{
	WebServerDispatcher::Ptr pDispatcher = new WebServerDispatcher(
		_pFixture->dispatcherConfig(WebServerDispatcher::CONF_OPT_LOG_FULL_REQUEST));
	pDispatcher->addVirtualPath(WebServerDispatcher::VirtualPath("/note"s,
		new TestHandlerFactory<ControlCharacterResponseHandler>, WebServerDispatcher::PathSecurity(),
		_pFixture->bundles.bundle()));
	LogCapture capture(_pFixture->bundles.context()->logger().name());

	ServerRequestStub request("127.0.0.1:50000"s, false);
	request.setURI("/note"s);
	pDispatcher->handleRequest(request, request.response(), false);
	assertTrue (request.response().getStatus() == HTTPResponse::HTTP_OK);

	const std::vector<Poco::Message> messages = capture.channel().waitForMessages(2);
	assertEqual (2, messages.size());
	const std::string& responseText = messages.at(1).getText();
	assertTrue (responseText.rfind("Response to "s, 0) == 0);
	assertTrue (responseText.find(" 200 OK%1B[2J\n\t"s) != std::string::npos);
	assertTrue (responseText.find("\n\tX-Note: a%0D%0AInjected: b"s) != std::string::npos);
	for (const auto& message: messages)
	{
		assertFalse (hasControlCharacter(message.getText()));
	}
}


void WebServerDispatcherTest::testBadRequestDebugMessageHasNoControlCharacters()
{
	WebServerDispatcher::Ptr pDispatcher = _pFixture->createDispatcher();
	LogCapture capture(_pFixture->bundles.context()->logger().name());
	_pFixture->bundles.context()->logger().setLevel(Poco::Message::PRIO_DEBUG);

	// Poco::URI rejects the incomplete percent encoding with an exception text that holds the path.
	ServerRequestStub request("127.0.0.1:50000"s, false);
	request.setURI("/dir/\x1b[2J%"s);
	pDispatcher->handleRequest(request, request.response(), false);
	assertTrue (request.response().getStatus() == HTTPResponse::HTTP_BAD_REQUEST);

	const std::vector<Poco::Message> messages = capture.channel().waitForMessages(1);
	const auto badRequest = std::find_if(messages.begin(), messages.end(), [](const Poco::Message& message)
		{ return message.getText().find("answered with 400 Bad Request: "s) != std::string::npos; });
	assertTrue (badRequest != messages.end());
	const std::string& text = badRequest->getText();
	assertTrue (text.find("/dir/%1B[2J%"s, text.find("400 Bad Request"s)) != std::string::npos);
	assertFalse (hasControlCharacter(text));
}


void WebServerDispatcherTest::testFullRequestLogHasNoCredentials()
{
	WebSessionManager::Ptr pSessionManager = new WebSessionManager(_pFixture->bundles.context());
	_pFixture->bundles.registry().registerService(WebSessionManager::SERVICE_NAME, pSessionManager, Properties());
	WebServerDispatcher::Ptr pDispatcher = new WebServerDispatcher(_pFixture->dispatcherConfig(WebServerDispatcher::CONF_OPT_LOG_FULL_REQUEST));
	pDispatcher->addVirtualPath(WebServerDispatcher::VirtualPath("/login"s, new LoginHandlerFactory(pSessionManager, _pFixture->bundles.context()), WebServerDispatcher::PathSecurity(), _pFixture->bundles.bundle()));
	WebServerDispatcher::VirtualPath api(handlerPath("/api"s, "read"s, _pFixture->bundles.bundle(), WebServerDispatcher::AUTH_SESSION));
	api.security.session = "app";
	api.security.csrfProtection = true;
	api.security.csrfTokenHeader = "X-App-CSRF";
	pDispatcher->addVirtualPath(api);
	TestServer server(*pDispatcher);
	LogCapture capture(_pFixture->bundles.context()->logger().name());

	HTTPRequest request(HTTPRequest::HTTP_GET, "/login"s);
	request.set("Authorization"s, "Basic YWxpY2U6c2VjcmV0"s);
	request.set("x-app-csrf"s, "request-csrf-token"s);
	request.set("Proxy-Authorization"s, "Basic cHJveHk6c2VjcmV0"s);
	request.set("Cookie"s, "osp.web.session.app=request-session-id"s);
	HTTPResponse response;
	server.send(request, response);
	std::vector<Poco::Net::HTTPCookie> cookies;
	response.getCookies(cookies);
	assertFalse (cookies.empty());

	const std::vector<Poco::Message> messages = capture.channel().waitForMessages(2);
	assertEqual (2, messages.size());
	for (const auto& message: messages)
	{
		const std::string& text = message.getText();
		assertTrue (text.find("YWxpY2U6c2VjcmV0"s) == std::string::npos);
		assertTrue (text.find("cHJveHk6c2VjcmV0"s) == std::string::npos);
		assertTrue (text.find("request-session-id"s) == std::string::npos);
		assertTrue (text.find("request-csrf-token"s) == std::string::npos);
		assertTrue (text.find(cookies[0].getValue()) == std::string::npos);
	}
	assertTrue (messages[0].getText().find("Authorization: <redacted>"s) != std::string::npos);
	assertTrue (messages[0].getText().find("Cookie: <redacted>"s) != std::string::npos);
	assertTrue (messages[1].getText().find("Set-Cookie: <redacted>"s) != std::string::npos);
}


void WebServerDispatcherTest::testFullRequestLogRedactsFieldsWithCredentialOrNonTokenNames()
{
	WebServerDispatcher::Ptr pDispatcher = new WebServerDispatcher(
		_pFixture->dispatcherConfig(WebServerDispatcher::CONF_OPT_LOG_FULL_REQUEST));
	pDispatcher->addVirtualPath(WebServerDispatcher::VirtualPath("/fields"s,
		new TestHandlerFactory<CredentialAndNonTokenFieldResponseHandler>, WebServerDispatcher::PathSecurity(),
		_pFixture->bundles.bundle()));
	LogCapture capture(_pFixture->bundles.context()->logger().name());

	// HTTPRequest::read() ends a field name at the first colon or line end.
	ServerRequestStub request("127.0.0.1:50000"s, false);
	request.setURI("/fields"s);
	request.add(" Cookie "s, "request-padded"s);
	request.add("Cookie"s, "request-cookie"s);
	request.add("cookie"s, "request-lower"s);
	request.add("X-Note"s, "request-note"s);
	pDispatcher->handleRequest(request, request.response(), false);
	assertTrue (request.response().getStatus() == HTTPResponse::HTTP_OK);

	const std::vector<Poco::Message> messages = capture.channel().waitForMessages(2);
	assertEqual (2, messages.size());
	const std::string& requestText = messages.at(0).getText();
	assertTrue (requestText.find("\n\t Cookie : <redacted>"s) != std::string::npos);
	assertTrue (requestText.find("\n\tCookie: <redacted>"s) != std::string::npos);
	assertTrue (requestText.find("\n\tcookie: <redacted>"s) != std::string::npos);
	assertTrue (requestText.find("\n\tX-Note: request-note"s) != std::string::npos);
	const std::string& responseText = messages.at(1).getText();
	assertTrue (responseText.find("\n\tSet-Cookie:sid: <redacted>"s) != std::string::npos);
	assertTrue (responseText.find("\n\tX%0D%0ASet-Cookie: <redacted>"s) != std::string::npos);
	assertTrue (responseText.find("\n\t Cookie : <redacted>"s) != std::string::npos);
	assertTrue (responseText.find("\n\tCookie: <redacted>"s) != std::string::npos);
	assertTrue (responseText.find("\n\tcookie: <redacted>"s) != std::string::npos);
	assertTrue (responseText.find("\n\tX-Note: response-note"s) != std::string::npos);
	for (const auto& value: {"request-padded"s, "request-cookie"s, "request-lower"s, "response-colon"s,
		"response-crlf"s, "response-padded"s, "response-cookie"s, "response-lower"s})
	{
		assertTrue (requestText.find(value) == std::string::npos);
		assertTrue (responseText.find(value) == std::string::npos);
	}
}


void WebServerDispatcherTest::testFullRequestLogKeepsRequestLineWithFieldNameAsMethod()
{
	WebServerDispatcher::Ptr pDispatcher = new WebServerDispatcher(
		_pFixture->dispatcherConfig(WebServerDispatcher::CONF_OPT_LOG_FULL_REQUEST));
	pDispatcher->addVirtualPath(handlerPath("/open"s, ""s, _pFixture->bundles.bundle()));
	LogCapture capture(_pFixture->bundles.context()->logger().name());

	ServerRequestStub request("127.0.0.1:50000"s, false);
	request.setMethod("Cookie:"s);
	request.setURI("/open?visible"s);
	request.set("Cookie"s, "osp.web.session.app=request-session-id"s);
	pDispatcher->handleRequest(request, request.response(), false);

	const std::vector<Poco::Message> messages = capture.channel().waitForMessages(2);
	assertEqual (2, messages.size());
	const std::string& requestText = messages.at(0).getText();
	assertTrue (requestText.rfind("Request from "s, 0) == 0);
	assertTrue (requestText.find("\n\tCookie: /open?visible HTTP/1.0\n\t"s) != std::string::npos);
	assertTrue (requestText.find("\n\tCookie: <redacted>"s) != std::string::npos);
	assertTrue (requestText.find("request-session-id"s) == std::string::npos);
}


void WebServerDispatcherTest::testForwardedClientAddressIsAnIPAddressOnly()
{
	WebServerDispatcher::Ptr pDispatcher = _pFixture->createDispatcher();
	TestServer server(*pDispatcher);
	LogCapture accessLog("osp.web.access"s);
	LogCapture bundleLog(_pFixture->bundles.context()->logger().name());

	HTTPRequest proxied(HTTPRequest::HTTP_GET, "/dir/hello.txt"s);
	proxied.set("X-Forwarded-For"s, "10.1.2.3, 10.0.0.1"s);
	HTTPResponse response;
	server.send(proxied, response);
	std::vector<Poco::Message> messages = accessLog.channel().waitForMessages(1);
	assertEqual (1, messages.size());
	assertEqual ("10.1.2.3"s, messages[0].get("ffclient"s));

	HTTPRequest named(HTTPRequest::HTTP_GET, "/dir/hello.txt"s);
	named.set("X-Forwarded-For"s, "proxy.invalid:8080"s);
	server.send(named, response);
	messages = accessLog.channel().waitForMessages(2);
	assertEqual (2, messages.size());
	assertEqual ("127.0.0.1"s, messages[1].get("ffclient"s));
	for (const auto& message: bundleLog.channel().waitForMessages(0))
	{
		assertTrue (message.getText().find("proxy.invalid"s) == std::string::npos);
	}
}


void WebServerDispatcherTest::testJSONErrorResponseEscapesRequestData()
{
	WebServerDispatcher::Ptr pDispatcher = _pFixture->createDispatcher();
	TestServer server(*pDispatcher);

	const std::string uri("/missing\\\"");
	HTTPRequest request(HTTPRequest::HTTP_GET, uri);
	request.set("Accept"s, "application/json"s);
	HTTPResponse response;
	const std::string body = server.send(request, response);
	assertTrue (response.getStatus() == HTTPResponse::HTTP_NOT_FOUND);
	Poco::JSON::Object::Ptr pError = Poco::JSON::Parser().parse(body).extract<Poco::JSON::Object::Ptr>();
	assertEqual ("The requested URL " + uri + " was not found on this server."s, pError->getValue<std::string>("detail"s));
}


void WebServerDispatcherTest::testCORSOrigins()
{
	WebServerDispatcher::Ptr pDispatcher = new WebServerDispatcher(_pFixture->dispatcherConfig(WebServerDispatcher::CONF_OPT_ENABLE_CORS));
	for (const auto& [path, allowOrigin]: std::map<std::string, std::string>{{"/none"s, ""s}, {"/reflect"s, "**"s}, {"/any"s, "*"s}, {"/list"s, "https://a.example, https://b.example"s}})
	{
		WebServerDispatcher::VirtualPath vPath(handlerPath(path, ""s, _pFixture->bundles.bundle()));
		vPath.cors.enable = true;
		vPath.cors.allowOrigin = allowOrigin;
		pDispatcher->addVirtualPath(vPath);
	}
	TestServer server(*pDispatcher);
	const auto send = [&server](const std::string& method, const std::string& uri, const std::string& origin, HTTPResponse& response)
	{
		HTTPRequest request(method, uri);
		request.set("Origin"s, origin);
		return server.send(request, response);
	};

	HTTPResponse none;
	send(HTTPRequest::HTTP_GET, "/none"s, "https://a.example"s, none);
	assertTrue (none.getStatus() == HTTPResponse::HTTP_OK);
	assertFalse (none.has("Access-Control-Allow-Origin"s));
	HTTPResponse nonePreflight;
	send(HTTPRequest::HTTP_OPTIONS, "/none"s, "https://a.example"s, nonePreflight);
	assertTrue (nonePreflight.getStatus() == HTTPResponse::HTTP_FORBIDDEN);
	assertFalse (nonePreflight.has("Access-Control-Allow-Origin"s));

	HTTPResponse reflect;
	send(HTTPRequest::HTTP_GET, "/reflect"s, "https://c.example"s, reflect);
	assertEqual ("https://c.example"s, reflect.get("Access-Control-Allow-Origin"s, ""s));
	assertEqual ("true"s, reflect.get("Access-Control-Allow-Credentials"s, ""s));
	assertEqual ("Origin"s, reflect.get("Vary"s, ""s));
	HTTPResponse reflectNull;
	send(HTTPRequest::HTTP_GET, "/reflect"s, "null"s, reflectNull);
	assertEqual ("null"s, reflectNull.get("Access-Control-Allow-Origin"s, ""s));

	HTTPResponse any;
	send(HTTPRequest::HTTP_GET, "/any"s, "https://c.example"s, any);
	assertEqual ("*"s, any.get("Access-Control-Allow-Origin"s, ""s));
	assertFalse (any.has("Access-Control-Allow-Credentials"s));

	HTTPResponse listed;
	send(HTTPRequest::HTTP_GET, "/list"s, "https://b.example"s, listed);
	assertEqual ("https://b.example"s, listed.get("Access-Control-Allow-Origin"s, ""s));
	assertEqual ("Origin"s, listed.get("Vary"s, ""s));
	HTTPResponse listedPreflight;
	send(HTTPRequest::HTTP_OPTIONS, "/list"s, "https://b.example"s, listedPreflight);
	assertTrue (listedPreflight.getStatus() == HTTPResponse::HTTP_OK);
	assertEqual ("*"s, listedPreflight.get("Access-Control-Allow-Methods"s, ""s));

	HTTPResponse unlisted;
	send(HTTPRequest::HTTP_GET, "/list"s, "https://c.example"s, unlisted);
	assertTrue (unlisted.getStatus() == HTTPResponse::HTTP_OK);
	assertFalse (unlisted.has("Access-Control-Allow-Origin"s));
	HTTPResponse unlistedPreflight;
	send(HTTPRequest::HTTP_OPTIONS, "/list"s, "https://c.example"s, unlistedPreflight);
	assertTrue (unlistedPreflight.getStatus() == HTTPResponse::HTTP_FORBIDDEN);

	HTTPResponse unlistedPost;
	assertEqual (""s, send(HTTPRequest::HTTP_POST, "/list"s, "https://c.example"s, unlistedPost));
	assertTrue (unlistedPost.getStatus() == HTTPResponse::HTTP_FORBIDDEN);
	assertFalse (unlistedPost.has("Access-Control-Allow-Origin"s));

	HTTPResponse listedPost;
	assertEqual ("-|-"s, send(HTTPRequest::HTTP_POST, "/list"s, "https://b.example"s, listedPost));
	assertEqual ("https://b.example"s, listedPost.get("Access-Control-Allow-Origin"s, ""s));

	for (const auto& sameOrigin: {"http://127.0.0.1:"s + std::to_string(server.port()), "HTTP://127.0.0.1:"s + std::to_string(server.port())})
	{
		HTTPResponse sameOriginPost;
		assertEqual ("-|-"s, send(HTTPRequest::HTTP_POST, "/none"s, sameOrigin, sameOriginPost));
		assertTrue (sameOriginPost.getStatus() == HTTPResponse::HTTP_OK);
	}

	for (const auto& [host, origin]: std::map<std::string, std::string>{{"web.example:80"s, "http://web.example"s}, {"Web.Example"s, "http://web.example:80"s}})
	{
		HTTPRequest request(HTTPRequest::HTTP_POST, "/none"s);
		request.set("Host"s, host);
		request.set("Origin"s, origin);
		HTTPResponse response;
		assertEqual ("-|-"s, server.send(request, response));
	}

	HTTPResponse otherPortPost;
	send(HTTPRequest::HTTP_POST, "/none"s, "http://127.0.0.1:1"s, otherPortPost);
	assertTrue (otherPortPost.getStatus() == HTTPResponse::HTTP_FORBIDDEN);

	HTTPResponse otherSchemePost;
	assertEqual (""s, send(HTTPRequest::HTTP_POST, "/none"s, "https://127.0.0.1:"s + std::to_string(server.port()), otherSchemePost));
	assertTrue (otherSchemePost.getStatus() == HTTPResponse::HTTP_FORBIDDEN);
}


void WebServerDispatcherTest::testOriginDependentResponsesVaryByOrigin()
{
	WebServerDispatcher::Ptr pDispatcher = new WebServerDispatcher(
		_pFixture->dispatcherConfig(WebServerDispatcher::CONF_OPT_ENABLE_CORS));
	for (const auto& [path, allowOrigin]: std::map<std::string, std::string>{{"/none"s, ""s}, {"/reflect"s, "**"s},
		{"/any"s, "*"s}, {"/list"s, "https://a.example"s}})
	{
		WebServerDispatcher::VirtualPath vPath(handlerPath(path, ""s, _pFixture->bundles.bundle()));
		vPath.cors.enable = true;
		vPath.cors.allowOrigin = allowOrigin;
		pDispatcher->addVirtualPath(vPath);
	}
	TestServer server(*pDispatcher);
	const auto vary = [&server](const std::string& uri, const std::string& origin)
	{
		HTTPRequest request(HTTPRequest::HTTP_GET, uri);
		if (!origin.empty()) request.set("Origin"s, origin);
		HTTPResponse response;
		server.send(request, response);
		return response.get("Vary"s, ""s);
	};
	const std::string sameOrigin("http://127.0.0.1:"s + std::to_string(server.port()));

	for (const auto& origin: {""s, sameOrigin, "https://a.example"s})
	{
		assertEqual ("Origin"s, vary("/reflect"s, origin));
		assertEqual ("Origin"s, vary("/list"s, origin));
		assertEqual (""s, vary("/none"s, origin));
		assertEqual (""s, vary("/any"s, origin));
	}
}


void WebServerDispatcherTest::testIsSameOrigin()
{
	ServerRequestStub plainRequest("127.0.0.1:50000"s, false);
	plainRequest.set("Host"s, "web.example:8080"s);
	assertTrue (WebServerDispatcher::isSameOrigin(plainRequest, "http://web.example:8080"s));
	assertTrue (WebServerDispatcher::isSameOrigin(plainRequest, "HTTP://Web.Example:8080"s));
	assertFalse (WebServerDispatcher::isSameOrigin(plainRequest, "https://web.example:8080"s));
	assertFalse (WebServerDispatcher::isSameOrigin(plainRequest, "http://web.example"s));
	assertFalse (WebServerDispatcher::isSameOrigin(plainRequest, "http://other.example:8080"s));
	assertFalse (WebServerDispatcher::isSameOrigin(plainRequest, "null"s));

	ServerRequestStub secureRequest("127.0.0.1:50000"s, true);
	secureRequest.set("Host"s, "web.example"s);
	assertTrue (WebServerDispatcher::isSameOrigin(secureRequest, "https://web.example:443"s));
	assertFalse (WebServerDispatcher::isSameOrigin(secureRequest, "http://web.example"s));
}


void WebServerDispatcherTest::testInvalidIfModifiedSinceIsIgnored()
{
	WebServerDispatcher::Ptr pDispatcher = _pFixture->createDispatcher();
	TestServer server(*pDispatcher);

	HTTPRequest invalid(HTTPRequest::HTTP_GET, "/dir/hello.txt"s);
	invalid.set("If-Modified-Since"s, "x"s);
	HTTPResponse response;
	assertEqual (TemporaryBundle::HELLO_TXT, server.send(invalid, response));
	assertTrue (response.getStatus() == HTTPResponse::HTTP_OK);

	HTTPRequest future(HTTPRequest::HTTP_GET, "/dir/hello.txt"s);
	future.set("If-Modified-Since"s, "Fri, 01 Jan 2100 00:00:00 GMT"s);
	server.send(future, response);
	assertTrue (response.getStatus() == HTTPResponse::HTTP_NOT_MODIFIED);
}


void WebServerDispatcherTest::testChangedResourceOfDirectoryBundleIsModified()
{
	const Path bundlePath(Path::forDirectory(_pFixture->bundles.bundle()->path()));
	TemporaryBundle::writeFile(bundlePath, "www/hello.txt"s, "changed"s);
	// The bundle directory is older than If-Modified-Since, the resource newer. Only the
	// time of the file is set: Poco::File::setLastModified() cannot open a directory on Windows.
	const Poco::Timestamp bundleModified = Poco::File(bundlePath).getLastModified();
	const Path resourcePath(bundlePath, Path("www/hello.txt"s, Path::PATH_UNIX));
	Poco::File(resourcePath).setLastModified(bundleModified + 2*Poco::Timespan::HOURS);
	WebServerDispatcher::Ptr pDispatcher = _pFixture->createDispatcher();
	TestServer server(*pDispatcher);

	HTTPRequest request(HTTPRequest::HTTP_GET, "/dir/hello.txt"s);
	request.set("If-Modified-Since"s,
		Poco::DateTimeFormatter::format(bundleModified + Poco::Timespan::HOURS, Poco::DateTimeFormat::HTTP_FORMAT));
	HTTPResponse response;
	assertEqual ("changed"s, server.send(request, response));
	assertTrue (response.getStatus() == HTTPResponse::HTTP_OK);
}


void WebServerDispatcherTest::testCompressedResponses()
{
	WebServerDispatcher::Ptr pDispatcher = _pFixture->createDispatcher(WebServerDispatcher::CONF_OPT_COMPRESS_RESPONSES);
	TestServer server(*pDispatcher);

	HTTPRequest gzip(HTTPRequest::HTTP_GET, "/zip/hello.txt"s);
	gzip.set("Accept-Encoding"s, "deflate, gzip;q=0.5"s);
	HTTPResponse response;
	std::istringstream compressed(server.send(gzip, response));
	assertEqual ("gzip"s, response.get("Content-Encoding"s, ""s));
	assertTrue (response.getChunkedTransferEncoding());
	assertEqual ("Accept-Encoding"s, response.get("Vary"s, ""s));
	Poco::InflatingInputStream inflater(compressed, Poco::InflatingStreamBuf::STREAM_GZIP);
	std::string body;
	Poco::StreamCopier::copyToString(inflater, body);
	assertEqual (TemporaryBundle::HELLO_TXT, body);

	for (const auto& acceptEncoding: {"gzip;q=0"s, "gzip;q=0.000, *"s, "*;q=0"s, "identity"s})
	{
		HTTPRequest identity(HTTPRequest::HTTP_GET, "/zip/hello.txt"s);
		identity.set("Accept-Encoding"s, acceptEncoding);
		assertEqual (TemporaryBundle::HELLO_TXT, server.send(identity, response));
		assertFalse (response.has("Content-Encoding"s));
		assertEqual ("Accept-Encoding"s, response.get("Vary"s, ""s));
	}
}


void WebServerDispatcherTest::testStrictPersistentResourceIsNotReplacedOnError()
{
	WebServerDispatcher::Ptr pDispatcher = new WebServerDispatcher(_pFixture->dispatcherConfig());
	pDispatcher->addVirtualPath(WebServerDispatcher::VirtualPath("/unloaded"s, "persist"s, WebServerDispatcher::PathSecurity(), _pFixture->bundles.unloadedBundle()));
	TestServer server(*pDispatcher);
	LogCapture capture(_pFixture->bundles.context()->logger().name());

	HTTPRequest request(HTTPRequest::HTTP_GET, "/unloaded/file.txt"s);
	HTTPResponse response;
	server.send(request, response);
	assertTrue (response.getStatus() == HTTPResponse::HTTP_NOT_FOUND);
	const std::vector<Poco::Message> messages = capture.channel().waitForMessages(1);
	assertEqual (1, messages.size());
	assertTrue (messages[0].getText().find("web.static.strict"s) != std::string::npos);
}


void WebServerDispatcherTest::testUnreadRequestBodyClosesConnection()
{
	WebServerDispatcher::Ptr pDispatcher = new WebServerDispatcher(_pFixture->dispatcherConfig());
	pDispatcher->addVirtualPath(handlerPath("/ignore"s, ""s, _pFixture->bundles.bundle()));
	pDispatcher->addVirtualPath(WebServerDispatcher::VirtualPath("/read"s, new TestHandlerFactory<BodyHandler>, WebServerDispatcher::PathSecurity(), _pFixture->bundles.bundle()));
	TestServer server(*pDispatcher);
	const auto post = [](Poco::Net::HTTPClientSession& session, const std::string& uri)
	{
		HTTPRequest request(HTTPRequest::HTTP_POST, uri, Poco::Net::HTTPMessage::HTTP_1_1);
		const std::string body("request body");
		request.setContentLength(static_cast<std::streamsize>(body.size()));
		session.sendRequest(request) << body;
		HTTPResponse response;
		std::string responseBody;
		Poco::StreamCopier::copyToString(session.receiveResponse(response), responseBody);
		return responseBody;
	};

	Poco::Net::HTTPClientSession kept("127.0.0.1"s, server.port());
	kept.setTimeout(CLIENT_TIMEOUT);
	kept.setKeepAlive(true);
	assertEqual ("request body"s, post(kept, "/read"s));
	assertEqual ("request body"s, post(kept, "/read"s));

	for (const auto& uri: {"/ignore"s, "/missing"s})
	{
		Poco::Net::HTTPClientSession session("127.0.0.1"s, server.port());
		session.setTimeout(CLIENT_TIMEOUT);
		session.setKeepAlive(true);
		post(session, uri);
		// The server closes the connection: the socket becomes readable and reports its end.
		assertTrue (session.socket().poll(CLIENT_TIMEOUT, Poco::Net::Socket::SELECT_READ));
		char ch = 0;
		assertEqual (0, session.socket().receiveBytes(&ch, 1));
	}
}


void WebServerDispatcherTest::testStaticResourceResponses()
{
	WebServerDispatcher::Ptr pDispatcher = _pFixture->createDispatcher();
	WebServerDispatcher::PathSecurity security;
	security.secure = true;
	pDispatcher->addVirtualPath(WebServerDispatcher::VirtualPath("/secure"s, "www"s, security, _pFixture->bundles.bundle()));
	TestServer server(*pDispatcher);

	HTTPRequest directory(HTTPRequest::HTTP_GET, "/dir?a=1&b=2"s);
	HTTPResponse response;
	server.send(directory, response);
	assertTrue (response.getStatus() == HTTPResponse::HTTP_FOUND);
	assertEqual ("/dir/?a=1&b=2"s, response.get("Location"s, ""s));

	HTTPRequest index(HTTPRequest::HTTP_GET, "/zip/"s);
	assertEqual (TemporaryBundle::INDEX_HTML, server.send(index, response));
	assertEqual ("text/html"s, response.getContentType());

	HTTPRequest missing(HTTPRequest::HTTP_GET, "/zip/missing.txt"s);
	server.send(missing, response);
	assertTrue (response.getStatus() == HTTPResponse::HTTP_NOT_FOUND);

	HTTPRequest plain(HTTPRequest::HTTP_GET, "/secure/hello.txt"s);
	server.send(plain, response);
	assertTrue (response.getStatus() == HTTPResponse::HTTP_FORBIDDEN);
}


#if defined(OSPWEB_TEST_LIBRARY)


namespace
{
	using LibraryLoader = LibraryReference<WebRequestHandlerFactory>::Loader;

	[[nodiscard]] WebServerDispatcher::RequestHandlerFactoryPtr sessionValueFactory(
		const std::shared_ptr<LibraryLoader>& pLoader, const BundleContext::Ptr& pContext)
		/// Returns a factory of the test library that keeps it loaded; its handlers store a value
		/// in the session "test".
	{
		auto pLibrary = std::make_shared<LibraryReference<WebRequestHandlerFactory>>(pLoader, OSPWEB_TEST_LIBRARY,
			"WebServer"s);
		std::unique_ptr<WebRequestHandlerFactory> pLibraryFactory(
			pLibrary->create("SessionValueRequestHandlerFactory"s));
		pLibraryFactory->init(pContext);
		return new LibraryRequestHandlerFactory(std::move(pLibrary), std::move(pLibraryFactory));
	}

	void storeSessionValue(Poco::Net::HTTPRequestHandlerFactory& factory, const WebSession::Ptr& pSession)
		/// Lets a handler of the factory store its value in the given session.
	{
		ServerRequestStub request("127.0.0.1:50000"s, false);
		Poco::Net::NameValueCollection cookies;
		cookies.add("osp.web.session.test"s, pSession->id());
		request.setCookies(cookies);
		std::unique_ptr<Poco::Net::HTTPRequestHandler> pHandler(factory.createRequestHandler(request));
		pHandler->handleRequest(request, request.response());
	}

	void removeSessions(WebSessionManager& sessionManager)
	{
		std::vector<std::string> ids;
		sessionManager.forEachSession([&ids](const std::string& id, const WebSession&)
			{
				ids.push_back(id);
			});
		for (const auto& id: ids)
		{
			sessionManager.remove(sessionManager.findById(id));
		}
	}

	class StopDuringRequestFactory: public Poco::Net::HTTPRequestHandlerFactory
		/// Fires the stop events of the directory bundle during a request, then creates the
		/// handler with the given factory.
	{
	public:
		StopDuringRequestFactory(TemporaryBundle& bundles, WebServerDispatcher::RequestHandlerFactoryPtr pFactory):
			_pBundles(&bundles),
			_pFactory(std::move(pFactory))
		{
		}

		[[nodiscard]] Poco::Net::HTTPRequestHandler* createRequestHandler(
			const Poco::Net::HTTPServerRequest& request) override
		{
			BundleEvent stopping(_pBundles->bundle(), BundleEvent::EV_BUNDLE_STOPPING);
			_pBundles->events().bundleStopping(this, stopping);
			BundleEvent stopped(_pBundles->bundle(), BundleEvent::EV_BUNDLE_STOPPED);
			_pBundles->events().bundleStopped(this, stopped);
			return _pFactory->createRequestHandler(request);
		}

	private:
		TemporaryBundle* _pBundles;
		WebServerDispatcher::RequestHandlerFactoryPtr _pFactory;
	};

	class RemoveDuringRequestFilterFactory: public WebFilterFactory
		/// Removes the filter of the given media type during a request, then creates the
		/// filter with the given factory.
	{
	public:
		RemoveDuringRequestFilterFactory(WebServerDispatcher& dispatcher, const std::string& mediaType,
			WebServerDispatcher::WebFilterFactoryPtr pFactory):
			_pDispatcher(&dispatcher),
			_mediaType(mediaType),
			_pFactory(std::move(pFactory))
		{
		}

		[[nodiscard]] WebFilter* createFilter(const WebFilter::Args& args) override
		{
			_pDispatcher->removeFilter(_mediaType);
			return _pFactory->createFilter(args);
		}

	private:
		WebServerDispatcher* _pDispatcher;
		std::string _mediaType;
		WebServerDispatcher::WebFilterFactoryPtr _pFactory;
	};
}


void WebServerDispatcherTest::testBundleLibraryStaysLoadedWhileSessionsExist()
{
	// Created before the session manager, as in the web bundle, so that the dispatcher
	// removes the mappings of a stopping bundle before the session manager releases the
	// session attributes of the bundle.
	WebServerDispatcher::Ptr pDispatcher = new WebServerDispatcher(_pFixture->dispatcherConfig());
	WebSessionManager::Ptr pSessionManager = new WebSessionManager(_pFixture->bundles.context());
	_pFixture->bundles.registry().registerService(WebSessionManager::SERVICE_NAME, pSessionManager, Properties());
	auto pLoader = std::make_shared<LibraryLoader>();
	pDispatcher->addVirtualPath(WebServerDispatcher::VirtualPath("/library"s,
		sessionValueFactory(pLoader, _pFixture->bundles.context()), WebServerDispatcher::PathSecurity(),
		_pFixture->bundles.bundle()));
	ServerRequestStub request("127.0.0.1:50000"s, false);
	WebSession::Ptr pSession = pSessionManager->create("test"s, request, SESSION_TIMEOUT_SECONDS,
		_pFixture->bundles.context());
	WebServerDispatcher::PathMap mappings;
	pDispatcher->virtualPathMappings(mappings);
	storeSessionValue(*mappings.at("/library/"s).pFactory, pSession);
	mappings.clear();
	assertTrue (pSession->has("value"s));

	BundleEvent stopping(_pFixture->bundles.bundle(), BundleEvent::EV_BUNDLE_STOPPING);
	_pFixture->bundles.events().bundleStopping(this, stopping);

	assertFalse (pSession->has("value"s));
	assertTrue (pLoader->isLibraryLoaded(OSPWEB_TEST_LIBRARY));

	BundleEvent stopped(_pFixture->bundles.bundle(), BundleEvent::EV_BUNDLE_STOPPED);
	_pFixture->bundles.events().bundleStopped(this, stopped);

	assertTrue (pLoader->isLibraryLoaded(OSPWEB_TEST_LIBRARY));
	pSessionManager->remove(pSession);
	pSession.reset();
	assertFalse (pLoader->isLibraryLoaded(OSPWEB_TEST_LIBRARY));
}


void WebServerDispatcherTest::testValueInSessionOfOtherBundleOutlivesBundleStop()
{
	WebServerDispatcher::Ptr pDispatcher = new WebServerDispatcher(_pFixture->dispatcherConfig());
	WebSessionManager::Ptr pSessionManager = new WebSessionManager(_pFixture->bundles.context());
	_pFixture->bundles.registry().registerService(WebSessionManager::SERVICE_NAME, pSessionManager, Properties());
	auto pLoader = std::make_shared<LibraryLoader>();
	pDispatcher->addVirtualPath(WebServerDispatcher::VirtualPath("/library"s,
		sessionValueFactory(pLoader, _pFixture->bundles.context()), WebServerDispatcher::PathSecurity(),
		_pFixture->bundles.bundle()));
	// The session of another bundle keeps the values of a stopping bundle.
	ServerRequestStub request("127.0.0.1:50000"s, false);
	const BundleContext::Ptr pOtherContext = _pFixture->bundles.context()->contextForBundle(
		_pFixture->bundles.zipBundle());
	WebSession::Ptr pSession = pSessionManager->create("test"s, request, SESSION_TIMEOUT_SECONDS, pOtherContext);
	WebSession::Ptr pRenewedSession = pSessionManager->create("test"s, request, SESSION_TIMEOUT_SECONDS, pOtherContext);
	WebServerDispatcher::PathMap mappings;
	pDispatcher->virtualPathMappings(mappings);
	storeSessionValue(*mappings.at("/library/"s).pFactory, pSession);
	storeSessionValue(*mappings.at("/library/"s).pFactory, pRenewedSession);
	mappings.clear();

	BundleEvent stopping(_pFixture->bundles.bundle(), BundleEvent::EV_BUNDLE_STOPPING);
	_pFixture->bundles.events().bundleStopping(this, stopping);
	BundleEvent stopped(_pFixture->bundles.bundle(), BundleEvent::EV_BUNDLE_STOPPED);
	_pFixture->bundles.events().bundleStopped(this, stopped);
	pRenewedSession = pSessionManager->renew("test"s, request, pRenewedSession);

	assertTrue (pSession->has("value"s));
	assertTrue (pRenewedSession->has("value"s));
	assertTrue (pLoader->isLibraryLoaded(OSPWEB_TEST_LIBRARY));
	pSessionManager->remove(pSession);
	pSession.reset();
	assertTrue (pLoader->isLibraryLoaded(OSPWEB_TEST_LIBRARY));
	pSessionManager->remove(pRenewedSession);
	pRenewedSession.reset();
	assertFalse (pLoader->isLibraryLoaded(OSPWEB_TEST_LIBRARY));
}


void WebServerDispatcherTest::testFilterLibraryStaysLoadedWhileSessionsExist()
{
	WebServerDispatcher::Ptr pDispatcher = new WebServerDispatcher(_pFixture->dispatcherConfig());
	WebSessionManager::Ptr pSessionManager = new WebSessionManager(_pFixture->bundles.context());
	_pFixture->bundles.registry().registerService(WebSessionManager::SERVICE_NAME, pSessionManager, Properties());
	auto pLoader = std::make_shared<LibraryReference<WebFilterFactory>::Loader>();
	auto pLibrary = std::make_shared<LibraryReference<WebFilterFactory>>(pLoader, OSPWEB_TEST_LIBRARY, "WebFilter"s);
	std::unique_ptr<WebFilterFactory> pLibraryFactory(pLibrary->create("SessionValueFilterFactory"s));
	pLibraryFactory->init(_pFixture->bundles.context());
	WebServerDispatcher::WebFilterFactoryPtr pFactory = Poco::makeShared<LibraryWebFilterFactory>(std::move(pLibrary),
		std::move(pLibraryFactory));
	ServerRequestStub request("127.0.0.1:50000"s, false);
	WebSession::Ptr pSession = pSessionManager->create("test"s, request, SESSION_TIMEOUT_SECONDS, _pFixture->bundles.context());
	Poco::Net::NameValueCollection cookies;
	cookies.add("osp.web.session.test"s, pSession->id());
	request.setCookies(cookies);
	std::istringstream resource;
	std::unique_ptr<WebFilter>(pFactory->createFilter(WebFilter::Args()))->process(request, request.response(),
		"/filtered"s, resource, _pFixture->bundles.bundle());
	pDispatcher->addFilter("text/x-session"s, std::move(pFactory), WebFilter::Args(), _pFixture->bundles.bundle());
	assertTrue (pSession->has("value"s));

	pDispatcher->removeFilter("text/x-session"s);

	assertTrue (pLoader->isLibraryLoaded(OSPWEB_TEST_LIBRARY));
	pSessionManager->remove(pSession);
	pSession.reset();
	assertFalse (pLoader->isLibraryLoaded(OSPWEB_TEST_LIBRARY));
}


void WebServerDispatcherTest::testRequestDuringBundleStopKeepsLibraryForItsSession()
{
	WebServerDispatcher::Ptr pDispatcher = new WebServerDispatcher(_pFixture->dispatcherConfig());
	WebSessionManager::Ptr pSessionManager = new WebSessionManager(_pFixture->bundles.context());
	_pFixture->bundles.registry().registerService(WebSessionManager::SERVICE_NAME, pSessionManager, Properties());
	auto pLoader = std::make_shared<LibraryLoader>();
	pDispatcher->addVirtualPath(WebServerDispatcher::VirtualPath("/library"s,
		new StopDuringRequestFactory(_pFixture->bundles, sessionValueFactory(pLoader, _pFixture->bundles.context())),
		WebServerDispatcher::PathSecurity(), _pFixture->bundles.bundle()));

	// The handler creates the session after the bundle has stopped.
	ServerRequestStub request("127.0.0.1:50000"s, false);
	request.setURI("/library"s);
	pDispatcher->handleRequest(request, request.response(), false);

	assertTrue (request.response().getStatus() == HTTPResponse::HTTP_NO_CONTENT);
	assertEqual (std::size_t{1}, pSessionManager->countSessions());
	assertTrue (pLoader->isLibraryLoaded(OSPWEB_TEST_LIBRARY));
	removeSessions(*pSessionManager);
	assertFalse (pLoader->isLibraryLoaded(OSPWEB_TEST_LIBRARY));
}


void WebServerDispatcherTest::testRequestDuringFilterRemovalKeepsLibraryForItsSession()
{
	WebServerDispatcher::Ptr pDispatcher = _pFixture->createDispatcher();
	WebSessionManager::Ptr pSessionManager = new WebSessionManager(_pFixture->bundles.context());
	_pFixture->bundles.registry().registerService(WebSessionManager::SERVICE_NAME, pSessionManager, Properties());
	auto pLoader = std::make_shared<LibraryReference<WebFilterFactory>::Loader>();
	auto pLibrary = std::make_shared<LibraryReference<WebFilterFactory>>(pLoader, OSPWEB_TEST_LIBRARY, "WebFilter"s);
	std::unique_ptr<WebFilterFactory> pLibraryFactory(pLibrary->create("SessionValueFilterFactory"s));
	pLibraryFactory->init(_pFixture->bundles.context());
	WebServerDispatcher::WebFilterFactoryPtr pFactory = Poco::makeShared<LibraryWebFilterFactory>(std::move(pLibrary),
		std::move(pLibraryFactory));
	pDispatcher->addFilter("text/plain"s,
		new RemoveDuringRequestFilterFactory(*pDispatcher, "text/plain"s, std::move(pFactory)), WebFilter::Args(),
		_pFixture->bundles.bundle());

	// The filter creates the session after it has been removed.
	ServerRequestStub request("127.0.0.1:50000"s, false);
	request.setURI("/dir/hello.txt"s);
	pDispatcher->handleRequest(request, request.response(), false);

	assertEqual (std::size_t{1}, pSessionManager->countSessions());
	assertTrue (pLoader->isLibraryLoaded(OSPWEB_TEST_LIBRARY));
	removeSessions(*pSessionManager);
	assertFalse (pLoader->isLibraryLoaded(OSPWEB_TEST_LIBRARY));
}


void WebServerDispatcherTest::testUninstallingBundleRemovesSessionsThatKeepItsLibrary()
{
	WebServerDispatcher::Ptr pDispatcher = new WebServerDispatcher(_pFixture->dispatcherConfig());
	WebSessionManager::Ptr pSessionManager = new WebSessionManager(_pFixture->bundles.context());
	_pFixture->bundles.registry().registerService(WebSessionManager::SERVICE_NAME, pSessionManager, Properties());
	auto pLoader = std::make_shared<LibraryLoader>();
	pDispatcher->addVirtualPath(WebServerDispatcher::VirtualPath("/library"s,
		sessionValueFactory(pLoader, _pFixture->bundles.context()), WebServerDispatcher::PathSecurity(),
		_pFixture->bundles.bundle()));
	ServerRequestStub request("127.0.0.1:50000"s, false);
	const BundleContext::Ptr pOtherContext = _pFixture->bundles.context()->contextForBundle(
		_pFixture->bundles.zipBundle());
	WebSession::Ptr pSession = pSessionManager->create("test"s, request, SESSION_TIMEOUT_SECONDS, pOtherContext);
	const std::string retainingId = pSession->id();
	WebServerDispatcher::PathMap mappings;
	pDispatcher->virtualPathMappings(mappings);
	storeSessionValue(*mappings.at("/library/"s).pFactory, pSession);
	mappings.clear();
	pSession.reset();

	BundleEvent stopping(_pFixture->bundles.bundle(), BundleEvent::EV_BUNDLE_STOPPING);
	_pFixture->bundles.events().bundleStopping(this, stopping);
	BundleEvent stopped(_pFixture->bundles.bundle(), BundleEvent::EV_BUNDLE_STOPPED);
	_pFixture->bundles.events().bundleStopped(this, stopped);
	const std::string laterId = pSessionManager->create("test"s, request, SESSION_TIMEOUT_SECONDS, pOtherContext)->id();
	assertTrue (pLoader->isLibraryLoaded(OSPWEB_TEST_LIBRARY));

	BundleEvent uninstalling(_pFixture->bundles.bundle(), BundleEvent::EV_BUNDLE_UNINSTALLING);
	_pFixture->bundles.events().bundleUninstalling(this, uninstalling);

	assertTrue (pSessionManager->findById(retainingId) == nullptr);
	assertTrue (pSessionManager->findById(laterId) != nullptr);
	assertFalse (pLoader->isLibraryLoaded(OSPWEB_TEST_LIBRARY));
}


#endif // OSPWEB_TEST_LIBRARY


void WebServerDispatcherTest::setUp()
{
	_pFixture = std::make_unique<Fixture>();
}


void WebServerDispatcherTest::tearDown()
{
	_pFixture.reset();
}


CppUnit::Test* WebServerDispatcherTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("WebServerDispatcherTest");

	CppUnit_addTest(pSuite, WebServerDispatcherTest, testVirtualDirectoryRoot);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testVirtualDirectoryAllow);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testVirtualDirectoryFail);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testChildInOtherCaseConflictsWithParent);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testRemoveDir);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testRemovePatternRemovesEveryMapping);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testParseRange);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testCanonicalPathWithQuery);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testRedirectLocationOn401);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testStaticResourceHasExactLength);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testCachedResourceOutlivesBundleStop);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testStaticResourceResponses);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testCleanPathRejectsDotSegments);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testCleanPathRejectsControlCharacters);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testControlCharacterInPathAnswersBadRequest);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testCleanPathRejectsSegmentsEndingWithDotOrSpace);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testCaseVariantOfProtectedPathIsProtected);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testNonASCIIVariantOfProtectedPathIsProtected);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testLongerFormOfProtectedPathIsProtected);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testFoldedSpellingOfProtectedPathIsNotFound);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testExactMatchIgnoresCase);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testResourcePathRejectsOtherNamesOfFiles);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testColonAnswersBadRequestForResourcesOnly);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testDotSegmentsAnswerBadRequest);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testMalformedURIAnswersBadRequest);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testPersistentResourceStaysInsideItsDirectory);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testPersistentResourceEndingWithSlash);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testPersistentResourceWithEmptySegment);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testPersistentResourceIsFoundOnlyUnderItsName);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testRangeRequests);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testClientAuthorizationHeadersAreRemoved);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testRequestIdentityIsSetWhileHandlerRuns);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testAuthorizationDoesNotBlockOtherRequests);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testUnavailableAuthServiceAnswers503);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testUnavailableAuthServiceIsLoggedOnce);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testOptionalCredentialsWithoutAuthService);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testOptionalCredentialsWithSessionWithoutAuthService);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testUnavailableAnswerHasBundleMessage);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testAuthorizationResponses);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testBasicCredentialsAreAuthorizedUnderAccountName);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testSessionCSRFProtection);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testSessionUserWithoutPermissionIsForbidden);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testSessionUserWithoutPermissionForHomePageIsNotAuthorized);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testDisabledUserIsNotAuthenticated);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testUnregisteredSessionManagerIsNotUsed);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testSecondFilterForMediaTypeIsRejected);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testLogMessagesHoldNoTokenAndNoControlCharacters);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testLoggedRequestDataHasNoControlCharacters);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testFullRequestLogEscapesMethodAndFieldValue);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testFullResponseLogEscapesFieldValue);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testFullResponseLogEscapesReasonAndFieldOfHandler);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testBadRequestDebugMessageHasNoControlCharacters);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testFullRequestLogHasNoCredentials);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testFullRequestLogRedactsFieldsWithCredentialOrNonTokenNames);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testFullRequestLogKeepsRequestLineWithFieldNameAsMethod);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testForwardedClientAddressIsAnIPAddressOnly);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testJSONErrorResponseEscapesRequestData);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testCORSOrigins);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testOriginDependentResponsesVaryByOrigin);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testIsSameOrigin);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testInvalidIfModifiedSinceIsIgnored);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testChangedResourceOfDirectoryBundleIsModified);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testCompressedResponses);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testStrictPersistentResourceIsNotReplacedOnError);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testUnreadRequestBodyClosesConnection);
#if defined(OSPWEB_TEST_LIBRARY)
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testBundleLibraryStaysLoadedWhileSessionsExist);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testValueInSessionOfOtherBundleOutlivesBundleStop);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testFilterLibraryStaysLoadedWhileSessionsExist);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testRequestDuringBundleStopKeepsLibraryForItsSession);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testRequestDuringFilterRemovalKeepsLibraryForItsSession);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testUninstallingBundleRemovesSessionsThatKeepItsLibrary);
#endif

	return pSuite;
}
