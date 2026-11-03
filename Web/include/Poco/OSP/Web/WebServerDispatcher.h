//
// WebServerDispatcher.h
//
// Library: OSP/Web
// Package: Web
// Module:  WebServerDispatcher
//
// Definition of the WebServerDispatcher class.
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef OSP_Web_WebServerDispatcher_INCLUDED
#define OSP_Web_WebServerDispatcher_INCLUDED


#include "Poco/OSP/Web/Web.h"
#include "Poco/OSP/Web/MediaTypeMapper.h"
#include "Poco/OSP/Web/WebFilter.h"
#include "Poco/OSP/Web/WebFilterFactory.h"
#include "Poco/OSP/Web/WebSessionManager.h"
#include "Poco/OSP/Web/TokenValidator.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/BundleEvent.h"
#include "Poco/OSP/Service.h"
#include "Poco/OSP/ServiceListener.h"
#include "Poco/OSP/Auth/AuthService.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Net/HTTPRequestHandlerFactory.h"
#include "Poco/Net/HTTPResponse.h"
#include "Poco/RegularExpression.h"
#include "Poco/Logger.h"
#include "Poco/SharedPtr.h"
#include "Poco/ThreadPool.h"
#include "Poco/Clock.h"
#include "Poco/BasicEvent.h"
#include "Poco/Mutex.h"
#include "Poco/String.h"
#include "Poco/FileStream.h"
#include "Poco/File.h"
#include <vector>
#include <map>
#include <memory>
#include <set>


