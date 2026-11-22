//
// WebEventRequestHandlerTest.cpp
//
// Copyright (c) 2026, Aleph ONE Software Engineering LLC
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "WebEventRequestHandlerTest.h"
#include "TemporaryBundle.h"
#include "Poco/OSP/WebEvent/WebEventRequestHandler.h"
#include "Poco/OSP/WebEvent/WebEventServiceImpl.h"
#include "Poco/OSP/Web/TokenValidator.h"
#include "Poco/OSP/Web/WebServerDispatcher.h"
#include "Poco/OSP/Web/WebServerExtensionPoint.h"
#include "Poco/OSP/Web/WebServerRequestHandlerFactory.h"
#include "Poco/OSP/Web/WebSession.h"
#include "Poco/OSP/Web/WebSessionManager.h"
#include "Poco/OSP/Auth/AuthService.h"
#include "Poco/OSP/Bundle.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/PreferencesService.h"
#include "Poco/OSP/Properties.h"
#include "Poco/Net/HTTPBasicCredentials.h"
#include "Poco/Net/HTTPClientSession.h"
#include "Poco/Net/HTTPCookie.h"
#include "Poco/Net/HTTPMessage.h"
#include "Poco/Net/HTTPRequest.h"
#include "Poco/Net/HTTPRequestHandler.h"
#include "Poco/Net/HTTPRequestHandlerFactory.h"
#include "Poco/Net/HTTPResponse.h"
#include "Poco/Net/HTTPServer.h"
#include "Poco/Net/HTTPServerParams.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Net/HTTPServerResponse.h"
#include "Poco/Net/NameValueCollection.h"
#include "Poco/Net/NetException.h"
#include "Poco/Net/ServerSocket.h"
#include "Poco/Net/SocketAddress.h"
#include "Poco/Net/WebSocket.h"
#include "Poco/DOM/DOMParser.h"
#include "Poco/DOM/Document.h"
#include "Poco/DOM/Element.h"
#include "Poco/DOM/NodeList.h"
#include "Poco/SAX/InputSource.h"
#include "Poco/Util/MapConfiguration.h"
#include "Poco/AutoPtr.h"
#include "Poco/Bugcheck.h"
#include "Poco/Delegate.h"
#include "Poco/Environment.h"
#include "Poco/Event.h"
#include "Poco/Exception.h"
#include "Poco/FileStream.h"
#include "Poco/Path.h"
#include "Poco/SharedPtr.h"
#include "Poco/StreamCopier.h"
#include "Poco/ThreadPool.h"
#include "Poco/Timespan.h"
#include "Poco/Types.h"
#include "Poco/URI.h"
#include "CppUnit/Test.h"
#include "CppUnit/TestCaller.h"
#include "CppUnit/TestCase.h"
#include "CppUnit/TestSuite.h"
#include <atomic>
#include <cstddef>
#include <ios>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>


using Poco::OSP::WebEvent::WebEventRequestHandler;
using Poco::OSP::WebEvent::WebEventServiceImpl;
using Poco::OSP::Web::WebServerDispatcher;
using Poco::OSP::Web::WebSessionManager;
using Poco::Net::HTTPResponse;


