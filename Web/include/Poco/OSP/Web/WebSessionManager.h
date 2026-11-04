//
// WebSessionManager.h
//
// Library: OSP/Web
// Package: Web
// Module:  WebSessionManager
//
// Definition of the WebSessionManager class.
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#ifndef OSP_Web_WebSessionManager_INCLUDED
#define OSP_Web_WebSessionManager_INCLUDED


#include "Poco/OSP/Web/Web.h"
#include "Poco/OSP/Web/WebSession.h"
#include "Poco/OSP/Web/WebSessionService.h"
#include "Poco/OSP/Web/WebSessionStore.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/BundleEvent.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Net/HTTPCookie.h"
#include "Poco/UniqueExpireCache.h"
#include "Poco/Logger.h"
#include "Poco/Mutex.h"
#include <functional>
#include <memory>


namespace Poco::OSP::Web {


class WebSessionRegistry;


class OSPWeb_API WebSessionManager: public WebSessionService<Poco::Net::HTTPServerRequest>
	/// A WebSessionManager manages HTTP sessions using cookies.
	///
	/// The SessionManager is registered under the service name
	/// "osp.web.session".
	///
	/// Note that the application name (appName) specified in get() and create()
	/// can contain a domain name, separated by '@'. If a domain name is given,
	/// it is used to set the domain for the session cookie. Similarly, a path
	/// can be given, beginning with a slash. If the appName contains no path,
	/// the default path (see setDefaultPath()) is used, or "/" if no default
	/// path has been set.
	///
	/// For example, if the appName is sample@.appinf.com, then the browser
	/// will send the session cookie to all hosts with names in the appinf.com
	/// domain. If a domain is not given, the session cookie will only be available
	/// to the host that has originally set it.
{
public:
	using Ptr = Poco::AutoPtr<WebSessionManager>;

	enum CookiePersistence
	{
		COOKIE_TRANSIENT  = 1, /// Session cookies are transient (go away when browser is closed).
		COOKIE_PERSISTENT = 2  /// Session cookies are persistent (kept in browser until they expire).
	};

	explicit WebSessionManager(Poco::OSP::BundleContext::Ptr pContext);
		/// Creates the SessionManager without a WebSessionStore.

	~WebSessionManager() override;
		/// Destroys the SessionManager.

	void setDefaultDomain(const std::string& domain);
		/// Sets the default domain for the session cookie.

	[[nodiscard]] const std::string& getDefaultDomain() const;
		/// Returns the default domain for the session cookie.

	void setDefaultPath(const std::string& path);
		/// Sets the default path for the session cookie, used if the
		/// application name does not contain a path.

	[[nodiscard]] const std::string& getDefaultPath() const;
		/// Returns the default path for the session cookie.

	void setCookiePersistence(CookiePersistence persistence);
		/// Sets the cookie persistence, which controls whether
		/// session and CSRF cookies are transient (go away when the
		/// browser is closed) or persistent (default).

	[[nodiscard]] CookiePersistence getCookiePersistence() const;
		/// Returns the cookie persistence for the session
		/// and CSRF cookies.

	void setCookieSecure(bool secure);
		/// Sets the secure attribute of the session cookie.
		///
		/// If set to true, the browser will only send the
		/// cookie over HTTPS connections.
		///
		/// Session and CSRF cookies sent in the response to a request
		/// received over HTTPS always have the secure attribute.

	[[nodiscard]] bool isCookieSecure() const;
		/// Returns true if the session cookie has the secure
		/// attribute set for all requests, otherwise false.

	void setCookieSameSite(Poco::Net::HTTPCookie::SameSite sameSite);
		/// Sets the SameSite attribute for the session cookie.
		///
		/// The default is Poco::Net::HTTPCookie::SAME_SITE_NOT_SPECIFIED.

	[[nodiscard]] Poco::Net::HTTPCookie::SameSite getCookieSameSite() const;
		/// Returns the SameSite attribute for the session cookie.

	POCO_DEPRECATED("use getCookieSameSite() instead")
	[[nodiscard]] Poco::Net::HTTPCookie::SameSite setCookieSameSite() const;
		/// Returns the SameSite attribute for the session cookie.

	void setVerifyAddress(bool verify);
		/// Enable or disable verification of client address
		/// against the address stored in the session.

	[[nodiscard]] bool isAddressVerified() const;
		/// Returns true if the client IP address is verified
		/// against the address stored in the session.

	void setCSRFCookie(const std::string& name);
		/// Sets the name of the CSRF/XSRF cookie.
		///
		/// If set, the CSRF token of the session will be stored in
		/// the cookie with the given name. This cookie is accessible
		/// from JavaScript and can be used to authenticate scripted
		/// HTTP requests (together with the session cookie).

	[[nodiscard]] const std::string& getCSRFCookie() const;
		/// Returns the name of the CSRF cookie, if set, otherwise
		/// an empty string.

	void setSessionStore(WebSessionStore::Ptr pWebSessionStore);
		/// Sets the WebSessionStore.

	[[nodiscard]] WebSessionStore::Ptr getSessionStore() const;
		/// Returns the WebSessionStore.

	[[nodiscard]] WebSession::Ptr findById(const std::string& sessionId);
		/// Returns the session with the given ID, or null if it does not exist.

	[[nodiscard]] std::size_t countSessions();
		/// Returns the number of active sessions.

	void forEachSession(const std::function<void(const std::string& sessionId, const WebSession& session)>& fn);
		/// Calls fn for every session in the in-memory session cache, while
		/// holding the session manager mutex. fn must not call back into the
		/// WebSessionManager.
		///
		/// Other threads may modify the sessions while fn runs, so fn must read
		/// attributes with WebSession::attributes(), WebSession::getValue() or
		/// WebSession::has(), which lock the session, and not with
		/// WebSession::find(), WebSession::begin() or WebSession::end().
		///
		/// Note: if a WebSessionStore has been set, only sessions currently
		/// present in the in-memory cache are enumerated (the store interface
		/// does not support enumerating all sessions).

	void retainUntilSessionsEnd(std::shared_ptr<const void> pObject);
		/// Keeps pObject until every session that exists now has been destroyed,
		/// also the sessions that renew() creates from them, or releases it at
		/// once if no session exists. The WebServerDispatcher keeps the libraries of a
		/// stopped bundle loaded this way, as sessions can hold values that these libraries
		/// have created (see WebServerDispatcher::onBundleUninstalling()).

	void removeSessionsRetaining(const std::shared_ptr<const void>& pObject);
		/// Removes every session that keeps pObject (see retainUntilSessionsEnd()),
		/// like remove(). A session is destroyed, and releases pObject, when the
		/// last request that uses it has ended.

	void clearCache();
		/// Removes every session from the in-memory session cache; the session
		/// store keeps its sessions. Sessions that have not expired are released
		/// after the mutex is unlocked, as destroying a session can call back
		/// into this manager; expired sessions are released while the mutex is
		/// held, as on every lookup of the cache.
		///
		/// A session can retain an object that holds this manager (see
		/// retainUntilSessionsEnd()), so the cache can keep the manager alive.
		/// The Web bundle calls clearCache() when it stops. Destroying a session
		/// can release the last other reference to this manager, so the caller
		/// must hold a reference to it.

	void updateSessionTimeout(const std::string& appName, WebSession::Ptr pSession, int timeout, const Poco::Net::HTTPServerRequest& request);
		/// Updates the session timeout for the given session and updates the
		/// Set-Cookie header in the response for the given request.

	[[nodiscard]] WebSession::Ptr renew(const std::string& appName, const Poco::Net::HTTPServerRequest& request, WebSession::Ptr pSession);
		/// Replaces the given session with a new session that has a new ID
		/// and a new CSRF token, and returns the new session.
		///
		/// The new session gets all other attributes and the timeout of the
		/// given session. The given session is removed, and the session
		/// cookie (and the CSRF cookie, if enabled) in the response for the
		/// given request is replaced.
		///
		/// A login handler calls renew() after the user has been authenticated,
		/// so that a session ID that was known before the login (for example,
		/// one set by another party) cannot be used to access the session
		/// of the logged-in user.

	// WebSessionService
	[[nodiscard]] WebSession::Ptr find(const std::string& appName,
		const Poco::Net::HTTPServerRequest& request) override;
	[[nodiscard]] WebSession::Ptr get(const std::string& appName, const Poco::Net::HTTPServerRequest& request,
		int expireSeconds, BundleContext::Ptr pContext) override;
	WebSession::Ptr create(const std::string& appName, const Poco::Net::HTTPServerRequest& request, int expireSeconds,
		BundleContext::Ptr pContext) override;
	void remove(WebSession::Ptr ptr) override;
	void removeForUser(const std::string& username) override;

	// Service
	[[nodiscard]] const std::type_info& type() const override;
	[[nodiscard]] bool isA(const std::type_info& otherType) const override;

	static const std::string SERVICE_NAME;

protected:
	void onBundleStopping(const void* pSender, BundleEvent& ev);
		/// Clears the attributes of every session owned by the stopping bundle that
		/// this manager has created or loaded and that still exists, also if it is
		/// no longer cached (see WebSession::releaseAttributes()). Does not empty
		/// the in-memory session cache (see clearCache()).

	[[nodiscard]] WebSession::Ptr findImpl(const std::string& appName, const Poco::Net::HTTPServerRequest& request,
		BundleContext::Ptr pContext);
	[[nodiscard]] WebSession::Ptr findByIdImpl(const std::string& sessionId, BundleContext::Ptr pContext);
	[[nodiscard]] WebSession::Ptr createImpl(const std::string& appName, const Poco::Net::HTTPServerRequest& request,
		int expireSeconds, BundleContext::Ptr pContext);
	void removeImpl(WebSession::Ptr pSession);
	void removeImpl(const std::string& id);
	[[nodiscard]] std::string getId(const std::string& appName, const Poco::Net::HTTPServerRequest& request);
	void addSessionCookie(const std::string& appName, const Poco::Net::HTTPServerRequest& request, WebSession::Ptr pSession);
	void addCSRFCookie(const std::string& appName, const Poco::Net::HTTPServerRequest& request, WebSession::Ptr pSession);
	[[nodiscard]] Poco::Net::HTTPCookie createSessionCookie(const std::string& appName, const Poco::Net::HTTPServerRequest& request, WebSession::Ptr pSession);
	[[nodiscard]] Poco::Net::HTTPCookie createCSRFCookie(const std::string& appName, const Poco::Net::HTTPServerRequest& request, WebSession::Ptr pSession);
		/// Creates the CSRF cookie for the session. Creates and stores a new
		/// CSRF token if the session has none. Must be called with the mutex locked.

	[[nodiscard]] std::string createToken(const Poco::Net::HTTPServerRequest& request);
	[[nodiscard]] std::string cookieName(const std::string& appName);
	[[nodiscard]] std::string cookieDomain(const std::string& appName);
	[[nodiscard]] std::string cookiePath(const std::string& appName);

private:
	static const std::string COOKIE_NAME;

	mutable Poco::FastMutex _mutex;
	Poco::OSP::BundleContext::Ptr _pContext;
	WebSessionStore::Ptr _pStore;
	Poco::UInt32 _serial = 0;
	Poco::UniqueExpireCache<std::string, WebSession> _cache;
	std::shared_ptr<WebSessionRegistry> _pSessionRegistry;
	std::string _defaultDomain;
	std::string _defaultPath;
	std::string _csrfCookie;
	CookiePersistence _cookiePersistence = COOKIE_PERSISTENT;
	bool _cookieSecure = false;
	bool _verifyAddress = true;
	Poco::Net::HTTPCookie::SameSite _cookieSameSite = Poco::Net::HTTPCookie::SAME_SITE_NOT_SPECIFIED;
	Poco::Logger& _logger;
};


} // namespace Poco::OSP::Web


#endif // OSP_Web_WebSessionManager_INCLUDED