namespace Poco::OSP::Web {


class OSPWeb_API WebServerDispatcher: public Service
	/// A WebServerDispatcher is some sort of meta HTTPRequestHandlerFactory. It groups together
	/// several other HTTPRequestHandlerFactory instances and distributes requests to them according to the registered
	/// request handler extension points (see the WebServerExtensionPoint class).
{
public:
	using Ptr = Poco::AutoPtr<WebServerDispatcher>;

	enum SpecializationMode
		/// A bundle that registers a resource mapping or request handler for a certain
		/// path can specify whether other bundles can register subdirectories of this directory.
	{
		SM_NONE  = 0, /// No one can register subdirectories
		SM_OWNER = 1, /// Only the owner bundle can register subdirectories
		SM_ALL   = 2  /// Everyone can register subdirectories
	};

	enum AuthMethod
	{
		AUTH_BASIC   = 1, /// HTTP Basic authentication
		AUTH_SESSION = 2, /// Session-based authentication
		AUTH_BEARER  = 4, /// Bearer token-based authentication (OAuth 2.0)
		AUTH_ALL     = AUTH_BASIC | AUTH_SESSION | AUTH_BEARER
	};

	enum ResponseFormat
	{
		RESPONSE_FORMAT_HTML = 1, /// HTML-formatted error page
		RESPONSE_FORMAT_JSON = 2  /// JSON-formatted error page
	};

	static constexpr std::size_t BUFFER_SIZE = 8192;

	using RequestHandlerFactoryPtr = Poco::SharedPtr<Poco::Net::HTTPRequestHandlerFactory>;
	using RegularExpressionPtr = Poco::SharedPtr<Poco::RegularExpression>;

	struct PathSecurity
		/// Security attributes for a registered path.
	{
		PathSecurity():
			mode(SM_OWNER),
			authMethods(),
			secure(false),
			csrfProtection(false)
		{
		}

		PathSecurity(SpecializationMode aMode, const std::string& aRealm, const std::string& aPermission, bool aSecure = false):
			mode(aMode),
			realm(aRealm),
			permission(aPermission),
			authMethods(),
			secure(aSecure),
			csrfProtection(false)
		{
		}

		SpecializationMode mode;             /// specialization mode (does not apply to pattern)
		std::string        realm;            /// realm if a permission (and thus authentication) is required
		std::string        permission;       /// required permission empty for none;
		                                     /// can also be "*": credentials must be present and valid,
		                                     /// or "**": credentials are optional, but if present must be valid.
		int                authMethods;      /// enabled authentication methods (see AuthMethod)
		std::string        session;          /// name of session for session-based authentication
		bool               secure;           /// path requires secure connection
		bool               csrfProtection;   /// enable/disable CSRF/XSRF protection
		std::string        csrfTokenHeader;  /// name of header containing CSRF/XSRF token for CSRF/XSRF protection
	};

	struct PathCORS
	{
		PathCORS() = default;

		bool               enable = false;   /// enable or disable CORS
		std::string        allowOrigin;      /// CORS allowed origin(s), see enableCORS()
		std::string        allowMethods;     /// CORS allowed methods
		bool               allowCredentials = true; /// allow credentials with CORS requests
		std::string        allowHeaders;     /// allowed headers in CORS requests
	};

	struct VirtualPath
		/// A VirtualPath struct is used to specify a path mapping for a bundle.
	{
		VirtualPath():
			hidden(false),
			cache(true),
			exactMatch(false),
			responseFormat(RESPONSE_FORMAT_HTML)
		{
		}

		VirtualPath(const std::string& aPath, const std::string& aResource, const PathSecurity& aSecurity, Bundle::ConstPtr pOwner):
			path(aPath),
			resource(aResource),
			security(aSecurity),
			hidden(false),
			cache(true),
			exactMatch(false),
			responseFormat(RESPONSE_FORMAT_HTML),
			pOwnerBundle(pOwner)
		{
		}

		VirtualPath(const std::string& aPath, RequestHandlerFactoryPtr aFactory, const PathSecurity& aSecurity, Bundle::ConstPtr pOwner):
			path(aPath),
			pFactory(aFactory),
			security(aSecurity),
			hidden(false),
			cache(true),
			exactMatch(false),
			responseFormat(RESPONSE_FORMAT_HTML),
			pOwnerBundle(pOwner)
		{
		}

		RegularExpressionPtr     pPattern;            /// pattern for matching request handlers
		std::string              path;                /// virtual server path (e.g., /images)
		std::set<std::string>    methods;             /// allowed methods ("GET", "POST", etc.)
		std::string              description;         /// user-readable description of resource or service
		std::string              resource;            /// resource path (if mapped to resource)
		std::string              indexPage;           /// index page (only used if resource path is set; defaults to "index.html")
		RequestHandlerFactoryPtr pFactory;            /// request handler factory (null if resource path is specified)
		PathSecurity             security;            /// security attributes
		PathCORS                 cors;                /// CORS settings
		bool                     hidden;              /// path is not included in list returned by listVirtualPaths()
		bool                     cache;               /// resource can be cached
		bool                     exactMatch;          /// request path must match exactly
		ResponseFormat           responseFormat;      /// error response format (HTML or JSON)
		std::string              redirectOn401;       /// redirect (302) on a 401 error if request is a GET
		Bundle::Ptr              pOwnerBundle;        /// bundle owning path
	};

	struct PathInfo
	{
		std::string description;
		std::string permission;
		std::string session;
		Bundle::Ptr pBundle;
	};

	enum Options
	{
		CONF_OPT_COMPRESS_RESPONSES  = 0x01,
			/// Compress responses using gzip content encoding.

		CONF_OPT_CACHE_RESOURCES     = 0x02,
			/// Enable in-memory caching of bundle resources.

		CONF_OPT_ADD_AUTH_HEADER     = 0x04,
			/// Add X-OSP-Authorized-User header to authenticated requests.

		CONF_OPT_ADD_SIGNATURE       = 0x08,
			/// Add server signature to generated HTML error responses.

		CONF_OPT_LOG_FULL_REQUEST    = 0x10,
			/// Enable full logging of request and response headers.

		CONF_OPT_ENABLE_CORS         = 0x20,
			/// Globally enable or disable CORS handling.
		
		CONF_OPT_ENABLE_ACCEPT_RANGE   = 0x40
			/// Enable accept range handling.
	};

	struct Config
	{
		Config() = default;

		BundleContext::Ptr pContext;
		MediaTypeMapper::Ptr pMediaTypeMapper;
		std::string authServiceName;
		std::string tokenValidatorName;
		std::set<std::string> compressedMediaTypes;
		Poco::Net::NameValueCollection customResponseHeaders;
		int options = 0;
		int authMethods = AUTH_ALL;
		std::string corsAllowedOrigin;
	};

	struct OSPWeb_API PathLess
		/// Orders paths by the lower-case form of each character.
	{
		[[nodiscard]] bool operator () (const std::string& path1, const std::string& path2) const;
			/// Returns true if path1 comes before path2.
	};

	using PathMap = std::map<std::string, VirtualPath, PathLess>;
		/// The path mappings by normalized path (see normalizePath()).
	using PathInfoMap = std::map<std::string, PathInfo>;
	using PatternVec = std::vector<VirtualPath>;

	using WebFilterPtr = Poco::SharedPtr<WebFilter>;
	using WebFilterFactoryPtr = Poco::SharedPtr<WebFilterFactory>;

	struct RequestHandledEvent
	{
		RequestHandledEvent(const Poco::Net::HTTPServerRequest& req, const Poco::Net::HTTPServerResponse& res, Poco::Clock::ClockDiff dur):
			request(req),
			response(res),
			duration(dur)
		{
		}
	
		const Poco::Net::HTTPServerRequest& request;
		const Poco::Net::HTTPServerResponse& response;
		const Poco::Clock::ClockDiff duration;
	};

	Poco::BasicEvent<const RequestHandledEvent> requestHandled;
		/// Fired after a request has been handled
		/// and the response has been sent.
		///
		/// Can be used for computing metrics.

	explicit WebServerDispatcher(const Config& config);
		/// Creates the WebServerDispatcher.

	~WebServerDispatcher() override;
		/// Destroys the WebServerDispatcher.

	void addVirtualPath(const VirtualPath& virtualPath);
		/// Adds the given VirtualPath to the path mapping table.
		///
		/// Throws a Poco::InvalidArgumentException if a mapping for the given path
		/// (in any case) already exists, or if a parent or child mapping does not
		/// allow the new mapping (see SpecializationMode).

	void removeVirtualPath(const std::string& virtualPath);
		/// Removes the mapping of the given path, or every mapping of the given
		/// pattern, from the path mapping table.
		///
		/// Does nothing if no such mapping exists.

	void listVirtualPaths(PathInfoMap& paths) const;
		/// Returns a map containing all virtual paths (key) that are not patterns
		/// and not marked as hidden, and their descriptions (value).

	void virtualPathMappings(PathMap& mappings) const;
		/// Returns all path mappings. This member function creates a full
		/// copy of the internal path map.

	void removeBundle(Bundle::ConstPtr pBundle);
		/// Removes all mappings from the given bundle.

	void uncacheBundleResources(Bundle::ConstPtr pBundle);
		/// Removes all cached resources from the given bundle form the cache.

	void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response, bool secure);
		/// Handles the given request. Secure specifies whether the request has been
		/// sent over a secure (HTTPS) connection.
		///
		/// For a path that requires a permission, the authorized user and scope are the
		/// Poco::OSP::Auth::RequestIdentity of the thread while the request handler is
		/// created and runs.
		///
		/// While the auth service is not available, a request for a path that requires
		/// a permission is answered with 503 Service Unavailable. For the permission "**",
		/// only a request with credentials, or in a session that names a user, gets this answer.