namespace
{

const std::string READER("reader");
	/// An account that holds every permission.

const std::string OUTSIDER("outsider");
	/// An account that holds no permission.

const std::string ACCOUNT_PASSWORD("account-password");
	/// The password of both accounts.

const std::string READER_TOKEN("reader-token");
	/// The only bearer token the token validator accepts, a token of READER.

const std::string RESTRICTED_PERMISSION("test.events.read");
	/// A permission name to restrict the endpoint to, as osp.web.event.permission allows.

const std::string AUTH_SERVICE_NAME("test.auth");

const std::string TOKEN_VALIDATOR_NAME("test.tokenValidator");

const std::string LOGIN_PATH("/login");

const std::string LOGIN_SESSION("test.login");
	/// The web session the login path logs users in to.

constexpr int SESSION_TIMEOUT_SECONDS = 60;

const std::string TEST_SUBJECT("com.example.webevent.auth");

const std::string TEST_PAYLOAD("{\"value\":1}");

const std::string TEST_NOTIFICATION("NOTIFY " + TEST_SUBJECT + " WebEvent/1.0\r\n" + TEST_PAYLOAD);
	/// The frame a subscriber of TEST_SUBJECT receives for TEST_PAYLOAD.

constexpr int STATUS_UNAUTHORIZED = HTTPResponse::HTTP_UNAUTHORIZED;

constexpr int STATUS_FORBIDDEN = HTTPResponse::HTTP_FORBIDDEN;


class TestAuthService: public Poco::OSP::Auth::AuthService
	/// Knows READER, who holds every permission, and OUTSIDER, who holds none.
{
public:
	[[nodiscard]] bool authenticate(const std::string& userName, const std::string& credentials) const override
	{
		return userExists(userName) && credentials == ACCOUNT_PASSWORD;
	}

	[[nodiscard]] bool authorize(const std::string& userName, const std::string&) const override
	{
		return userName == READER;
	}

	[[nodiscard]] bool authorize(const std::string& userName, const std::string&, const std::string&) const override
	{
		return userName == READER;
	}

	[[nodiscard]] bool userExists(const std::string& userName) const override
	{
		return userName == READER || userName == OUTSIDER;
	}
};


class TestTokenValidator: public Poco::OSP::Web::TokenValidator
	/// Accepts READER_TOKEN, as a token of READER, and no other token.
{
public:
	[[nodiscard]] bool validateToken(const std::string& token, std::string& username, std::string& scope) override
	{
		if (token != READER_TOKEN) return false;
		username = READER;
		scope = "test";
		return true;
	}
};


class LoginRequestHandler: public Poco::Net::HTTPRequestHandler
	/// Opens a web session as the login of a web application does: logged in
	/// as the user the "user" query parameter names or, without one, not
	/// logged in at all.
{
public:
	LoginRequestHandler(WebSessionManager::Ptr pSessionManager, const std::string& sessionName, Poco::OSP::BundleContext::Ptr pContext):
		_pSessionManager(std::move(pSessionManager)),
		_sessionName(sessionName),
		_pContext(std::move(pContext))
	{
	}

	void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) override
	{
		Poco::OSP::Web::WebSession::Ptr pSession = _pSessionManager->create(_sessionName, request, SESSION_TIMEOUT_SECONDS, _pContext);
		for (const auto& [name, value]: Poco::URI(request.getURI()).getQueryParameters())
		{
			if (name == "user") pSession->setValue<std::string>("username", value);
		}
		response.setStatusAndReason(HTTPResponse::HTTP_NO_CONTENT);
		response.send();
	}

private:
	WebSessionManager::Ptr _pSessionManager;
	std::string _sessionName;
	Poco::OSP::BundleContext::Ptr _pContext;
};


class LoginRequestHandlerFactory: public Poco::Net::HTTPRequestHandlerFactory
{
public:
	LoginRequestHandlerFactory(WebSessionManager::Ptr pSessionManager, const std::string& sessionName, Poco::OSP::BundleContext::Ptr pContext):
		_pSessionManager(std::move(pSessionManager)),
		_sessionName(sessionName),
		_pContext(std::move(pContext))
	{
	}

	[[nodiscard]] Poco::Net::HTTPRequestHandler* createRequestHandler(const Poco::Net::HTTPServerRequest&) override
	{
		return new LoginRequestHandler(_pSessionManager, _sessionName, _pContext);
	}

private:
	WebSessionManager::Ptr _pSessionManager;
	std::string _sessionName;
	Poco::OSP::BundleContext::Ptr _pContext;
};


class WebEventHandlerFactory: public Poco::Net::HTTPRequestHandlerFactory
	/// Creates the WebEvent request handler as the bundle's request handler
	/// factory does, without further allowed origins, and counts the
	/// requests the dispatcher lets through.
{
public:
	explicit WebEventHandlerFactory(WebEventServiceImpl::Ptr pService):
		_pService(std::move(pService))
	{
	}

	[[nodiscard]] Poco::Net::HTTPRequestHandler* createRequestHandler(const Poco::Net::HTTPServerRequest&) override
	{
		++_created;
		return new WebEventRequestHandler(_pService);
	}

	[[nodiscard]] int created() const
	{
		return _created;
	}

private:
	WebEventServiceImpl::Ptr _pService;
	std::atomic<int> _created{0};
};


Poco::Path shippedBundleFile(const std::string& name)
	/// Returns the path of a file in the bundle directory of the WebEvent
	/// bundle's sources.
{
	Poco::Path bundleDir(Poco::Path::forDirectory(Poco::Environment::get("POCO_BASE")));
	bundleDir.pushDirectory("WebEvent");
	bundleDir.pushDirectory("bundle");
	return Poco::Path(bundleDir, name);
}


std::string shippedProperties()
	/// Returns the bundle.properties file of the WebEvent bundle.
{
	Poco::FileInputStream stream(shippedBundleFile("bundle.properties").toString());
	std::string properties;
	Poco::StreamCopier::copyToString(stream, properties);
	return properties;
}


