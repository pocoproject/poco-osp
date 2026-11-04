//
// WebServerDispatcherTest.cpp
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
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
#include "Poco/OSP/Bundle.h"
#include "Poco/OSP/BundleEvent.h"
#include "Poco/OSP/ServiceRegistry.h"
#include "Poco/OSP/Auth/AuthService.h"
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
#include "Poco/Exception.h"
#include "Poco/Types.h"
#include "Poco/Timespan.h"
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
		using WebServerDispatcher::findResource;
	};

	class TestAuthService: public Poco::OSP::Auth::AuthService
		/// Knows the user "alice" with the password "secret" and the permission "read".
	{
	public:
		bool authenticate(const std::string& userName, const std::string& credentials) const override
		{
			return userName == "alice" && credentials == "secret";
		}

		bool authorize(const std::string& userName, const std::string& permission) const override
		{
			return userName == "alice" && permission == "read";
		}

		bool authorize(const std::string& userName, const std::string& scope, const std::string& permission) const override
		{
			return authorize(userName, permission);
		}

		bool userExists(const std::string& userName) const override
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
		bool validateToken(const std::string& token, std::string& username, std::string& scope) override
		{
			if (token != "alice-token") return false;
			username = "alice";
			scope = "api";
			return true;
		}
	};

	class BlockingAuthService: public TestAuthService
		/// Blocks in authenticate() until released, to keep a request inside authorization.
	{
	public:
		bool authenticate(const std::string& userName, const std::string& credentials) const override
		{
			_entered.set();
			_released.tryWait(WAIT_TIMEOUT_MS);
			return TestAuthService::authenticate(userName, credentials);
		}

		bool waitUntilEntered()
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

	template <class Handler>
	class TestHandlerFactory: public Poco::Net::HTTPRequestHandlerFactory
	{
	public:
		Poco::Net::HTTPRequestHandler* createRequestHandler(const Poco::Net::HTTPServerRequest&) override
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

		Poco::Net::HTTPRequestHandler* createRequestHandler(const Poco::Net::HTTPServerRequest&) override
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

		WebFilter* createFilter(const WebFilter::Args&) override
		{
			return new NamedFilter(_name);
		}

	private:
		std::string _name;
	};

	WebServerDispatcher::VirtualPath handlerPath(const std::string& path, const std::string& permission, const Bundle::Ptr& pOwner, int authMethods = 0)
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

		Poco::UInt16 port() const
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

	Session loginSession(TestServer& server, const std::string& uri)
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

	WebServerDispatcher::Ptr createDispatcher(int options = 0) const
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

	WebServerDispatcher::Config dispatcherConfig(int options = 0) const
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


WebServerDispatcherTest::~WebServerDispatcherTest()
{
}


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
	assertEqual (1, mappings.count("/sub/subsub/"s));

	disp.removeVirtualPath("/sub/subsub");
	disp.virtualPathMappings(mappings);
	assertEqual (0, mappings.count("/sub/subsub/"s));
	assertEqual (1, mappings.count("/sub/"s));

	disp.addVirtualPath(vPath);
	disp.virtualPathMappings(mappings);
	assertEqual (1, mappings.count("/sub/subsub/"s));
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
	pDispatcher->addFilter("text/plain"s, new NamedFilterFactory("first"s), WebFilter::Args());
	try
	{
		pDispatcher->addFilter("text/plain"s, new NamedFilterFactory("second"s), WebFilter::Args());
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

	const std::string uri("/missing\\\"%01");
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


void WebServerDispatcherTest::testBundleLibraryStaysLoadedUntilBundleStopped()
{
	// Created before the session manager, as in the web bundle, so that the dispatcher
	// removes the mappings of a stopping bundle before the session manager releases the
	// session attributes of the bundle.
	WebServerDispatcher::Ptr pDispatcher = new WebServerDispatcher(_pFixture->dispatcherConfig());
	WebSessionManager::Ptr pSessionManager = new WebSessionManager(_pFixture->bundles.context());
	_pFixture->bundles.registry().registerService(WebSessionManager::SERVICE_NAME, pSessionManager, Properties());
	using Reference = LibraryReference<WebRequestHandlerFactory>;
	auto pLoader = std::make_shared<Reference::Loader>();
	const std::string path(OSPWEB_TEST_LIBRARY);
	auto pLibrary = std::make_shared<Reference>(pLoader, path, "WebServer"s);
	std::unique_ptr<WebRequestHandlerFactory> pLibraryFactory(pLoader->create("SessionValueRequestHandlerFactory"s));
	pLibraryFactory->init(_pFixture->bundles.context());
	WebServerDispatcher::RequestHandlerFactoryPtr pFactory = new LibraryRequestHandlerFactory(std::move(pLibrary), std::move(pLibraryFactory));
	pDispatcher->addVirtualPath(WebServerDispatcher::VirtualPath("/library"s, pFactory, WebServerDispatcher::PathSecurity(), _pFixture->bundles.bundle()));
	ServerRequestStub request("127.0.0.1:50000"s, false);
	WebSession::Ptr pSession = pSessionManager->create("test"s, request, SESSION_TIMEOUT_SECONDS, _pFixture->bundles.context());
	Poco::Net::NameValueCollection cookies;
	cookies.add("osp.web.session.test"s, pSession->id());
	request.setCookies(cookies);
	std::unique_ptr<Poco::Net::HTTPRequestHandler> pHandler(pFactory->createRequestHandler(request));
	pFactory.reset();
	pHandler->handleRequest(request, request.response());
	pHandler.reset();
	assertTrue (pSession->has("value"s));

	BundleEvent stopping(_pFixture->bundles.bundle(), BundleEvent::EV_BUNDLE_STOPPING);
	_pFixture->bundles.events().bundleStopping(this, stopping);

	assertFalse (pSession->has("value"s));
	assertTrue (pLoader->isLibraryLoaded(path));

	BundleEvent stopped(_pFixture->bundles.bundle(), BundleEvent::EV_BUNDLE_STOPPED);
	_pFixture->bundles.events().bundleStopped(this, stopped);

	assertFalse (pLoader->isLibraryLoaded(path));
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
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testRemoveDir);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testRemovePatternRemovesEveryMapping);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testParseRange);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testCanonicalPathWithQuery);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testRedirectLocationOn401);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testStaticResourceHasExactLength);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testStaticResourceResponses);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testCleanPathRejectsDotSegments);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testDotSegmentsAnswerBadRequest);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testMalformedURIAnswersBadRequest);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testPersistentResourceStaysInsideItsDirectory);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testPersistentResourceEndingWithSlash);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testRangeRequests);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testClientAuthorizationHeadersAreRemoved);
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
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testUnregisteredSessionManagerIsNotUsed);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testSecondFilterForMediaTypeIsRejected);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testLogMessagesHoldNoTokenAndNoControlCharacters);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testFullRequestLogHasNoCredentials);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testForwardedClientAddressIsAnIPAddressOnly);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testJSONErrorResponseEscapesRequestData);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testCORSOrigins);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testIsSameOrigin);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testInvalidIfModifiedSinceIsIgnored);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testCompressedResponses);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testStrictPersistentResourceIsNotReplacedOnError);
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testUnreadRequestBodyClosesConnection);
#if defined(OSPWEB_TEST_LIBRARY)
	CppUnit_addTest(pSuite, WebServerDispatcherTest, testBundleLibraryStaysLoadedUntilBundleStopped);
#endif

	return pSuite;
}