	[[nodiscard]] Poco::ThreadPool& threadPool();
		/// Returns the thread pool for use by Poco::Net::HTTPServer.

	void addFilter(const std::string& mediaType, WebFilterFactoryPtr pFilterFactory, const WebFilter::Args& args,
		const Bundle::ConstPtr& pOwnerBundle);
		/// Adds a filter factory of the given bundle for the given media type.
		///
		/// Throws a Poco::ExistsException if a filter already exists for the given mediaType.

	void removeFilter(const std::string& mediaType);
		/// Removes the filter for the given mediaType. The filter factory, which can keep
		/// a library loaded, is released when the web sessions that exist now have been
		/// destroyed (see WebSessionManager::retainUntilSessionsEnd()). A request that still
		/// uses it extends this to the sessions that exist when the request has destroyed
		/// its filter. Uninstalling its bundle removes these sessions.

	[[nodiscard]] static int parseAuthMethods(const std::string& methods);
		/// Parses a comma-separated list of authentication method names and
		/// returns a value with the corresponding AuthMethod flags set.

	[[nodiscard]] static std::string canonicalPathWithQuery(const std::string& canonicalPath,
		const std::string& requestUri);
		/// Returns canonicalPath with the query string of requestUri appended
		/// (separated by '?'), or canonicalPath unchanged if requestUri has
		/// no query. Used to preserve query parameters across the trailing-
		/// slash canonicalization redirect.