class ExtensionMapper: public Poco::OSP::Web::WebServerExtensionPoint
	/// Maps an extension as the web server's extension point does, without
	/// loading the library of its request handler factory.
{
public:
	ExtensionMapper(Poco::OSP::BundleContext::Ptr pContext, WebServerDispatcher* pDispatcher):
		WebServerExtensionPoint(std::move(pContext), pDispatcher)
	{
	}

	[[nodiscard]] WebServerDispatcher::VirtualPath map(
		const Poco::OSP::Bundle::Ptr& pBundle,
		Poco::XML::Element* pElement)
	{
		WebServerDispatcher::VirtualPath vPath;
		handleCommon(pBundle, pElement, vPath);
		return vPath;
	}
};


WebServerDispatcher::VirtualPath shippedExtension(const TemporaryBundle& bundles, WebServerDispatcher& dispatcher)
	/// Returns the virtual path that the web server's extension point maps for
	/// the request handler extension of the WebEvent bundle, read from the
	/// bundle's sources, as an extension of the directory bundle of bundles.
{
	const std::string extensionsPath(shippedBundleFile("extensions.xml").toString());
	Poco::FileInputStream extensionsStream(extensionsPath);
	Poco::XML::InputSource source(extensionsStream);
	Poco::XML::DOMParser parser;
	Poco::AutoPtr<Poco::XML::Document> pDocument = parser.parse(&source);
	Poco::AutoPtr<Poco::XML::NodeList> pExtensions = pDocument->getElementsByTagName("extension");
	for (unsigned long i = 0; i < pExtensions->length(); ++i)
	{
		auto* pElement = static_cast<Poco::XML::Element*>(pExtensions->item(i));
		if (pElement->getAttribute("point") == "osp.web.server.requesthandler" && pElement->getAttribute("class") == "Poco::OSP::WebEvent::WebEventRequestHandlerFactory")
		{
			Poco::AutoPtr<ExtensionMapper> pMapper(new ExtensionMapper(bundles.context(), &dispatcher));
			return pMapper->map(bundles.bundle(), pElement);
		}
	}
	throw Poco::NotFoundException("WebEvent request handler extension", extensionsPath);
}


class Endpoint
	/// The WebEvent endpoint as the web server serves it, on a loopback port:
	/// a WebServerDispatcher that maps the bundle's request handler extension
	/// as the web server's extension point does, with the accounts of
	/// TestAuthService, the token of TestTokenValidator, a web session manager,
	/// and a login path that opens web sessions.
{
public:
	explicit Endpoint(const std::string& session = std::string(), const std::string& permission = std::string());
		/// Creates the endpoint, with the given web session and permission as
		/// osp.web.event.session and osp.web.event.permission in the
		/// application configuration, if they are given. The login path logs
		/// users in to LOGIN_SESSION.

	~Endpoint();
		/// Stops the service and the server.

	Endpoint(const Endpoint&) = delete;
	Endpoint(Endpoint&&) = delete;
	Endpoint& operator = (const Endpoint&) = delete;
	Endpoint& operator = (Endpoint&&) = delete;

	[[nodiscard]] WebEventServiceImpl& service();
		/// Returns the WebEvent service behind the endpoint.

	[[nodiscard]] const WebServerDispatcher::VirtualPath& webEventPath() const;
		/// Returns the virtual path of the WebEvent endpoint, as the web
		/// server's extension point maps the bundle's extension.

	[[nodiscard]] int handlersCreated() const;
		/// Returns the number of requests the dispatcher has let through to
		/// the WebEvent request handler.

	[[nodiscard]] std::string origin() const;
		/// Returns the origin of the endpoint, as a browser sends it for a
		/// page of the server.

	[[nodiscard]] Poco::Net::HTTPRequest upgradeRequest(const std::string& suffix = std::string()) const;
		/// Returns a request for the endpoint's path followed by suffix.

	[[nodiscard]] Poco::Net::NameValueCollection login(const std::string& user = std::string());
		/// Opens a web session at the login path, logged in as user or, if user
		/// is empty, not logged in, and returns the cookies the response sets.

	[[nodiscard]] Poco::Net::WebSocket upgrade(Poco::Net::HTTPRequest& request);
		/// Performs the WebSocket handshake with the request. Throws a
		/// Poco::Net::WebSocketException if the server refuses it.

	[[nodiscard]] int upgradeStatus(Poco::Net::HTTPRequest& request);
		/// Performs the WebSocket handshake with the request and returns the
		/// status the server answers with: 101 if it upgrades the connection,
		/// which is closed again then.

	[[nodiscard]] int upgradeStatus(Poco::Net::HTTPRequest& request, HTTPResponse& response);
		/// Performs the WebSocket handshake with the request as above, and
		/// also returns the response in response.

private:
	// Declared first, so that the services registered with its registry
	// and the bundle context outlive everything below.
	TemporaryBundle _bundles;
	WebServerDispatcher::VirtualPath _webEventPath;
	WebSessionManager::Ptr _pSessionManager;
	WebEventServiceImpl::Ptr _pService;
	Poco::SharedPtr<WebEventHandlerFactory> _pHandlerFactory;
	WebServerDispatcher::Ptr _pDispatcher;
	Poco::ThreadPool _pool;
	Poco::UInt16 _port = 0;
	std::unique_ptr<Poco::Net::HTTPServer> _pServer;
};


