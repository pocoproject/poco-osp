//
// WebSessionManager.cpp
//
// Library: OSP/Web
// Package: Web
// Module:  WebSessionManager
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#include "Poco/OSP/Web/WebSessionManager.h"
#include "WebSessionRegistry.h"
#include "Poco/OSP/BundleEvent.h"
#include "Poco/OSP/BundleEvents.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Net/HTTPServerResponse.h"
#include "Poco/Net/NameValueCollection.h"
#include "Poco/StringTokenizer.h"
#include "Poco/NumberParser.h"
#include "Poco/NumberFormatter.h"
#include "Poco/SHA1Engine.h"
#include "Poco/RandomStream.h"
#include "Poco/Exception.h"
#include "Poco/Timestamp.h"
#include "Poco/Delegate.h"
#include "Poco/Bugcheck.h"
#include <memory>
#include <vector>
#include <string>
#include <utility>


using Poco::Net::NameValueCollection;
using Poco::FastMutex;
using Poco::NumberFormatter;
using namespace std::string_literals;


namespace Poco::OSP::Web {


const std::string WebSessionManager::COOKIE_NAME("osp.web.session");
const std::string WebSessionManager::SERVICE_NAME("osp.web.session");


WebSessionManager::WebSessionManager(Poco::OSP::BundleContext::Ptr pContext):
	_pContext(pContext),
	_pSessionRegistry(std::make_shared<WebSessionRegistry>()),
	_logger(Poco::Logger::get("osp.web.session"))
{
	_pContext->events().bundleStopping += Poco::delegate(this, &WebSessionManager::onBundleStopping);
}


WebSessionManager::~WebSessionManager()
{
	try
	{
		_pContext->events().bundleStopping -= Poco::delegate(this, &WebSessionManager::onBundleStopping);
	}
	catch (...)
	{
		poco_unexpected();
	}
}


void WebSessionManager::setDefaultDomain(const std::string& domain)
{
	_defaultDomain = domain;
}


const std::string& WebSessionManager::getDefaultDomain() const
{
	return _defaultDomain;
}


void WebSessionManager::setDefaultPath(const std::string& path)
{
	_defaultPath = path;
}


const std::string& WebSessionManager::getDefaultPath() const
{
	return _defaultPath;
}


void WebSessionManager::setCookiePersistence(WebSessionManager::CookiePersistence persistence)
{
	_cookiePersistence = persistence;
}


WebSessionManager::CookiePersistence WebSessionManager::getCookiePersistence() const
{
	return _cookiePersistence;
}


void WebSessionManager::setCookieSameSite(Poco::Net::HTTPCookie::SameSite sameSite)
{
	_cookieSameSite = sameSite;
}


Poco::Net::HTTPCookie::SameSite WebSessionManager::getCookieSameSite() const
{
	return _cookieSameSite;
}


Poco::Net::HTTPCookie::SameSite WebSessionManager::setCookieSameSite() const
{
	return getCookieSameSite();
}


void WebSessionManager::setCookieSecure(bool secure)
{
	_cookieSecure = secure;
}


bool WebSessionManager::isCookieSecure() const
{
	return _cookieSecure;
}


void WebSessionManager::setVerifyAddress(bool verify)
{
	_verifyAddress = verify;
}


bool WebSessionManager::isAddressVerified() const
{
	return _verifyAddress;
}


void WebSessionManager::setCSRFCookie(const std::string& name)
{
	_csrfCookie = name;
}


const std::string& WebSessionManager::getCSRFCookie() const
{
	return _csrfCookie;
}


void WebSessionManager::setSessionStore(WebSessionStore::Ptr pWebSessionStore)
{
	FastMutex::ScopedLock lock(_mutex);

	_pStore = pWebSessionStore;
}


WebSessionStore::Ptr WebSessionManager::getSessionStore() const
{
	FastMutex::ScopedLock lock(_mutex);

	return _pStore;
}


const std::type_info& WebSessionManager::type() const
{
	return typeid(WebSessionManager);
}


bool WebSessionManager::isA(const std::type_info& otherType) const
{
	std::string name(typeid(WebSessionManager).name());
	return name == otherType.name() || WebSessionService<Poco::Net::HTTPServerRequest>::isA(otherType);
}


WebSession::Ptr WebSessionManager::findById(const std::string& sessionId)
{
	FastMutex::ScopedLock lock(_mutex);

	return findByIdImpl(sessionId, _pContext);
}


std::size_t WebSessionManager::countSessions()
{
	FastMutex::ScopedLock lock(_mutex);

	if (_pStore != nullptr)
	{
		return _pStore->countSessions();
	}
	else
	{
		return _cache.size();
	}
}


void WebSessionManager::forEachSession(const std::function<void(const std::string& sessionId, const WebSession& session)>& fn)
{
	FastMutex::ScopedLock lock(_mutex);

	_cache.forEach(fn);
}


void WebSessionManager::updateSessionTimeout(const std::string& appName, WebSession::Ptr pSession, int timeout, const Poco::Net::HTTPServerRequest& request)
{
	pSession->timeout(timeout);

	// createCSRFCookie() may create a token, which needs the lock.
	FastMutex::ScopedLock lock(_mutex);
	request.response().replaceCookie(createSessionCookie(appName, request, pSession));
	if (!_csrfCookie.empty())
	{
		request.response().replaceCookie(createCSRFCookie(appName, request, pSession));
	}
}


WebSession::Ptr WebSessionManager::renew(const std::string& appName, const Poco::Net::HTTPServerRequest& request, WebSession::Ptr pSession)
{
	FastMutex::ScopedLock lock(_mutex);

	WebSession::Attributes attrs = pSession->attributes();
	attrs[WebSession::CSRF_TOKEN] = createToken(request);
	WebSession::Ptr pNewSession(new WebSession(createToken(request), 1, pSession->timeout(), request.clientAddress().host(), _pStore, pSession->_pContext, Poco::Timestamp(), attrs));
	pNewSession->registerWith(_pSessionRegistry);
	_cache.add(pNewSession->id(), pNewSession);
	if (_pStore != nullptr)
	{
		_pStore->saveSession(pNewSession);
	}
	removeImpl(pSession);
	request.response().replaceCookie(createSessionCookie(appName, request, pNewSession));
	if (!_csrfCookie.empty())
	{
		request.response().replaceCookie(createCSRFCookie(appName, request, pNewSession));
	}
	if (_logger.debug())
	{
		_logger.debug("Session %s renewed as %s."s, pSession->id(), pNewSession->id());
	}
	return pNewSession;
}


WebSession::Ptr WebSessionManager::find(const std::string& appName, const Poco::Net::HTTPServerRequest& request)
{
	FastMutex::ScopedLock lock(_mutex);

	return findImpl(appName, request, _pContext);
}


WebSession::Ptr WebSessionManager::get(const std::string& appName, const Poco::Net::HTTPServerRequest& request, int expireSeconds, BundleContext::Ptr pContext)
{
	FastMutex::ScopedLock lock(_mutex);

	WebSession::Ptr pSession = findImpl(appName, request, pContext);
	if (pSession == nullptr)
	{
		_logger.debug("No existing session, creating new one."s);
		pSession = createImpl(appName, request, expireSeconds, pContext);
	}
	return pSession;
}


WebSession::Ptr WebSessionManager::create(const std::string& appName, const Poco::Net::HTTPServerRequest& request, int expireSeconds, BundleContext::Ptr pContext)
{
	FastMutex::ScopedLock lock(_mutex);

	return createImpl(appName, request, expireSeconds, pContext);
}


void WebSessionManager::remove(WebSession::Ptr pSession)
{
	FastMutex::ScopedLock lock(_mutex);

	removeImpl(pSession);
}


void WebSessionManager::removeForUser(const std::string& username)
{
	FastMutex::ScopedLock lock(_mutex);

	std::vector<std::string> sessionsToRemove;

	if (_pStore != nullptr)
	{
		sessionsToRemove = _pStore->sessionsByValue("username"s, username);
	}
	else
	{
		_cache.forEach(
			[&sessionsToRemove, &username](const std::string& id, const WebSession& session)
			{
				try
				{
					if (session.getValue<std::string>("username"s) == username)
					{
						sessionsToRemove.push_back(id);
					}
				}
				catch (Poco::NotFoundException&)
				{
					// A session without a username belongs to no user.
				}
				catch (Poco::BadCastException&)
				{
					// A username that is not a std::string matches no user.
				}
			}
		);
	}

	for (const auto& id: sessionsToRemove)
	{
		removeImpl(id);
	}
}


void WebSessionManager::onBundleStopping(const void*, BundleEvent& ev)
{
	// The attributes are destroyed after the registry is unlocked, so that destroying
	// them can release sessions. The libraries of the bundle that can have created them
	// are still loaded: the WebServerDispatcher releases them when the bundle has stopped.
	std::vector<WebSession::Attributes> released;
	_pSessionRegistry->forEach([&released, &ev](WebSession& session)
		{
			released.push_back(session.releaseAttributes(ev.bundle()));
		});
}


WebSession::Ptr WebSessionManager::findImpl(const std::string& appName, const Poco::Net::HTTPServerRequest& request, BundleContext::Ptr pContext)
{
	WebSession::Ptr pSession;
	std::string sessionId(getId(appName, request));
	if (!sessionId.empty())
	{
		if (_logger.debug())
		{
			_logger.debug("Looking up session %s..."s, sessionId);
		}
		pSession = findByIdImpl(sessionId, pContext);
	}
	else
	{
		_logger.debug("No session cookie found in request."s);
	}
	if (pSession != nullptr)
	{
		if (_logger.debug())
		{
			_logger.debug("Existing session %s found."s, sessionId);
		}
		if (!_verifyAddress || pSession->clientAddress() == request.clientAddress().host())
		{
			pSession->access();
			_cache.add(pSession->id(), pSession);
			addSessionCookie(appName, request, pSession);
			addCSRFCookie(appName, request, pSession);
		}
		else
		{
			_logger.notice("Client address mismatch for a session of %s (known address: %s, request address: %s). The session is removed."s,
				cookieName(appName), pSession->clientAddress().toString(), request.clientAddress().host().toString());

			// possible attack: same session ID from different host - invalidate session
			removeImpl(pSession);
			return nullptr;
		}
	}
	return pSession;
}


WebSession::Ptr WebSessionManager::findByIdImpl(const std::string& sessionId, BundleContext::Ptr pContext)
{
	WebSession::Ptr pSession = _cache.get(sessionId);
	if (_logger.debug() && pSession != nullptr)
	{
		_logger.debug("Session %s found in cache."s, sessionId);
	}
	if (_pStore != nullptr)
	{
		if (_logger.debug())
		{
			_logger.debug("Looking up session %s in session store..."s, sessionId);
		}
		if (pSession != nullptr)
		{
			std::pair<WebSession::Ptr, bool> result = _pStore->loadSession(pContext, sessionId, pSession->version());
			if (result.first != nullptr && result.second)
				pSession = result.first;
			else if (!result.second)
				pSession.reset();
		}
		else
		{
			std::pair<WebSession::Ptr, bool> result = _pStore->loadSession(pContext, sessionId);
			if (result.second) pSession = result.first;
		}
		if (pSession != nullptr) pSession->registerWith(_pSessionRegistry);
	}
	return pSession;
}


WebSession::Ptr WebSessionManager::createImpl(const std::string& appName, const Poco::Net::HTTPServerRequest& request, int expireSeconds, BundleContext::Ptr pContext)
{
	WebSession::Ptr pSession(new WebSession(createToken(request), createToken(request), 1, expireSeconds, request.clientAddress().host(), _pStore, pContext));
	pSession->registerWith(_pSessionRegistry);
	_cache.add(pSession->id(), pSession);
	addSessionCookie(appName, request, pSession);
	addCSRFCookie(appName, request, pSession);
	if (_pStore != nullptr)
	{
		_pStore->saveSession(pSession);
	}
	if (_logger.debug())
	{
		_logger.debug("New session %s created."s, pSession->id());
	}
	return pSession;
}


void WebSessionManager::removeImpl(WebSession::Ptr pSession)
{
	removeImpl(pSession->id());
}


void WebSessionManager::removeImpl(const std::string& id)
{
	if (_logger.debug())
	{
		_logger.debug("Removing session %s."s, id);
	}
	_cache.remove(id);
	if (_pStore != nullptr)
	{
		_pStore->expireSession(id, 0);
	}
}


std::string WebSessionManager::getId(const std::string& appName, const Poco::Net::HTTPServerRequest& request)
{
	std::string id;
	std::string name(cookieName(appName));
	NameValueCollection cookies;
	request.getCookies(cookies);
	NameValueCollection::ConstIterator it = cookies.find(name);
	if (it != cookies.end())
	{
		id = it->second;
	}
	return id;
}


void WebSessionManager::addSessionCookie(const std::string& appName, const Poco::Net::HTTPServerRequest& request, WebSession::Ptr pSession)
{
	request.response().replaceCookie(createSessionCookie(appName, request, std::move(pSession)));
}


void WebSessionManager::addCSRFCookie(const std::string& appName, const Poco::Net::HTTPServerRequest& request, WebSession::Ptr pSession)
{
	if (!_csrfCookie.empty())
	{
		request.response().replaceCookie(createCSRFCookie(appName, request, std::move(pSession)));
	}
}


Poco::Net::HTTPCookie WebSessionManager::createSessionCookie(const std::string& appName, const Poco::Net::HTTPServerRequest& request, WebSession::Ptr pSession)
{
	Poco::Net::HTTPCookie cookie(cookieName(appName), pSession->id());
	if (_cookiePersistence == COOKIE_PERSISTENT)
	{
		cookie.setMaxAge(pSession->timeout());
	}
	cookie.setPath(cookiePath(appName));
	cookie.setDomain(cookieDomain(appName));
	cookie.setHttpOnly();
	cookie.setSecure(_cookieSecure || request.secure());
	cookie.setSameSite(_cookieSameSite);
	return cookie;
}


Poco::Net::HTTPCookie WebSessionManager::createCSRFCookie(const std::string& appName, const Poco::Net::HTTPServerRequest& request, WebSession::Ptr pSession)
{
	std::string csrfToken = pSession->getValue<std::string>(WebSession::CSRF_TOKEN, ""s);
	if (csrfToken.empty())
	{
		// A session loaded from a store may have no token.
		csrfToken = createToken(request);
		pSession->set(WebSession::CSRF_TOKEN, csrfToken);
	}
	Poco::Net::HTTPCookie csrfCookie(_csrfCookie, csrfToken);
	if (_cookiePersistence == COOKIE_PERSISTENT)
	{
		csrfCookie.setMaxAge(pSession->timeout());
	}
	csrfCookie.setPath(cookiePath(appName));
	csrfCookie.setSecure(_cookieSecure || request.secure());
	csrfCookie.setSameSite(Poco::Net::HTTPCookie::SAME_SITE_STRICT);
	return csrfCookie;
}


std::string WebSessionManager::createToken(const Poco::Net::HTTPServerRequest& request)
{
	++_serial;

	Poco::SHA1Engine sha1;
	sha1.update(&_serial, sizeof(_serial));
	Poco::Timestamp::TimeVal tv = Poco::Timestamp().epochMicroseconds();
	sha1.update(&tv, sizeof(tv));
	Poco::RandomInputStream ris;
	for (int i = 0; i < 32; i++)
	{
		char c = ris.get();
		sha1.update(c);
	}
	sha1.update(request.clientAddress().toString());

	std::string result = Poco::DigestEngine::digestToHex(sha1.digest());
	return result;
}


std::string WebSessionManager::cookieName(const std::string& appName)
{
	std::string cookieName(COOKIE_NAME);
	if (!appName.empty())
	{
		std::string::size_type pos = appName.find('@');
		if (pos == std::string::npos) pos = appName.find('/');
		if (pos == std::string::npos) pos = appName.size();
		cookieName.append(".");
		cookieName.append(appName, 0, pos);
	}
	return cookieName;
}


std::string WebSessionManager::cookieDomain(const std::string& appName)
{
	std::string result;
	std::string::size_type pos = appName.find('@');
	if (pos != std::string::npos)
	{
		pos++;
		while (pos < appName.size() && appName[pos] != '/') result += appName[pos++];
	}
	return result.empty() ? _defaultDomain : result;
}


std::string WebSessionManager::cookiePath(const std::string& appName)
{
	std::string::size_type pos = appName.find('/');
	if (pos != std::string::npos)
		return appName.substr(pos);
	else if (!_defaultPath.empty())
		return _defaultPath;
	else
		return "/"s;
}


} // namespace Poco::OSP::Web