	[[nodiscard]] static std::string redirectLocationOn401(const std::string& loginTarget,
		const std::string& requestUri);
		/// Returns the URL the client should be redirected to when a request
		/// fails authorization on a vPath that has redirectOn401 set.
		/// The path and query of requestUri (never its scheme and host),
		/// with a leading run of '/' and '\' replaced by a single '/',
		/// are appended as a `redirect` query parameter so the
		/// login UI can return there after a successful login. Skips the
		/// append (to avoid self-referential or ever-nesting chains) when
		/// the request already targets the login path or already carries
		/// its own redirect= parameter. The parameter value is encoded via
		/// Poco::URI::addQueryParameter, which percent-encodes '+' so
		/// HTMLForm decoders do not corrupt it.

	[[nodiscard]] static bool isSameOrigin(const Poco::Net::HTTPServerRequest& request, const std::string& origin);
		/// Returns true if origin, the value of an Origin header field, names the
		/// origin of the request itself: the scheme of the connection the request was
		/// received on (see HTTPServerRequest::secure()), and the host and port of its
		/// Host header field, with the default ports of the schemes applied.
		///
		/// Behind a reverse proxy that terminates TLS, the scheme of the connection
		/// is "http", so the "https" origin of the proxy is not the same origin.

	[[nodiscard]] static bool parseRange(const std::string& range, Poco::File::FileSize fileSize,
		Poco::File::FileSize& rangeStart, Poco::File::FileSize& rangeLength);
		/// Parses the "Range" HTTP header to determine the requested byte range, and
		/// assigns the result based on fileSize to rangeStart and rangeLength.
		///
		/// Returns false if the header is not a valid byte range set (RFC 9110 14.1.2)
		/// or if none of its ranges is satisfiable for fileSize. A last position beyond
		/// the end is limited to the end of the file. Of several ranges, the first
		/// satisfiable one is returned.

	// Service
	[[nodiscard]] const std::type_info& type() const override;
	[[nodiscard]] bool isA(const std::type_info& otherType) const override;

	static const std::string SERVICE_NAME;

protected:
	void onBundleStopping(const void* pSender, BundleEvent& ev);
		/// When a bundle is stopped, all of its request handlers and mappings are automatically disabled.
		/// The request handler factories of the mappings are kept until the bundle has stopped.

	void onBundleStopped(const void* pSender, BundleEvent& ev);
		/// Releases the request handler factories of the mappings of the stopped bundle,
		/// which can unload the libraries of the bundle, when the web sessions that exist
		/// now have been destroyed (see WebSessionManager::retainUntilSessionsEnd()).
		/// A request that still uses such a factory extends this to the sessions that
		/// exist when the request has ended.

	void onBundleUninstalling(const void* pSender, BundleEvent& ev);
		/// Removes the web sessions that keep factories of the bundle (see onBundleStopped()
		/// and removeFilter()) and releases the factories before the libraries of the bundle
		/// are removed from the code cache, so that a new version loads its own libraries.
		/// A running request that uses a factory of the bundle keeps the libraries loaded until
		/// it has ended and every web session that exists at that time has been destroyed
		/// (see WebSessionManager::retainUntilSessionsEnd()).