Endpoint::Endpoint(const std::string& session, const std::string& permission):
	_bundles(shippedProperties()),
	_pSessionManager(new WebSessionManager(_bundles.context()))
{
	WebEventServiceImpl::Config config;
	config.workerCount = 1;
	_pService = new WebEventServiceImpl(_bundles.context(), config);
	_pHandlerFactory = new WebEventHandlerFactory(_pService);

	Poco::AutoPtr<Poco::Util::MapConfiguration> pSettings = new Poco::Util::MapConfiguration;
	if (!session.empty()) pSettings->setString("osp.web.event.session", session);
	if (!permission.empty()) pSettings->setString("osp.web.event.permission", permission);
	_bundles.registry().registerService(Poco::OSP::PreferencesService::SERVICE_NAME,
		new Poco::OSP::PreferencesService(_bundles.context()->persistentDirectory(), pSettings),
		Poco::OSP::Properties());
	_bundles.registry().registerService(AUTH_SERVICE_NAME, new TestAuthService, Poco::OSP::Properties());
	_bundles.registry().registerService(TOKEN_VALIDATOR_NAME, new TestTokenValidator, Poco::OSP::Properties());
	_bundles.registry().registerService(WebSessionManager::SERVICE_NAME, _pSessionManager, Poco::OSP::Properties());

	WebServerDispatcher::Config dispatcherConfig;
	dispatcherConfig.pContext = _bundles.context();
	dispatcherConfig.authServiceName = AUTH_SERVICE_NAME;
	dispatcherConfig.tokenValidatorName = TOKEN_VALIDATOR_NAME;
	_pDispatcher = new WebServerDispatcher(dispatcherConfig);

	_webEventPath = shippedExtension(_bundles, *_pDispatcher);
	_webEventPath.pFactory = _pHandlerFactory;
	_pDispatcher->addVirtualPath(_webEventPath);

	WebServerDispatcher::VirtualPath loginPath;
	loginPath.path = LOGIN_PATH;
	loginPath.pFactory = new LoginRequestHandlerFactory(_pSessionManager, LOGIN_SESSION, _bundles.context());
	loginPath.security.mode = WebServerDispatcher::SM_NONE;
	loginPath.hidden = true;
	loginPath.pOwnerBundle = _bundles.bundle();
	_pDispatcher->addVirtualPath(loginPath);

	Poco::Net::ServerSocket serverSocket(Poco::Net::SocketAddress("127.0.0.1", 0));
	_port = serverSocket.address().port();
	_pServer = std::make_unique<Poco::Net::HTTPServer>(new Poco::OSP::Web::WebServerRequestHandlerFactory(*_pDispatcher, false),
		_pool, serverSocket, new Poco::Net::HTTPServerParams);
	_pServer->start();
}


Endpoint::~Endpoint()
{
	try
	{
		_pService->stop();
		_pServer->stopAll(true);
		_pServer->stop();
		_pool.joinAll();
	}
	catch (...)
	{
		poco_unexpected();
	}
}


WebEventServiceImpl& Endpoint::service()
{
	return *_pService;
}


const WebServerDispatcher::VirtualPath& Endpoint::webEventPath() const
{
	return _webEventPath;
}


int Endpoint::handlersCreated() const
{
	return _pHandlerFactory->created();
}


std::string Endpoint::origin() const
{
	return "http://127.0.0.1:" + std::to_string(_port);
}


Poco::Net::HTTPRequest Endpoint::upgradeRequest(const std::string& suffix) const
{
	return Poco::Net::HTTPRequest(Poco::Net::HTTPRequest::HTTP_GET, _webEventPath.path + suffix,
		Poco::Net::HTTPMessage::HTTP_1_1);
}


Poco::Net::NameValueCollection Endpoint::login(const std::string& user)
{
	Poco::URI uri(LOGIN_PATH);
	if (!user.empty()) uri.addQueryParameter("user", user);
	Poco::Net::HTTPClientSession session("127.0.0.1", _port);
	Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, uri.getPathAndQuery(), Poco::Net::HTTPMessage::HTTP_1_1);
	session.sendRequest(request);
	HTTPResponse response;
	session.receiveResponse(response).ignore(std::numeric_limits<std::streamsize>::max());
	std::vector<Poco::Net::HTTPCookie> cookies;
	response.getCookies(cookies);
	Poco::Net::NameValueCollection result;
	for (const auto& cookie: cookies)
	{
		result.set(cookie.getName(), cookie.getValue());
	}
	return result;
}


Poco::Net::WebSocket Endpoint::upgrade(Poco::Net::HTTPRequest& request)
{
	Poco::Net::HTTPClientSession session("127.0.0.1", _port);
	HTTPResponse response;
	return Poco::Net::WebSocket(session, request, response);
}


int Endpoint::upgradeStatus(Poco::Net::HTTPRequest& request)
{
	HTTPResponse response;
	return upgradeStatus(request, response);
}


int Endpoint::upgradeStatus(Poco::Net::HTTPRequest& request, HTTPResponse& response)
{
	Poco::Net::HTTPClientSession session("127.0.0.1", _port);
	try
	{
		Poco::Net::WebSocket ws(session, request, response);
	}
	catch (Poco::Net::WebSocketException&)
	{
	}
	return response.getStatus();
}


class SubscriptionWaiter
	/// Takes note of the subscriptions the service receives while it exists.
{
public:
	explicit SubscriptionWaiter(WebEventServiceImpl& service):
		_service(service)
	{
		_service.subscriptionRequestReceived += Poco::delegate(this, &SubscriptionWaiter::onSubscription);
	}

	~SubscriptionWaiter()
	{
		try
		{
			_service.subscriptionRequestReceived -= Poco::delegate(this, &SubscriptionWaiter::onSubscription);
		}
		catch (...)
		{
			poco_unexpected();
		}
	}

	SubscriptionWaiter(const SubscriptionWaiter&) = delete;
	SubscriptionWaiter(SubscriptionWaiter&&) = delete;
	SubscriptionWaiter& operator = (const SubscriptionWaiter&) = delete;
	SubscriptionWaiter& operator = (SubscriptionWaiter&&) = delete;

	[[nodiscard]] bool wait(long milliseconds)
		/// Waits for a subscription; returns false if none arrives in time.
	{
		return _event.tryWait(milliseconds);
	}

private:
	void onSubscription(const std::string&)
	{
		_event.set();
	}

	WebEventServiceImpl& _service;
	Poco::Event _event;
};


std::string receiveFrame(Poco::Net::WebSocket& ws)
	/// Returns the next frame the connection receives.
{
	ws.setReceiveTimeout(Poco::Timespan(5, 0));
	std::vector<char> buffer(1024);
	int flags = 0;
	const int received = ws.receiveFrame(buffer.data(), static_cast<int>(buffer.size()), flags);
	return std::string(buffer.data(), received > 0 ? static_cast<std::size_t>(received) : 0);
}


std::string subscribeAndReceive(WebEventServiceImpl& service, Poco::Net::WebSocket& ws)
	/// Subscribes the connection to TEST_SUBJECT, notifies TEST_PAYLOAD for it
	/// once the service has the subscription, and returns the frame the
	/// connection receives; empty if the subscription does not arrive in time.
{
	SubscriptionWaiter waiter(service);
	const std::string subscribe("SUBSCRIBE " + TEST_SUBJECT + " WebEvent/1.0");
	ws.sendFrame(subscribe.data(), static_cast<int>(subscribe.size()), Poco::Net::WebSocket::FRAME_TEXT);
	if (!waiter.wait(5000)) return std::string();

	service.notify(TEST_SUBJECT, TEST_PAYLOAD);
	return receiveFrame(ws);
}


} // namespace


WebEventRequestHandlerTest::WebEventRequestHandlerTest(const std::string& name):
	CppUnit::TestCase(name)
{
}


WebEventRequestHandlerTest::~WebEventRequestHandlerTest() = default;


void WebEventRequestHandlerTest::testExtensionRequiresAuthentication()
{
	Endpoint endpoint;
	const WebServerDispatcher::VirtualPath& extension = endpoint.webEventPath();
	const int authMethods = extension.security.authMethods;

	assertEqual("/webevent", extension.path);
	// "**" lets requests without credentials through, as an empty permission does.
	assertFalse(extension.security.permission.empty());
	assertTrue(extension.security.permission != "**");
	assertTrue(extension.security.permission.find("${") == std::string::npos);
	// No session login by default: an application names the session of its login.
	assertTrue(extension.security.session.empty());
	assertTrue(authMethods == 0 || (authMethods & WebServerDispatcher::AUTH_SESSION) != 0);
}