	[[nodiscard]] static std::string normalizePath(const std::string& path);
		/// Creates normalized path for internal storage.
		/// The normalized path always starts and ends with a slash.
		/// A UTF-8 path is in Unicode normalization form C, as macOS file
		/// systems open a resource under any normalization form of its path.

	[[nodiscard]] const VirtualPath& mapPath(const std::string& path, const std::string& method) const;
		/// Maps a URI to a VirtualPath.
		///
		/// Throws a NotFoundException if no suitable mapping can be found.

	void sendResource(Poco::Net::HTTPServerRequest& request, const std::string& path, const std::string& vpath, const std::string& resPath, const std::string& resBase, const std::string& index, Bundle::ConstPtr pBundle, bool canCache);
		/// Sends a bundle resource as response.

	[[nodiscard]] std::istream* findResource(Bundle::ConstPtr pBundle, const std::string& base, const std::string& res, const std::string& index, std::string& mediaType, std::string& resolvedPath, bool canCache, Poco::File::FileSize* pSize = nullptr, Poco::Timestamp* pLastModified = nullptr) const;
		/// Returns a resource stream for the given path, or a null pointer
		/// if no matching resource exists.
		///
		/// If given, pSize receives the size of the resource and pLastModified
		/// the time of its last modification (that of the bundle file for
		/// resources in a bundle file).

	[[nodiscard]] std::istream* getCachedResource(Bundle::ConstPtr pBundle, const std::string& path, bool canCache, Poco::File::FileSize* pSize = nullptr) const;
		/// Returns a resource stream for the given path, or a null pointer
		/// if no matching resource exists. If caching is enabled both globally
		/// and for the specific resource, attempts to cache the resource.

	[[nodiscard]] static bool cleanPath(std::string& path);
		/// Replaces backslashes with slashes, removes the dots at the
		/// end of the path and checks for illegal or dangerous
		/// characters. A path with a control character, a dot segment
		/// ("." or ".."), a segment that still ends with a dot or a
		/// segment that ends with a space is not okay.
		///
		/// Returns true if the path is okay, false otherwise.

	[[nodiscard]] static bool isResourcePath(const std::string& path);
		/// Returns false if the cleaned path contains a ':' (an NTFS data stream or a
		/// drive) or, on Windows, a '~' followed by a digit (an 8.3 short name).
		/// handleRequest() answers a request for such a resource with 400 Bad Request;
		/// the paths of request handlers are not checked.

	[[nodiscard]] bool handleCORS(const Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response, const VirtualPath& vPath) const;
		/// Handles CORS preflight requests and headers if CORS is enabled for the
		/// path (its cors attribute and CONF_OPT_ENABLE_CORS, set from "osp.web.cors.enable");
		/// otherwise returns true at once.
		///
		/// Returns false if the request has been answered. An Origin equal to the
		/// origin of the request itself (see isSameOrigin()) is always allowed.
		/// For other origins, a preflight (OPTIONS) request is answered here, with
		/// 403 Forbidden if the origin is not allowed. A request with a method other
		/// than GET and HEAD from an origin that is not allowed is answered with
		/// 403 Forbidden; GET and HEAD requests from such an origin are handled
		/// normally, without CORS headers.
		///
		/// If the allowed origin (see enableCORS()) is neither empty nor "*", the
		/// response has a "Vary: Origin" header, whatever the Origin of the request.

	[[nodiscard]] bool enableCORS(const Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response,
		const VirtualPath& vPath) const;
		/// Adds CORS headers to the response if the origin of the request is allowed,
		/// and returns whether it is.
		///
		/// The allowed origin is the one of the path, or the global one if the path
		/// has none. Empty allows no origin; "*" allows every origin, without
		/// credentials; "**" allows every origin by sending back the request's
		/// origin; any other value is a comma-separated list of allowed origins.