void WebEventRequestHandlerTest::testExtensionTakesSettingsFromConfiguration()
	/// osp.web.event.session and osp.web.event.permission in the application
	/// configuration replace the defaults of the bundle's extension.
{
	Endpoint endpoint(LOGIN_SESSION, RESTRICTED_PERMISSION);

	assertEqual(LOGIN_SESSION, endpoint.webEventPath().security.session);
	assertEqual(RESTRICTED_PERMISSION, endpoint.webEventPath().security.permission);
}


void WebEventRequestHandlerTest::testUnauthenticatedUpgradeIsRefused()
{
	Endpoint endpoint;

	// as browser pages and non-browser clients address the endpoint
	for (const std::string& suffix: {std::string(), std::string("/"), std::string("?page=%2Fapp")})
	{
		Poco::Net::HTTPRequest request = endpoint.upgradeRequest(suffix);
		assertEqual(STATUS_UNAUTHORIZED, endpoint.upgradeStatus(request));
	}
	assertEqual(0, endpoint.handlersCreated());
	assertTrue(endpoint.service().listSubscribers().empty());
}


void WebEventRequestHandlerTest::testUnauthorizedUpgradeIsRefused()
{
	Endpoint endpoint(LOGIN_SESSION);

	Poco::Net::HTTPRequest wrongPassword = endpoint.upgradeRequest();
	Poco::Net::HTTPBasicCredentials(READER, "wrong-password").authenticate(wrongPassword);
	assertEqual(STATUS_UNAUTHORIZED, endpoint.upgradeStatus(wrongPassword));

	Poco::Net::HTTPRequest unknownAccount = endpoint.upgradeRequest();
	Poco::Net::HTTPBasicCredentials("nobody", ACCOUNT_PASSWORD).authenticate(unknownAccount);
	assertEqual(STATUS_UNAUTHORIZED, endpoint.upgradeStatus(unknownAccount));

	Poco::Net::HTTPRequest refusedToken = endpoint.upgradeRequest();
	refusedToken.setCredentials("Bearer", "refused-token");
	assertEqual(STATUS_UNAUTHORIZED, endpoint.upgradeStatus(refusedToken));

	const Poco::Net::NameValueCollection notLoggedIn = endpoint.login();
	assertFalse(notLoggedIn.empty());
	Poco::Net::HTTPRequest sessionWithoutUser = endpoint.upgradeRequest();
	sessionWithoutUser.setCookies(notLoggedIn);
	assertEqual(STATUS_UNAUTHORIZED, endpoint.upgradeStatus(sessionWithoutUser));

	const Poco::Net::NameValueCollection removedAccount = endpoint.login("removed");
	assertFalse(removedAccount.empty());
	Poco::Net::HTTPRequest sessionOfRemovedAccount = endpoint.upgradeRequest();
	sessionOfRemovedAccount.setCookies(removedAccount);
	assertEqual(STATUS_UNAUTHORIZED, endpoint.upgradeStatus(sessionOfRemovedAccount));

	Poco::Net::NameValueCollection forged;
	for (const auto& [name, value]: endpoint.login(READER))
	{
		forged.set(name, std::string(value.size(), '0'));
	}
	assertFalse(forged.empty());
	Poco::Net::HTTPRequest forgedSession = endpoint.upgradeRequest();
	forgedSession.setCookies(forged);
	assertEqual(STATUS_UNAUTHORIZED, endpoint.upgradeStatus(forgedSession));

	assertEqual(0, endpoint.handlersCreated());
	assertTrue(endpoint.service().listSubscribers().empty());
}


void WebEventRequestHandlerTest::testUpgradeWithoutPermissionIsRefused()
	/// An account without the permission is refused with 403: it has been
	/// authenticated, and authenticating again would not help.
{
	Endpoint endpoint(LOGIN_SESSION, RESTRICTED_PERMISSION);

	Poco::Net::HTTPRequest outsiderCredentials = endpoint.upgradeRequest("/");
	Poco::Net::HTTPBasicCredentials(OUTSIDER, ACCOUNT_PASSWORD).authenticate(outsiderCredentials);
	assertEqual(STATUS_FORBIDDEN, endpoint.upgradeStatus(outsiderCredentials));

	const Poco::Net::NameValueCollection outsiderCookies = endpoint.login(OUTSIDER);
	assertFalse(outsiderCookies.empty());
	Poco::Net::HTTPRequest outsiderSession = endpoint.upgradeRequest("?page=%2Fapp");
	outsiderSession.setCookies(outsiderCookies);
	assertEqual(STATUS_FORBIDDEN, endpoint.upgradeStatus(outsiderSession));

	assertEqual(0, endpoint.handlersCreated());
	assertTrue(endpoint.service().listSubscribers().empty());

	Poco::Net::HTTPRequest readerCredentials = endpoint.upgradeRequest("/");
	Poco::Net::HTTPBasicCredentials(READER, ACCOUNT_PASSWORD).authenticate(readerCredentials);
	Poco::Net::WebSocket ws = endpoint.upgrade(readerCredentials);
	assertEqual(1, endpoint.handlersCreated());
	assertEqual(TEST_NOTIFICATION, subscribeAndReceive(endpoint.service(), ws));
}


void WebEventRequestHandlerTest::testUpgradeWithSessionCookieIsAdmitted()
{
	Endpoint endpoint(LOGIN_SESSION);

	const Poco::Net::NameValueCollection cookies = endpoint.login(READER);
	assertFalse(cookies.empty());
	Poco::Net::HTTPRequest request = endpoint.upgradeRequest("?page=%2Fapp");
	request.setCookies(cookies);
	Poco::Net::WebSocket ws = endpoint.upgrade(request);
	assertEqual(1, endpoint.handlersCreated());
	assertEqual(TEST_NOTIFICATION, subscribeAndReceive(endpoint.service(), ws));
}


void WebEventRequestHandlerTest::testUpgradeWithSessionCookieFromForeignOriginIsRefused()
	/// A browser sends the session cookie with an upgrade that a page of any
	/// site asks for, so a valid cookie admits the upgrade only from a page
	/// of the server itself.
{
	Endpoint endpoint(LOGIN_SESSION);

	const Poco::Net::NameValueCollection cookies = endpoint.login(READER);
	assertFalse(cookies.empty());

	Poco::Net::HTTPRequest foreign = endpoint.upgradeRequest("?page=%2Fapp");
	foreign.setCookies(cookies);
	foreign.set("Origin", "http://attacker.example");
	assertEqual(STATUS_FORBIDDEN, endpoint.upgradeStatus(foreign));
	assertTrue(endpoint.service().listSubscribers().empty());

	Poco::Net::HTTPRequest own = endpoint.upgradeRequest("?page=%2Fapp");
	own.setCookies(cookies);
	own.set("Origin", endpoint.origin());
	Poco::Net::WebSocket ws = endpoint.upgrade(own);
	assertEqual(TEST_NOTIFICATION, subscribeAndReceive(endpoint.service(), ws));
}


void WebEventRequestHandlerTest::testUpgradeWithoutWebSessionIgnoresCookie()
	/// Without osp.web.event.session, as the bundle ships, the cookie of a logged-in
	/// session admits nothing; credentials do.
{
	Endpoint endpoint;

	const Poco::Net::NameValueCollection cookies = endpoint.login(READER);
	assertFalse(cookies.empty());
	Poco::Net::HTTPRequest cookieOnly = endpoint.upgradeRequest("?page=%2Fapp");
	cookieOnly.setCookies(cookies);
	HTTPResponse cookieOnlyResponse;
	assertEqual(STATUS_UNAUTHORIZED, endpoint.upgradeStatus(cookieOnly, cookieOnlyResponse));
	assertTrue(cookieOnlyResponse.get("WWW-Authenticate", "").starts_with("Basic "));
	assertEqual(0, endpoint.handlersCreated());

	Poco::Net::HTTPRequest credentials = endpoint.upgradeRequest("?page=%2Fapp");
	credentials.setCookies(cookies);
	Poco::Net::HTTPBasicCredentials(READER, ACCOUNT_PASSWORD).authenticate(credentials);
	Poco::Net::WebSocket ws = endpoint.upgrade(credentials);
	assertEqual(1, endpoint.handlersCreated());
	assertEqual(TEST_NOTIFICATION, subscribeAndReceive(endpoint.service(), ws));
}


void WebEventRequestHandlerTest::testUpgradeWithBasicCredentialsIsAdmitted()
{
	Endpoint endpoint;

	Poco::Net::HTTPRequest request = endpoint.upgradeRequest("/");
	Poco::Net::HTTPBasicCredentials(READER, ACCOUNT_PASSWORD).authenticate(request);
	Poco::Net::WebSocket ws = endpoint.upgrade(request);
	assertEqual(1, endpoint.handlersCreated());
	assertEqual(TEST_NOTIFICATION, subscribeAndReceive(endpoint.service(), ws));
}