	[[nodiscard]] bool authorize(Poco::Net::HTTPServerRequest& request, const VirtualPath& vPath, std::string& username,
		std::string& scope, bool& authenticated) const;
		/// Authorizes the request.
		///
		/// Sets username to the user that the request names (see the access log) and
		/// scope to the scope that the token validator returns for a bearer token, or
		/// leaves them empty. Both name an authenticated identity only if the method
		/// returns true.
		///
		/// Sets authenticated to true if the request carries a valid, logged-in
		/// identity (valid credentials, or a session for an existing user), even when
		/// the return value is false because that identity lacks the required permission.
		/// This lets the caller distinguish "not logged in" from "logged in but
		/// forbidden" and react accordingly.

	[[nodiscard]] bool authorizeSession(Poco::Net::HTTPServerRequest& request, const VirtualPath& vPath,
		std::string& username, bool& authenticated) const;
		/// Authorizes the request using a session-based authentication.
		///
		/// Sets authenticated to true when a valid session for an existing user
		/// is present, regardless of whether the permission check succeeds,
		/// but not if the request fails the CSRF check, or if the auth service
		/// reports the user as not enabled (see AuthService::userEnabled()).

	[[nodiscard]] bool authorizeBasic(Poco::Net::HTTPServerRequest& request, const std::string& creds, const VirtualPath& vPath, std::string& username, bool& authenticated) const;
		/// Authorizes the request using a HTTP Basic Authentication.
		///
		/// Sets authenticated to true if the credentials are valid, regardless of
		/// whether the permission check succeeds. If the credentials are valid,
		/// username is the account name that AuthService::authenticateAccount()
		/// returns.

	[[nodiscard]] bool authorizeBearer(Poco::Net::HTTPServerRequest& request, const std::string& token, const VirtualPath& vPath, std::string& username, std::string& scope, bool& authenticated) const;
		/// Authorizes the request using bearer token-based authentication.
		///
		/// Sets authenticated to true if the token is valid and names an existing
		/// user that the auth service does not report as not enabled (see
		/// authorizeSession()), regardless of whether the permission check succeeds.

	[[nodiscard]] bool hasPermission(Poco::OSP::Auth::AuthService::Ptr pAuthService, const std::string& username,
		const std::string& permission) const;
		/// Checks if the user has the required permission.
		/// Supports comma-separated permissions (OR logic):
		/// "perm1,perm2" means the user needs at least one.
		/// Special values: "*" (any authenticated user), "**" (optional auth).

	[[nodiscard]] bool hasPermission(Poco::OSP::Auth::AuthService::Ptr pAuthService, const std::string& username,
		const std::string& scope, const std::string& permission) const;
		/// Checks if the user has the required permission within the given scope.
		/// Supports comma-separated permissions (OR logic).

	void sendFound(Poco::Net::HTTPServerRequest& request, const std::string& path, ResponseFormat format = RESPONSE_FORMAT_HTML);
		/// Sends a 302 Found response.

	void sendNotFound(Poco::Net::HTTPServerRequest& request, const std::string& path, ResponseFormat format = RESPONSE_FORMAT_HTML);
		/// Sends a 404 Not Found error response.

	void sendNotAuthorized(Poco::Net::HTTPServerRequest& request, const std::string& path, ResponseFormat format = RESPONSE_FORMAT_HTML);
		/// Sends a 401 Unauthorized error response.

	void sendForbidden(Poco::Net::HTTPServerRequest& request, const std::string& path, ResponseFormat format = RESPONSE_FORMAT_HTML);
		/// Sends a 403 Forbidden error response.

	void sendBadRequest(Poco::Net::HTTPServerRequest& request, const std::string& message, ResponseFormat format = RESPONSE_FORMAT_HTML);
		/// Sends a 400 Bad Request error response.

	void sendMethodNotAllowed(Poco::Net::HTTPServerRequest& request, const std::string& message, ResponseFormat format = RESPONSE_FORMAT_HTML);
		/// Sends a 405 Method Not Allowed error response.

	void sendInternalError(Poco::Net::HTTPServerRequest& request, const std::string& message, ResponseFormat format = RESPONSE_FORMAT_HTML);
		/// Sends a 500 Internal Server Error response.