void WebEventRequestHandlerTest::testUpgradeWithBearerTokenIsAdmitted()
{
	Endpoint endpoint;

	Poco::Net::HTTPRequest request = endpoint.upgradeRequest("/");
	request.setCredentials("Bearer", READER_TOKEN);
	Poco::Net::WebSocket ws = endpoint.upgrade(request);
	assertEqual(1, endpoint.handlersCreated());
	assertEqual(TEST_NOTIFICATION, subscribeAndReceive(endpoint.service(), ws));
}


void WebEventRequestHandlerTest::testConnectionBelongsToAuthenticatedUser()
	/// Without a user resolver, a connection belongs to the user that the web
	/// server has authenticated for its upgrade request: the subject
	/// authorizer is asked with that user, and notifyUser() reaches it.
{
	Endpoint endpoint(LOGIN_SESSION);
	endpoint.service().setSubjectAuthorizer(
		[](const std::string& username, const std::string&, WebEventServiceImpl::SubjectAction)
		{
			return username == READER;
		});

	const Poco::Net::NameValueCollection cookies = endpoint.login(READER);
	assertFalse(cookies.empty());
	Poco::Net::HTTPRequest request = endpoint.upgradeRequest("?page=%2Fapp");
	request.setCookies(cookies);
	Poco::Net::WebSocket ws = endpoint.upgrade(request);
	assertEqual(TEST_NOTIFICATION, subscribeAndReceive(endpoint.service(), ws));

	const WebEventServiceImpl::SubscriberInfoVec subscribers = endpoint.service().listSubscribers();
	assertEqual(1, static_cast<int>(subscribers.size()));
	assertEqual(READER, subscribers[0].username);

	endpoint.service().notifyUser(READER, TEST_SUBJECT, TEST_PAYLOAD);
	assertEqual(TEST_NOTIFICATION, receiveFrame(ws));
}


void WebEventRequestHandlerTest::testUserResolverReplacesAuthenticatedUser()
{
	const std::string resolvedUser("test.resolved");
	Endpoint endpoint;
	endpoint.service().setUserResolver(
		[&resolvedUser](const Poco::Net::HTTPServerRequest&)
		{
			return resolvedUser;
		});

	Poco::Net::HTTPRequest request = endpoint.upgradeRequest("/");
	Poco::Net::HTTPBasicCredentials(READER, ACCOUNT_PASSWORD).authenticate(request);
	Poco::Net::WebSocket ws = endpoint.upgrade(request);
	assertEqual(TEST_NOTIFICATION, subscribeAndReceive(endpoint.service(), ws));

	const WebEventServiceImpl::SubscriberInfoVec subscribers = endpoint.service().listSubscribers();
	assertEqual(1, static_cast<int>(subscribers.size()));
	assertEqual(resolvedUser, subscribers[0].username);
}


void WebEventRequestHandlerTest::setUp()
{
}


void WebEventRequestHandlerTest::tearDown()
{
}


CppUnit::Test* WebEventRequestHandlerTest::suite()
{
	CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("WebEventRequestHandlerTest");

	CppUnit_addTest(pSuite, WebEventRequestHandlerTest, testExtensionRequiresAuthentication);
	CppUnit_addTest(pSuite, WebEventRequestHandlerTest, testExtensionTakesSettingsFromConfiguration);
	CppUnit_addTest(pSuite, WebEventRequestHandlerTest, testUnauthenticatedUpgradeIsRefused);
	CppUnit_addTest(pSuite, WebEventRequestHandlerTest, testUnauthorizedUpgradeIsRefused);
	CppUnit_addTest(pSuite, WebEventRequestHandlerTest, testUpgradeWithoutPermissionIsRefused);
	CppUnit_addTest(pSuite, WebEventRequestHandlerTest, testUpgradeWithSessionCookieIsAdmitted);
	CppUnit_addTest(pSuite, WebEventRequestHandlerTest, testUpgradeWithSessionCookieFromForeignOriginIsRefused);
	CppUnit_addTest(pSuite, WebEventRequestHandlerTest, testUpgradeWithoutWebSessionIgnoresCookie);
	CppUnit_addTest(pSuite, WebEventRequestHandlerTest, testUpgradeWithBasicCredentialsIsAdmitted);
	CppUnit_addTest(pSuite, WebEventRequestHandlerTest, testUpgradeWithBearerTokenIsAdmitted);
	CppUnit_addTest(pSuite, WebEventRequestHandlerTest, testConnectionBelongsToAuthenticatedUser);
	CppUnit_addTest(pSuite, WebEventRequestHandlerTest, testUserResolverReplacesAuthenticatedUser);

	return pSuite;
}