	void sendResponse(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPResponse::HTTPStatus status, const std::string& message, ResponseFormat format = RESPONSE_FORMAT_HTML);
		/// Sends a standard status/error response.

	void sendHTMLResponse(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPResponse::HTTPStatus status, const std::string& message);
		/// Sends a standard status/error response in HTML format.

	void sendJSONResponse(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPResponse::HTTPStatus status, const std::string& message);
		/// Sends a standard status/error response in JSON format.

	[[nodiscard]] static std::string htmlize(const std::string& str);
		/// Returns a HTML-ized version of the given string.

	[[nodiscard]] Poco::OSP::Auth::AuthService::Ptr authService() const;
		/// Returns a pointer to the auth service, if it is registered,
		/// or null otherwise.

	[[nodiscard]] TokenValidator::Ptr tokenValidator() const;
		/// Returns a pointer to the token validator, if it is registered,
		/// or null otherwise.

	[[nodiscard]] WebSessionManager::Ptr sessionManager() const;
		/// Returns a pointer to the WebSessionManager, if it is registered,
		/// or null otherwise.

	[[nodiscard]] std::string formatMessage(const std::string& messageId, const std::string& arg1 = std::string(),
		const std::string& arg2 = std::string());
		/// Reads a message from the bundle.properties resource and replaces
		/// placeholders $1 and $2 with arg1 and arg2, respectively.

	[[nodiscard]] bool shouldCompressMediaType(const std::string& mediaType) const;
		/// Returns true iff content with the given media type should be compressed.

	[[nodiscard]] WebFilterPtr findFilter(const std::string& mediaType);
		/// Returns a WebFilter instance for the given mediaType, or a null
		/// pointer if no WebFilterFactory has been registered for the given
		/// mediaType.

	void addCustomResponseHeaders(Poco::Net::HTTPServerResponse& response);
		/// Adds any configured custom response headers.

	void logRequest(const Poco::Net::HTTPServerRequest& request, const Poco::Net::HTTPServerResponse& response, const std::string& username, Poco::Clock::ClockDiff duration);
		/// Logs the HTTP request.

	void sendFileRange(Poco::Net::HTTPServerResponse& response, std::istream& resourceStream, Poco::File::FileSize length, Poco::File::FileSize rangeStart, Poco::File::FileSize rangeLength);
		/// Configures response to return partial content defined with rangeStart and rangeLength
		/// from resourceStream with status 206 Partial Content.

	[[nodiscard]] Poco::Net::SocketAddress realClientAddress(const Poco::Net::HTTPServerRequest& request) const;
		/// Returns the client address obtained from the first entry of the X-Forwarded-For
		/// header, if that is an IP address, otherwise the client address from the request.
		///
		/// The X-Forwarded-For header is supplied by the client or by a proxy, and the
		/// address taken from it is only as trustworthy as that source.

	static const std::string BEARER;
	static const std::string X_OSP_AUTHORIZED_USER;
	static const std::string X_OSP_AUTHORIZED_SCOPE;

private:
	void onAuthServiceRegistered(const ServiceRef::Ptr& pServiceRef);
	void onAuthServiceUnregistered(const ServiceRef::Ptr& pServiceRef);
	void onTokenValidatorRegistered(const ServiceRef::Ptr& pServiceRef);
	void onTokenValidatorUnregistered(const ServiceRef::Ptr& pServiceRef);
	void onSessionManagerRegistered(const ServiceRef::Ptr& pServiceRef);
	void onSessionManagerUnregistered(const ServiceRef::Ptr& pServiceRef);

	[[nodiscard]] const std::string& allowedOrigin(const VirtualPath& vPath) const;
		/// Returns the allowed origin of the path, or the global one if the path has none.

	[[nodiscard]] std::set<std::string, Poco::CILess> csrfTokenHeaders() const;
		/// Returns the names of the CSRF token header fields of all paths that have
		/// been added, for leaving their values out of logged requests.

	[[nodiscard]] bool sessionNamesUser(const Poco::Net::HTTPServerRequest& request, const VirtualPath& vPath) const;
		/// Returns true if the path has a session name, session-based authentication
		/// is enabled for it, and the request has that session of the WebSessionManager,
		/// with a user name.

	struct WebFilterFactoryInfo
	{
		WebFilterFactoryPtr pFactory;
		WebFilter::Args args;
		Bundle::Ptr pOwnerBundle;
	};
	using FilterFactoryMap = std::map<std::string, WebFilterFactoryInfo>;
	using ResourceCache = std::map<std::string, std::shared_ptr<const std::string>>;

	[[nodiscard]] WebFilterPtr findFilterAndFactory(const std::string& mediaType, WebFilterFactoryInfo& factoryInfo);
		/// Returns a WebFilter instance like findFilter(mediaType), and stores the
		/// WebFilterFactory that has created it in factoryInfo.

	void retainForSessions(const Bundle::ConstPtr& pBundle, const std::shared_ptr<const void>& pObject);
		/// Keeps pObject, which keeps libraries of the given bundle loaded, until the web
		/// sessions that exist now have been destroyed or onBundleUninstalling() removes them.
		/// Without a registered WebSessionManager, pObject is released with the caller's reference.

	void retainIfUnmapped(const VirtualPath& vPath);
		/// After a request, calls retainForSessions() with the request handler factory of
		/// vPath if its bundle has stopped meanwhile (not active, and no mapping has the factory).

	void retainIfRemoved(const std::string& mediaType, const WebFilterFactoryInfo& factoryInfo);
		/// After a request, calls retainForSessions() with the filter factory if it is no
		/// longer the filter factory for the media type.

	BundleContext::Ptr _pContext;
	MediaTypeMapper::Ptr _pMediaTypeMapper;
	PathMap _pathMap;
	PatternVec _patternVec;
	// The request handler factories of the mappings of stopping bundles, by bundle ID. They keep the
	// libraries of the bundles loaded, because other bundleStopping delegates can still destroy objects
	// that the libraries have created, such as the attributes of web sessions.
	std::map<int, std::vector<RequestHandlerFactoryPtr>> _stoppingBundleFactories;
	// The objects that retainForSessions() has given to web sessions, by bundle ID.
	std::map<int, std::vector<std::weak_ptr<const void>>> _sessionRetainedObjects;
	std::set<std::string, Poco::CILess> _csrfTokenHeaders;
	std::string _authServiceName;
	std::string _tokenValidatorName;
	bool _corsEnabled;
	std::string _corsAllowedOrigin;
	bool _compressResponses;
	bool _enableAcceptRange;
	bool _cacheResources;
	bool _addAuthHeader;
	bool _addSignature;
	bool _logFullRequest;
	int _authMethods;
	std::set<std::string> _compressedMediaTypes;
	Poco::Net::NameValueCollection _customResponseHeaders;
	Poco::OSP::Auth::AuthService::Ptr _pAuthService;
	TokenValidator::Ptr _pTokenValidator;
	ServiceListener::Ptr _pAuthServiceListener;
	ServiceListener::Ptr _pTokenValidatorListener;
	WebSessionManager::Ptr _pSessionManager;
	ServiceListener::Ptr _pSessionManagerListener;
	mutable ResourceCache _resourceCache;
	mutable Poco::FastMutex _resourceCacheMutex;
	FilterFactoryMap _filterFactoryMap;
	mutable Poco::FastMutex _filterFactoryMutex;
	Poco::ThreadPool _threadPool;
	mutable Poco::FastMutex _mutex;
	mutable Poco::FastMutex _authServiceMutex;
	mutable Poco::FastMutex _tokenValidatorMutex;
	mutable Poco::FastMutex _sessionManagerMutex;
	Poco::Logger& _accessLogger;
};


//
// inlines
//
inline Poco::ThreadPool& WebServerDispatcher::threadPool()
{
	return _threadPool;
}


} // namespace Poco::OSP::Web


#endif // OSP_Web_WebServerDispatcher_INCLUDED
