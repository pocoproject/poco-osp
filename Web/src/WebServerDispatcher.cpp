//
// WebServerDispatcher.cpp
//
// Library: OSP/Web
// Package: Web
// Module:  WebServerDispatcher
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#include "Poco/OSP/Web/WebServerDispatcher.h"
#include "Poco/OSP/Web/MediaTypeMapper.h"
#include "Poco/OSP/Web/WebSession.h"
#include "Poco/OSP/Web/WebFilter.h"
#include "Poco/OSP/Web/TokenValidator.h"
#include "Poco/OSP/BundleDirectory.h"
#include "Poco/OSP/BundleEvent.h"
#include "Poco/OSP/BundleEvents.h"
#include "Poco/OSP/ServiceRegistry.h"
#include "Poco/OSP/ServiceRef.h"
#include "Poco/OSP/Auth/RequestIdentity.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Net/HTTPServerResponse.h"
#include "Poco/Net/HTTPRequestHandler.h"
#include "Poco/Net/HTTPServerParams.h"
#include "Poco/Net/HTTPBasicCredentials.h"
#include "Poco/Net/HTTPMessage.h"
#include "Poco/Net/MessageHeader.h"
#include "Poco/Net/NameValueCollection.h"
#include "Poco/Net/IPAddress.h"
#include "Poco/Net/SocketAddress.h"
#include "Poco/Delegate.h"
#include "Poco/URI.h"
#include "Poco/UUIDGenerator.h"
#include "Poco/StreamCopier.h"
#include "Poco/Exception.h"
#include "Poco/NumberFormatter.h"
#include "Poco/NumberParser.h"
#include "Poco/Format.h"
#include "Poco/ScopedLock.h"
#include "Poco/File.h"
#include "Poco/DateTimeParser.h"
#include "Poco/DateTimeFormatter.h"
#include "Poco/DateTimeFormat.h"
#include "Poco/DeflatingStream.h"
#include "Poco/MemoryStream.h"
#include "Poco/Buffer.h"
#include "Poco/StringTokenizer.h"
#include "Poco/String.h"
#include "Poco/UTF8String.h"
#include "Poco/Ascii.h"
#include "Poco/Types.h"
#include "Poco/Message.h"
#include "Poco/SharedPtr.h"
#include "Poco/Bugcheck.h"
#include "Poco/Config.h"
#include <algorithm>
#include <ios>
#include <memory>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>


using namespace std::string_literals;


using Poco::OSP::Auth::AuthService;
using Poco::OSP::ServiceRegistry;
using Poco::OSP::ServiceRef;
using Poco::Net::HTTPRequestHandler;
using Poco::Net::HTTPResponse;
using Poco::Net::HTTPBasicCredentials;
using Poco::Delegate;
using Poco::Path;
using Poco::StreamCopier;
using Poco::FastMutex;


namespace Poco::OSP::Web {


namespace
{
	class CachedResourceStream: public Poco::MemoryInputStream
		/// Reads a cached resource and keeps it, also when the cache removes it meanwhile.
	{
	public:
		CachedResourceStream() = delete;

		explicit CachedResourceStream(std::shared_ptr<const std::string> pResource):
			Poco::MemoryInputStream(pResource->data(), static_cast<std::streamsize>(pResource->size())),
			_pResource(std::move(pResource))
		{
		}

		~CachedResourceStream() override = default;

		CachedResourceStream(const CachedResourceStream&) = delete;
		CachedResourceStream(CachedResourceStream&&) = delete;
		CachedResourceStream& operator = (const CachedResourceStream&) = delete;
		CachedResourceStream& operator = (CachedResourceStream&&) = delete;

	private:
		std::shared_ptr<const std::string> _pResource;
	};

	template <typename Fn>
	class AtScopeEnd
		/// Calls a function when it is destroyed.
	{
	public:
		AtScopeEnd() = delete;

		POCO_NODISCARD_CTOR explicit AtScopeEnd(Fn fn):
			_fn(std::move(fn))
		{
		}

		~AtScopeEnd()
		{
			try
			{
				_fn();
			}
			catch (...)
			{
				poco_unexpected();
			}
		}

		AtScopeEnd(const AtScopeEnd&) = delete;
		AtScopeEnd(AtScopeEnd&&) = delete;
		AtScopeEnd& operator = (const AtScopeEnd&) = delete;
		AtScopeEnd& operator = (AtScopeEnd&&) = delete;

	private:
		Fn _fn;
	};
}


static std::set<std::string> parseResourceList(const std::string& s)
{
	std::set<std::string> out;
	Poco::StringTokenizer tok(s, ",;", Poco::StringTokenizer::TOK_TRIM | Poco::StringTokenizer::TOK_IGNORE_EMPTY);
	for (auto it = tok.begin(); it != tok.end(); ++it)
	{
		std::string item = *it;
		Poco::toLowerInPlace(item);
		out.insert(item);
	}
	return out;
}

static bool isPersistentResource(const Poco::OSP::Bundle::ConstPtr& pBundle, const std::string& resBase)
{
	const std::string cfg = pBundle->properties().getString("web.static.resource", "");
	if (cfg.empty()) return false;
	std::set<std::string> wanted = parseResourceList(cfg);
	std::string key = resBase; Poco::toLowerInPlace(key);
	// The resource attribute of a mapping may end with a slash.
	key.erase(key.find_last_not_of('/') + 1);
	return wanted.contains(key);
}


[[nodiscard]] static std::string knownUserName(const Poco::OSP::Auth::AuthService& authService, const std::string& username)
	// Returns username, or its lower-case form if only that is known to the auth service:
	// LDAP directories may know a user in lower case only, while sessions, credentials
	// and tokens keep the case the user typed.
{
	if (!authService.userExists(username))
	{
		std::string lcUsername = Poco::toLower(username);
		if (lcUsername != username && authService.userExists(lcUsername)) return lcUsername;
	}
	return username;
}


[[nodiscard]] static bool parseRangePosition(const std::string& digits, Poco::UInt64& position)
{
	return !digits.empty() && std::all_of(digits.begin(), digits.end(), Poco::Ascii::isDigit) && Poco::NumberParser::tryParseUnsigned64(digits, position);
}


[[nodiscard]] static bool parseRangeSpec(const std::string& spec, Poco::File::FileSize fileSize, bool& satisfiable, Poco::File::FileSize& rangeStart, Poco::File::FileSize& rangeLength)
	// Parses one range of a Range header field (RFC 9110 14.1.1). Returns false if
	// the syntax is invalid; otherwise sets satisfiable and, if true, the range.
{
	const std::string::size_type dashPos = spec.find('-');
	if (dashPos == std::string::npos) return false;
	const std::string first(spec, 0, dashPos);
	const std::string last(spec, dashPos + 1);
	Poco::UInt64 firstPos = 0;
	Poco::UInt64 lastPos = std::numeric_limits<Poco::UInt64>::max();
	if (first.empty())
	{
		Poco::UInt64 suffixLength = 0;
		if (!parseRangePosition(last, suffixLength)) return false;
		satisfiable = suffixLength > 0 && fileSize > 0;
		rangeLength = std::min(suffixLength, fileSize);
		rangeStart = fileSize - rangeLength;
		return true;
	}
	if (!parseRangePosition(first, firstPos)) return false;
	if (!last.empty() && (!parseRangePosition(last, lastPos) || lastPos < firstPos)) return false;
	satisfiable = firstPos < fileSize;
	if (satisfiable)
	{
		rangeStart = firstPos;
		rangeLength = std::min(lastPos, fileSize - 1) - firstPos + 1;
	}
	return true;
}


[[nodiscard]] static bool acceptsGzip(const Poco::Net::HTTPServerRequest& request)
	// Returns true if the Accept-Encoding header field lists gzip, or "*" without
	// gzip, with a quality value other than 0 (RFC 9110 12.5.3).
{
	bool wildcardAccepted = false;
	Poco::StringTokenizer codings(request.get("Accept-Encoding"s, ""s), ","s, Poco::StringTokenizer::TOK_TRIM | Poco::StringTokenizer::TOK_IGNORE_EMPTY);
	for (const auto& element: codings)
	{
		std::string coding;
		Poco::Net::NameValueCollection parameters;
		Poco::Net::MessageHeader::splitParameters(element, coding, parameters);
		const std::string quality(parameters.get("q"s, "1"s));
		const bool accepted = quality.empty() || quality[0] != '0' || quality.find_first_not_of("0."s) != std::string::npos;
		if (Poco::icompare(coding, "gzip"s) == 0) return accepted;
		if (coding == "*") wildcardAccepted = accepted;
	}
	return wildcardAccepted;
}


[[nodiscard]] static bool isControlCharacter(char ch)
{
	return Poco::Ascii::hasProperties(ch, Poco::Ascii::ACP_CONTROL);
}


[[nodiscard]] static bool isToken(const std::string& text)
	// Returns true if the text is an HTTP token: one or more tchar of RFC 9110.
{
	static constexpr std::string_view TOKEN_PUNCTUATION("!#$%&'*+-.^_`|~");
	return !text.empty() && std::all_of(text.begin(), text.end(), [](char ch)
	{
		return Poco::Ascii::isAlphaNumeric(ch) || TOKEN_PUNCTUATION.find(ch) != std::string_view::npos;
	});
}


[[nodiscard]] static std::string loggedText(const std::string& text)
	// Percent-encodes control characters of request text, so that a terminal or log viewer
	// that shows the log does not act on them.
{
	std::string result;
	for (const char ch: text)
	{
		if (isControlCharacter(ch))
		{
			result += '%';
			Poco::NumberFormatter::appendHex(result, static_cast<unsigned char>(ch), 2);
		}
		else result += ch;
	}
	return result;
}


[[nodiscard]] static std::string loggedRequestLine(const Poco::Net::HTTPServerRequest& request)
	// Returns the method, URI and version, separated by spaces and escaped.
{
	std::string result(loggedText(request.getMethod()));
	result += ' ';
	result += loggedText(request.getURI());
	result += ' ';
	result += loggedText(request.getVersion());
	return result;
}


[[nodiscard]] static std::string loggedMessageHead(const std::string& startLine,
	const Poco::Net::NameValueCollection& fields, const std::set<std::string, Poco::CILess>& csrfTokenHeaders)
	// Returns the start line, escaped by the caller, and the escaped header fields on indented
	// lines, with the values of the header fields that carry credentials, session IDs or CSRF
	// tokens, or whose name is not an HTTP token, replaced.
{
	static const std::set<std::string, Poco::CILess> CREDENTIAL_FIELDS{
		"Authorization"s, "Proxy-Authorization"s, "Cookie"s, "Set-Cookie"s};
	std::string result(startLine);
	for (const auto& [name, value]: fields)
	{
		result += "\n\t"s;
		result += loggedText(name);
		result += ": "s;
		// A recipient can parse a name that is not a token as another field, such as Set-Cookie.
		if (!isToken(name) || CREDENTIAL_FIELDS.contains(name) || csrfTokenHeaders.contains(name))
			result += "<redacted>"s;
		else
			result += loggedText(value);
	}
	return result;
}


[[nodiscard]] static Poco::Timestamp resourceLastModified(const Poco::OSP::Bundle::ConstPtr& pBundle,
	const std::string& resourcePath)
	// Uses the time of the resource file for a directory bundle, as changing a file does not
	// change the time of the bundle directory.
{
	const Poco::File bundleFile(pBundle->path());
	if (bundleFile.isDirectory())
	{
		const Poco::File resourceFile(Path(Path::forDirectory(pBundle->path()), Path(resourcePath, Path::PATH_UNIX)));
		if (resourceFile.exists()) return resourceFile.getLastModified();
	}
	return bundleFile.getLastModified();
}


static bool isStrictFS(const Poco::OSP::Bundle::ConstPtr& pBundle)
{
	return Poco::icompare(pBundle->properties().getString("web.static.strict", "false"), "true") == 0;
}


const std::string WebServerDispatcher::SERVICE_NAME("osp.web.dispatcher");
const std::string WebServerDispatcher::BEARER("Bearer");
const std::string WebServerDispatcher::X_OSP_AUTHORIZED_USER("X-OSP-Authorized-User");
const std::string WebServerDispatcher::X_OSP_AUTHORIZED_SCOPE("X-OSP-Authorized-Scope");


WebServerDispatcher::WebServerDispatcher(const Config& config):
	_pContext(config.pContext),
	_pMediaTypeMapper(config.pMediaTypeMapper),
	_authServiceName(config.authServiceName),
	_tokenValidatorName(config.tokenValidatorName),
	_corsEnabled((config.options & CONF_OPT_ENABLE_CORS) != 0),
	_corsAllowedOrigin(config.corsAllowedOrigin),
	_compressResponses((config.options & CONF_OPT_COMPRESS_RESPONSES) != 0),
	_enableAcceptRange((config.options & CONF_OPT_ENABLE_ACCEPT_RANGE) != 0),
	_cacheResources((config.options & CONF_OPT_CACHE_RESOURCES) != 0),
	_addAuthHeader((config.options & CONF_OPT_ADD_AUTH_HEADER) != 0),
	_addSignature((config.options & CONF_OPT_ADD_SIGNATURE) != 0),
	_logFullRequest((config.options & CONF_OPT_LOG_FULL_REQUEST) != 0),
	_authMethods(config.authMethods),
	_compressedMediaTypes(config.compressedMediaTypes),
	_customResponseHeaders(config.customResponseHeaders),
	_threadPool("WebServer"s),
	_accessLogger(Poco::Logger::get("osp.web.access"s))
{
	_pContext->events().bundleStopping += Delegate<WebServerDispatcher, BundleEvent>(this, &WebServerDispatcher::onBundleStopping);
	_pContext->events().bundleStopped += Delegate<WebServerDispatcher, BundleEvent>(this, &WebServerDispatcher::onBundleStopped);
	_pContext->events().bundleUninstalling += Delegate<WebServerDispatcher, BundleEvent>(this,
		&WebServerDispatcher::onBundleUninstalling);
	if (!_authServiceName.empty())
	{
		_pAuthServiceListener = _pContext->registry().createListener(Poco::format("name == \"%s\""s, _authServiceName),
			Poco::delegate(this, &WebServerDispatcher::onAuthServiceRegistered),
			Poco::delegate(this, &WebServerDispatcher::onAuthServiceUnregistered));
		if (authService() == nullptr)
		{
			// The auth bundle may start after the web bundle: this is no error yet.
			_pContext->logger().information("The auth service %s is not registered yet. Requests for paths that require a permission are answered with 503 Service Unavailable until it is registered."s, _authServiceName);
		}
	}
	else
	{
		_pContext->logger().warning("No auth service is configured (osp.web.authServiceName). Requests for paths that require a permission are answered with 503 Service Unavailable."s);
	}
	if (!_tokenValidatorName.empty())
	{
		_pTokenValidatorListener = _pContext->registry().createListener(Poco::format("name == \"%s\""s, _tokenValidatorName),
			Poco::delegate(this, &WebServerDispatcher::onTokenValidatorRegistered),
			Poco::delegate(this, &WebServerDispatcher::onTokenValidatorUnregistered));
	}
	_pSessionManagerListener = _pContext->registry().createListener(Poco::format("name == \"%s\""s, WebSessionManager::SERVICE_NAME),
		Poco::delegate(this, &WebServerDispatcher::onSessionManagerRegistered),
		Poco::delegate(this, &WebServerDispatcher::onSessionManagerUnregistered));
}


WebServerDispatcher::~WebServerDispatcher()
{
	try
	{
		_pAuthServiceListener = nullptr;
		_pTokenValidatorListener = nullptr;
		_pSessionManagerListener = nullptr;
		_pContext->events().bundleStopping -= Delegate<WebServerDispatcher, BundleEvent>(this, &WebServerDispatcher::onBundleStopping);
		_pContext->events().bundleStopped -= Delegate<WebServerDispatcher, BundleEvent>(this, &WebServerDispatcher::onBundleStopped);
		_pContext->events().bundleUninstalling -= Delegate<WebServerDispatcher, BundleEvent>(this,
			&WebServerDispatcher::onBundleUninstalling);
	}
	catch (...)
	{
		poco_unexpected();
	}
}


bool WebServerDispatcher::PathLess::operator () (const std::string& path1, const std::string& path2) const
{
	// Mappings that differ only in case name the same path; normalizePath() has given them the same normalization form.
	return Poco::UTF8::icompare(path1, path2) < 0;
}


const std::type_info& WebServerDispatcher::type() const
{
	return typeid(WebServerDispatcher);
}


bool WebServerDispatcher::isA(const std::type_info& otherType) const
{
	std::string name(typeid(WebServerDispatcher).name());
	return name == otherType.name() || Service::isA(otherType);
}


void WebServerDispatcher::addVirtualPath(const VirtualPath& virtualPath)
{
	FastMutex::ScopedLock lock(_mutex);

	if (!virtualPath.security.csrfTokenHeader.empty())
	{
		_csrfTokenHeaders.insert(virtualPath.security.csrfTokenHeader);
	}

	if (virtualPath.pPattern != nullptr)
	{
		_patternVec.push_back(virtualPath);

		std::string msg("Pattern '");
		msg += virtualPath.path;
		msg += "' mapped by bundle ";
		msg += virtualPath.pOwnerBundle->symbolicName();
		msg += " to request handler.";
		_pContext->logger().information(msg);
	}
	else
	{
		VirtualPath vPath(virtualPath);
		vPath.path = normalizePath(vPath.path);
		Poco::Path path(vPath.path);
		PathMap::iterator itTmp;

		// we have to check if a parent entry exists, start with /
		std::string entry("/");
		PathMap::const_iterator it = _pathMap.find(entry);
		if (it != _pathMap.end())
		{
			if (it->second.security.mode < vPath.security.mode || vPath.path == entry || it->second.security.mode == SM_NONE ||
				(it->second.security.mode == SM_OWNER && vPath.pOwnerBundle != it->second.pOwnerBundle))
				throw Poco::InvalidArgumentException("A protected path mapping (" + it->second.path + ") already exists for that virtual path: " + vPath.path);
		}
		int depth = path.depth();
		for (int i = 0; i < depth; ++i)
		{
			entry += path[i];
			entry += '/';
			it = _pathMap.find(entry);
			if (it != _pathMap.end())
			{
				if (it->second.security.mode < vPath.security.mode || vPath.path == entry || it->second.security.mode == SM_NONE ||
					(it->second.security.mode == SM_OWNER && vPath.pOwnerBundle != it->second.pOwnerBundle))
					throw Poco::InvalidArgumentException("A protected path mapping (" + it->second.path + ") already exists for that virtual path: " + vPath.path);
			}
		}
		poco_assert_dbg (entry == vPath.path);
		// temporary insert the mapping
		itTmp = _pathMap.insert(std::make_pair(vPath.path, vPath)).first;

		it = itTmp;
		++it;
		// we also have to check that no children exist that would violate the specializationMode
		const std::string lcPath = Poco::UTF8::toLower(vPath.path);
		while (it != _pathMap.end() && Poco::UTF8::toLower(it->first).compare(0, lcPath.size(), lcPath) == 0)
		{
			// search for a child that has a less stricter mode than the parent or that has mode none
			// or search for a child that has mode SM_OWNER and a different owner
			if (it->second.security.mode > vPath.security.mode || vPath.security.mode == SM_NONE ||
				(vPath.security.mode == SM_OWNER && vPath.pOwnerBundle != it->second.pOwnerBundle))
			{
				_pathMap.erase(itTmp);
				throw Poco::InvalidArgumentException("A protected child mapping (" + it->second.path + ") already exists for that virtual path: " + vPath.path);
			}
			++it;
		}

		std::string msg("Virtual path '");
		msg += vPath.path;
		msg += "' mapped by bundle ";
		msg += vPath.pOwnerBundle->symbolicName();
		msg += " to ";
		if (!vPath.resource.empty())
		{
			msg += "resource path '";
			msg += vPath.resource;
			msg += "'";
		}
		else msg += "request handler";
		msg += ".";
		_pContext->logger().information(msg);
	}
}


void WebServerDispatcher::removeVirtualPath(const std::string& virtualPath)
{
	FastMutex::ScopedLock lock(_mutex);

	std::string vPath(normalizePath(virtualPath));
	if (_pathMap.erase(vPath) != 0)
	{
		std::string msg("Virtual path '");
		msg += vPath;
		msg += "' unmapped.";
		_pContext->logger().information(msg);
	}
	else
	{
		// The same pattern can be mapped several times, for different methods.
		const auto itRemoved = std::remove_if(_patternVec.begin(), _patternVec.end(), [&virtualPath](const VirtualPath& pattern) { return pattern.path == virtualPath; });
		if (itRemoved != _patternVec.end())
		{
			_patternVec.erase(itRemoved, _patternVec.end());
			std::string msg("Pattern '");
			msg += virtualPath;
			msg += "' unmapped.";
			_pContext->logger().information(msg);
		}
	}
}


void WebServerDispatcher::listVirtualPaths(PathInfoMap& paths) const
{
	FastMutex::ScopedLock lock(_mutex);

	paths.clear();
	for (PathMap::const_iterator it = _pathMap.begin(); it != _pathMap.end(); ++it)
	{
		if (!it->second.hidden)
		{
			PathInfo info;
			info.description = it->second.description;
			info.permission  = it->second.security.permission;
			info.session     = it->second.security.session;
			info.pBundle     = it->second.pOwnerBundle;
			paths[it->first] = info;
		}
	}
}


void WebServerDispatcher::virtualPathMappings(PathMap& mappings) const
{
	FastMutex::ScopedLock lock(_mutex);

	mappings = _pathMap;
}


void WebServerDispatcher::addFilter(const std::string& mediaType, WebFilterFactoryPtr pFilterFactory,
	const WebFilter::Args& args, const Bundle::ConstPtr& pOwnerBundle)
{
	Poco::FastMutex::ScopedLock lock(_filterFactoryMutex);

	if (!_filterFactoryMap.contains(mediaType))
	{
		_filterFactoryMap[mediaType] = WebFilterFactoryInfo{std::move(pFilterFactory), args, pOwnerBundle};
	}
	else throw Poco::ExistsException("WebFilter", mediaType);
}


void WebServerDispatcher::removeFilter(const std::string& mediaType)
{
	FilterFactoryMap::node_type filter;
	{
		Poco::FastMutex::ScopedLock lock(_filterFactoryMutex);

		filter = _filterFactoryMap.extract(mediaType);
	}
	// The sessions that exist now can hold values that the library of the filter has created.
	if (!filter.empty())
	{
		retainForSessions(filter.mapped().pOwnerBundle,
			std::make_shared<const WebFilterFactoryPtr>(std::move(filter.mapped().pFactory)));
	}
}


void WebServerDispatcher::handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response, bool secure)
{
	Poco::Clock startTime;
	std::string username;
	try
	{
		if (_logFullRequest && _pContext->logger().information())
		{
			// Not HTTPRequest::write(), which can reject control characters that HTTPRequest::read() accepts.
			// Escaped here, so that a CR or LF in a field does not start a line.
			_pContext->logger().information("Request from %s:\n\t%s"s, request.clientAddress().toString(),
				loggedMessageHead(loggedRequestLine(request), request, csrfTokenHeaders()));
		}

		addCustomResponseHeaders(response);

		std::string path;
		bool validPath = false;
		try
		{
			path = URI(request.getURI()).getPath();
			validPath = cleanPath(path);
		}
		catch (Poco::SyntaxException& exc)
		{
			_pContext->logger().debug("Request for %s answered with 400 Bad Request: %s"s, loggedText(request.getURI()),
				loggedText(exc.displayText()));
		}
		if (validPath)
		{
			VirtualPath vPath;
			{
				// The mapping is copied, so that neither authorization nor the response
				// holds the lock, and a bundle that stops meanwhile cannot remove it.
				FastMutex::ScopedLock lock(_mutex);
				vPath = mapPath(path, request.getMethod());
			}
			if (vPath.pFactory == nullptr && !isResourcePath(path))
			{
				sendBadRequest(request, formatMessage("invalid"s));
			}
			else if (vPath.security.secure && !secure)
			{
				sendResponse(request, HTTPResponse::HTTP_FORBIDDEN, formatMessage("secure"s, vPath.path), vPath.responseFormat);
			}
			else if (handleCORS(request, response, vPath))
			{
				std::string scope;
				bool authenticated = false;
				// With optional credentials ("**"), a request without credentials needs no auth service.
				if (!vPath.security.permission.empty() && authService() == nullptr && (vPath.security.permission != "**" || request.hasCredentials() || sessionNamesUser(request, vPath)))
				{
					_pContext->logger().debug(
						"Request for %s answered with 503 Service Unavailable: no auth service is available."s,
						loggedText(request.getURI()));
					sendResponse(request, HTTPResponse::HTTP_SERVICE_UNAVAILABLE, formatMessage("unavailable"s, vPath.path), vPath.responseFormat);
				}
				else if (authorize(request, vPath, username, scope, authenticated))
				{
					if (vPath.pFactory != nullptr)
					{
						if (vPath.methods.empty() || vPath.methods.contains(request.getMethod()))
						{
							// Runs after the handler has been destroyed: the bundle can have stopped meanwhile.
							const AtScopeEnd retainUnmappedFactory([this, &vPath]()
								{
									retainIfUnmapped(vPath);
								});
							std::optional<Poco::OSP::Auth::RequestIdentity> identity;
							if (!username.empty()) identity.emplace(username, scope);
							std::unique_ptr<HTTPRequestHandler> pHandler(vPath.pFactory->createRequestHandler(request));
							try
							{
								if (pHandler != nullptr)
									pHandler->handleRequest(request, response);
								else
									sendNotFound(request, path, vPath.responseFormat);
							}
							catch (Poco::Exception& exc)
							{
								throw Poco::UnhandledException("Request Handler", exc);
							}
						}
						else
						{
							sendMethodNotAllowed(request, formatMessage("method"s, request.getMethod(), request.getURI()), vPath.responseFormat);
						}
					}
					else // static resource
					{
						// The request path can name the mapping in another case or normalization form,
						// and so with another length: the resource path follows the mapping's segments.
						std::string::size_type resPos = 0;
						for (int i = Path(vPath.path, Path::PATH_UNIX).depth(); i > 0; --i)
						{
							resPos = path.find('/', path.find_first_not_of('/', resPos));
						}
						if (resPos != std::string::npos)
						{
							std::string resPath(path, resPos + 1, std::string::npos);
							std::string index(vPath.indexPage);
							if (index.empty()) index = "index.html";
							sendResource(request, path, vPath.path, resPath, vPath.resource, index, vPath.pOwnerBundle, vPath.cache);
						}
						else
						{
							sendFound(request, canonicalPathWithQuery(vPath.path, request.getURI()));
						}
					}
				}
				else // !authorize()
				{
					// Only directory/static mappings redirect a failed GET to a friendly HTML page.
					// Request-handler (REST) mappings always answer with a 401 so API clients get a
					// machine-readable error instead of being bounced to the login HTML.
					const bool redirect = vPath.pFactory == nullptr && !vPath.redirectOn401.empty() && request.getMethod() == Poco::Net::HTTPRequest::HTTP_GET;
					if (authenticated && (request.hasCredentials() || !redirect))
					{
						// Valid credentials or a session without the permission: authenticating again would not help.
						sendResponse(request, HTTPResponse::HTTP_FORBIDDEN, formatMessage("notauth"s, vPath.path), vPath.pFactory != nullptr ? RESPONSE_FORMAT_JSON : vPath.responseFormat);
					}
					else if (redirect)
					{
						if (authenticated)
						{
							// The user is logged in but lacks the permission required for this
							// resource. Re-authenticating would not help, so send them to the
							// home page rather than the login page, and omit the redirect= param
							// that would otherwise bounce them straight back to a page they
							// cannot access after a pointless re-login. Guard against a redirect
							// loop should the home page itself be forbidden.
							const std::string homeLocation("/");
							if (path != homeLocation)
							{
								response.redirect(homeLocation);
							}
							else
							{
								sendResponse(request, HTTPResponse::HTTP_FORBIDDEN,
									formatMessage("notauth"s, vPath.path), vPath.responseFormat);
							}
						}
						else
						{
							response.redirect(redirectLocationOn401(vPath.redirectOn401, request.getURI()));
						}
					}
					else
					{
						if (vPath.security.session.empty())
						{
							const std::string realm(vPath.security.realm.empty() ? vPath.path : vPath.security.realm);
							int authMethods = vPath.security.authMethods;
							if (authMethods == 0) authMethods = _authMethods;
							if ((authMethods & AUTH_BEARER) != 0 && (authMethods & AUTH_BASIC) == 0)
							{
								// A bearer-only route must not challenge with Basic (which makes
								// HTTP tooling prompt for a password and confuses OAuth-capable
								// clients that key their auth discovery off the scheme).
								response.setStatusAndReason(HTTPResponse::HTTP_UNAUTHORIZED);
								response.set("WWW-Authenticate"s, Poco::format("Bearer realm=\"%s\""s, realm));
							}
							else
							{
								response.requireAuthentication(realm);
							}
						}
						// REST endpoints always get a JSON 401 payload regardless of the Accept header;
						// directory mappings keep their configured response format.
						ResponseFormat format = vPath.pFactory != nullptr ? RESPONSE_FORMAT_JSON : vPath.responseFormat;
						sendNotAuthorized(request, vPath.path, format);
					}
				}
			}
		}
		else
		{
			sendBadRequest(request, formatMessage("invalid"s));
		}
	}
	catch (Poco::NotFoundException&)
	{
		try
		{
			sendNotFound(request, request.getURI());
		}
		catch (Poco::Exception& exc)
		{
			_pContext->logger().error("Exception while sending Not Found response: %s"s, exc.displayText());
		}
	}
	catch (Poco::Exception& exc)
	{
		try
		{
			std::string reference = Poco::UUIDGenerator::defaultGenerator().createRandom().toString();
			std::string msg("Error processing ");
			msg += request.getMethod();
			msg += " request for \"";
			msg += request.getURI();
			msg += "\" from ";
			msg += request.clientAddress().toString();
			msg += ": ";
			msg += exc.displayText();
			if (exc.nested() != nullptr)
			{
				msg += ": ";
				msg += exc.nested()->displayText();
			}
			msg += " [reference: ";
			msg += reference;
			msg += "]";
			// The exception texts can contain request data as well.
			_pContext->logger().error(loggedText(msg));
			sendInternalError(request, formatMessage("internal"s, reference));
		}
		catch (Poco::Exception& exc)
		{
			_pContext->logger().error("Exception while sending error response: %s"s, exc.displayText());
		}
	}

	// On a persistent connection, a request body that has not been read to its end would be
	// taken for the next request. The connection is closed instead of reading a body of any size.
	if (request.getChunkedTransferEncoding() || request.getContentLength64() > 0)
	{
		if (!request.stream().eof() && Poco::icompare(request.get(Poco::Net::HTTPMessage::CONNECTION, ""s), Poco::Net::HTTPRequest::UPGRADE) != 0)
		{
			response.setKeepAlive(false);
		}
	}

	if (_logFullRequest && _pContext->logger().information())
	{
		std::ostringstream ostr;
		ostr << "Response to " << request.clientAddress().toString();
		if (!username.empty()) ostr << " [user: " << Poco::UTF8::escape(username) << "]";
		ostr << ":\n\t";
		// Not HTTPResponse::write(), which can reject control characters, such as those of a request field
		// copied into a response field.
		std::string statusLine(loggedText(response.getVersion()));
		statusLine += ' ';
		statusLine += Poco::NumberFormatter::format(static_cast<int>(response.getStatus()));
		statusLine += ' ';
		statusLine += loggedText(response.getReason());
		ostr << loggedMessageHead(statusLine, response, csrfTokenHeaders());
		_pContext->logger().information(ostr.str());
	}

	Poco::Clock::ClockDiff duration = startTime.elapsed();
	if (_accessLogger.information())
	{
		logRequest(request, response, username, duration);
	}

	try
	{
		RequestHandledEvent ev(request, response, duration);
		requestHandled(ev);
	}
	catch (Poco::Exception& exc)
	{
		_pContext->logger().error("Delegate for requestHandled event leaked exception: %s"s, exc.displayText());
	}
	catch (std::exception& exc)
	{
		_pContext->logger().error("Delegate for requestHandled event leaked exception: %s"s, std::string(exc.what()));
	}
	catch (...)
	{
		_pContext->logger().error("Delegate for requestHandled event leaked unknown exception."s);
	}
}


void WebServerDispatcher::logRequest(const Poco::Net::HTTPServerRequest& request, const Poco::Net::HTTPServerResponse& response, const std::string& username, Poco::Clock::ClockDiff duration)
{
	Poco::Message message(_accessLogger.name(), loggedRequestLine(request), Poco::Message::PRIO_INFORMATION);

	if (username.empty())
		message["username"s] = "-";
	else
		message["username"s] = Poco::UTF8::escape(username);

	message["status"s] = Poco::NumberFormatter::format(static_cast<int>(response.getStatus()));
	message["client"s] = request.clientAddress().host().toString();
	message["ffclient"s] = realClientAddress(request).host().toString();

	if (response.getContentLength64() != Poco::Net::HTTPMessage::UNKNOWN_CONTENT_LENGTH)
		message["size"s] = Poco::NumberFormatter::format(response.getContentLength64());
	else
		message["size"s] = "-";

	message["host"s] = loggedText(request.get("Host"s, ""s));
	message["referer"s] = loggedText(request.get("Referer"s, ""s));
	message["useragent"s] = loggedText(request.get("User-Agent"s, ""s));
	message["duration"s] = Poco::NumberFormatter::format(static_cast<double>(duration)/Poco::Clock::resolution(), 6);

	_accessLogger.log(message);
}


bool WebServerDispatcher::parseRange(const std::string& range, Poco::File::FileSize fileSize, Poco::File::FileSize& rangeStart, Poco::File::FileSize& rangeLength)
{
	const std::string::size_type eqPos = range.find('=');
	if (eqPos == std::string::npos || Poco::icompare(range, 0, eqPos, "bytes"s) != 0) return false;

	// Every range must be valid; only the first satisfiable one is sent,
	// as the dispatcher does not send multipart responses.
	Poco::StringTokenizer specs(range.substr(eqPos + 1), ","s, Poco::StringTokenizer::TOK_TRIM | Poco::StringTokenizer::TOK_IGNORE_EMPTY);
	bool found = false;
	for (const auto& spec: specs)
	{
		bool satisfiable = false;
		Poco::File::FileSize start = 0;
		Poco::File::FileSize length = 0;
		if (!parseRangeSpec(spec, fileSize, satisfiable, start, length)) return false;
		if (satisfiable && !found)
		{
			rangeStart = start;
			rangeLength = length;
			found = true;
		}
	}
	return found;
}


void WebServerDispatcher::sendFileRange(Poco::Net::HTTPServerResponse& response, std::istream& resourceStream, Poco::File::FileSize length, Poco::File::FileSize rangeStart, Poco::File::FileSize rangeLength)
{
	response.setStatusAndReason(Poco::Net::HTTPResponse::HTTP_PARTIAL_CONTENT);
#if defined(POCO_HAVE_INT64)	
		response.setContentLength64(rangeLength);
#else
		response.setContentLength(static_cast<int>(rangeLength));
#endif
		response.set("Content-Range", Poco::format("bytes %?d-%?d/%?d", rangeStart, rangeStart + rangeLength - 1, length));
		std::ostream& ostr = response.send();

		resourceStream.seekg(static_cast<std::streamoff>(rangeStart));
		if (!resourceStream)
		{
			// A resource stream from a ZIP bundle cannot seek; skip to the range by reading.
			resourceStream.clear();
			resourceStream.ignore(static_cast<std::streamsize>(rangeStart));
		}
		Poco::Buffer<char> buffer(BUFFER_SIZE);
		Poco::File::FileSize copied = 0;
		while (copied < rangeLength && resourceStream.good() && ostr.good())
		{
			resourceStream.read(buffer.begin(), static_cast<std::streamsize>(std::min<Poco::File::FileSize>(BUFFER_SIZE, rangeLength - copied)));
			ostr.write(buffer.begin(), resourceStream.gcount());
			copied += static_cast<Poco::File::FileSize>(resourceStream.gcount());
		}
		// The client waits for the announced length, which a shorter resource cannot provide.
		if (copied < rangeLength) response.setKeepAlive(false);
}


void WebServerDispatcher::sendResource(Poco::Net::HTTPServerRequest& request, const std::string& path, const std::string& vpath, const std::string& resPath, const std::string& resBase, const std::string& index, Bundle::ConstPtr pBundle, bool canCache)
{
	const std::string& meth(request.getMethod());
	Poco::Net::HTTPServerResponse& response(request.response());
	std::string mediaType;
	std::string resolvedPath;
	Poco::File::FileSize resourceSize = 0;
	Poco::Timestamp lastModified;
	std::unique_ptr<std::istream> pResourceStream(findResource(pBundle, resBase, resPath, index, mediaType, resolvedPath, canCache, &resourceSize, &lastModified));
	if (pResourceStream != nullptr)
	{
		response.setContentType(mediaType);

		WebFilterFactoryInfo filterFactoryInfo;
		// Runs after the filter has been destroyed: the filter can have been removed meanwhile.
		const AtScopeEnd retainRemovedFilterFactory([this, &mediaType, &filterFactoryInfo]()
			{
				retainIfRemoved(mediaType, filterFactoryInfo);
			});
		Poco::SharedPtr<WebFilter> pFilter = findFilterAndFactory(mediaType, filterFactoryInfo);
		if (pFilter != nullptr)
		{
			response.setChunkedTransferEncoding(true);
			pFilter->process(request, response, resolvedPath, *pResourceStream, pBundle);
		}
		else if (meth == "GET" || meth == "HEAD")
		{
			// A Range header field with a unit other than bytes is ignored (RFC 9110 14.2).
			static const std::string BYTES_RANGE("bytes=");
			bool acceptRangeResponse (_enableAcceptRange && Poco::icompare(request.get("Range"s, ""s), 0, BYTES_RANGE.size(), BYTES_RANGE) == 0);
			if(_enableAcceptRange) response.set("Accept-Ranges", "bytes");
			const bool compressible(_compressResponses && shouldCompressMediaType(mediaType));
			// The body depends on Accept-Encoding whenever the media type can be compressed.
			if (compressible) response.add("Vary"s, "Accept-Encoding"s);

			if (request.has("If-Modified-Since"s))
			{
				Poco::DateTime modifiedSince;
				int tzd;
				// An invalid date is ignored (RFC 9110 13.1.3).
				if (Poco::DateTimeParser::tryParse(request.get("If-Modified-Since"s), modifiedSince, tzd) && lastModified <= modifiedSince.timestamp())
				{
					response.setContentLength(0);
					response.setStatusAndReason(Poco::Net::HTTPResponse::HTTP_NOT_MODIFIED);
					response.send();
					return;
				}
			}

			if (meth == "HEAD")
			{
#if defined(POCO_HAVE_INT64)
				response.setContentLength64(static_cast<Poco::Int64>(resourceSize));
#else
				response.setContentLength(static_cast<int>(resourceSize));
#endif
				response.send();
				return;
			}
			else if(meth == "GET")
			{
				if (acceptRangeResponse)
				{
					Poco::File::FileSize rangeStart  = 0;
					Poco::File::FileSize rangeLength = 0;
					if (parseRange(request.get("Range"), resourceSize, rangeStart, rangeLength))
					{
						sendFileRange(response, *pResourceStream, resourceSize, rangeStart, rangeLength);
					}
					else
					{
						response.setStatusAndReason(Poco::Net::HTTPResponse::HTTP_REQUESTED_RANGE_NOT_SATISFIABLE);
						response.set("Content-Range"s, Poco::format("bytes */%?d"s, resourceSize));
						response.setContentLength(0);
						response.send();
						return;
					}
				}
				else
				{
					bool compressResponse(compressible && acceptsGzip(request));
					if (compressResponse)
					{
						// The gzip-encoded length is not known up front, so frame
						// the response as chunked and omit Content-Length. Setting
						// both would violate RFC 7230 3.3.3 (and advertise the
						// uncompressed size, which is wrong for the encoded body).
						response.set("Content-Encoding"s, "gzip"s);
						response.setChunkedTransferEncoding(true);
					}
					else
					{
						// The exact size is known, so send an accurate
						// Content-Length and let the body be identity-framed.
						response.setChunkedTransferEncoding(false);
						response.setContentLength64(static_cast<Poco::Int64>(resourceSize));
					}

					std::ostream& responseStream = response.send();

					if (compressResponse)
					{
						Poco::DeflatingOutputStream gzipStream(responseStream, Poco::DeflatingStreamBuf::STREAM_GZIP, 1);
						StreamCopier::copyStream(*pResourceStream, gzipStream);
					}
					else
					{
						StreamCopier::copyStream(*pResourceStream, responseStream);
					}
				}
			}
		}
		else
		{
			sendMethodNotAllowed(request, formatMessage("method"s, meth, request.getURI()));
		}
	}
	else if (path.size() == vpath.size())
	{
		sendForbidden(request, path);
	}
	else
	{
		sendNotFound(request, path);
	}
}


WebServerDispatcher::WebFilterPtr WebServerDispatcher::findFilter(const std::string& mediaType)
{
	WebFilterFactoryInfo factoryInfo;
	return findFilterAndFactory(mediaType, factoryInfo);
}


WebServerDispatcher::WebFilterPtr WebServerDispatcher::findFilterAndFactory(const std::string& mediaType,
	WebFilterFactoryInfo& factoryInfo)
{
	WebFilterPtr pFilter;
	Poco::ScopedLockWithUnlock<Poco::FastMutex> lock(_filterFactoryMutex);
	FilterFactoryMap::iterator it = _filterFactoryMap.find(mediaType);
	if (it != _filterFactoryMap.end())
	{
		factoryInfo = it->second;
		lock.unlock();
		pFilter = factoryInfo.pFactory->createFilter(factoryInfo.args);
	}
	return pFilter;
}


void WebServerDispatcher::removeBundle(Bundle::ConstPtr pBundle)
{
	FastMutex::ScopedLock lock(_mutex);

	PathMap::iterator itm = _pathMap.begin();
	while (itm != _pathMap.end())
	{
		if (itm->second.pOwnerBundle == pBundle)
		{
			std::string path(itm->first);
			PathMap::iterator delIt(itm);
			++itm;
			_pathMap.erase(delIt);
			std::string msg("Virtual path '");
			msg += path;
			msg += "' unmapped because its bundle (";
			msg += pBundle->symbolicName();
			msg += ") is stopping.";
			_pContext->logger().information(msg);
		}
		else ++itm;
	}

	PatternVec::iterator itv = _patternVec.begin();
	while (itv != _patternVec.end())
	{
		if (itv->pOwnerBundle == pBundle)
		{
			std::string pattern(itv->path);
			itv = _patternVec.erase(itv);
			std::string msg("Pattern '");
			msg += pattern;
			msg += "' unmapped because its bundle (";
			msg += pBundle->symbolicName();
			msg += ") is stopping.";
			_pContext->logger().information(msg);
		}
		else
		{
			++itv;
		}
	}
}


void WebServerDispatcher::uncacheBundleResources(Bundle::ConstPtr pBundle)
{
	std::string cachePath = "//";
	cachePath += pBundle->symbolicName();
	cachePath += "/";

	Poco::FastMutex::ScopedLock lock(_resourceCacheMutex);

	ResourceCache::iterator it = _resourceCache.begin();
	ResourceCache::iterator end = _resourceCache.end();
	while (it != end)
	{
		if (it->first.compare(0, cachePath.size(), cachePath) == 0)
		{
			ResourceCache::iterator itDel = it;
			++it;
			_resourceCache.erase(itDel);
		}
		else ++it;
	}
}


void WebServerDispatcher::onBundleStopping(const void*, BundleEvent& ev)
{
	Bundle::ConstPtr pBundle = ev.bundle();
	{
		FastMutex::ScopedLock lock(_mutex);

		std::vector<RequestHandlerFactoryPtr>& factories = _stoppingBundleFactories[pBundle->id()];
		const auto keepFactory = [&factories, &pBundle](const VirtualPath& vPath)
		{
			if (vPath.pOwnerBundle == pBundle && vPath.pFactory != nullptr)
			{
				factories.push_back(vPath.pFactory);
			}
		};
		for (const auto& mapping: _pathMap)
		{
			keepFactory(mapping.second);
		}
		for (const auto& vPath: _patternVec)
		{
			keepFactory(vPath);
		}
	}
	removeBundle(pBundle);
	uncacheBundleResources(pBundle);
}


void WebServerDispatcher::onBundleStopped(const void*, BundleEvent& ev)
{
	decltype(_stoppingBundleFactories)::node_type factories;
	{
		FastMutex::ScopedLock lock(_mutex);

		factories = _stoppingBundleFactories.extract(ev.bundle()->id());
	}
	// The sessions that exist now can hold values that the libraries of the bundle have created.
	if (!factories.empty() && !factories.mapped().empty())
	{
		retainForSessions(ev.bundle(),
			std::make_shared<const std::vector<RequestHandlerFactoryPtr>>(std::move(factories.mapped())));
	}
}


void WebServerDispatcher::onBundleUninstalling(const void*, BundleEvent& ev)
{
	decltype(_sessionRetainedObjects)::node_type retained;
	{
		FastMutex::ScopedLock lock(_mutex);

		retained = _sessionRetainedObjects.extract(ev.bundle()->id());
	}
	WebSessionManager::Ptr pSessionManager = sessionManager();
	if (retained.empty() || pSessionManager == nullptr) return;

	for (const auto& pRetained: retained.mapped())
	{
		const std::shared_ptr<const void> pObject = pRetained.lock();
		if (pObject != nullptr)
		{
			pSessionManager->removeSessionsRetaining(pObject);
		}
	}
	// The last reference to an object is released here, unless a request still uses a removed session.
}


void WebServerDispatcher::retainForSessions(const Bundle::ConstPtr& pBundle, const std::shared_ptr<const void>& pObject)
{
	// The caller releases its reference to pObject outside the locks: releasing the last one can unload a library.
	WebSessionManager::Ptr pSessionManager = sessionManager();
	if (pSessionManager == nullptr) return;

	FastMutex::ScopedLock lock(_mutex);

	if (pBundle != nullptr)
	{
		std::vector<std::weak_ptr<const void>>& retained = _sessionRetainedObjects[pBundle->id()];
		retained.erase(std::remove_if(retained.begin(), retained.end(), [](const std::weak_ptr<const void>& pRetained)
			{
				return pRetained.expired();
			}), retained.end());
		retained.push_back(pObject);
	}
	// Under the lock, so that onBundleUninstalling() finds every session that gets the object.
	pSessionManager->retainUntilSessionsEnd(pObject);
}


void WebServerDispatcher::retainIfUnmapped(const VirtualPath& vPath)
{
	if (vPath.pFactory == nullptr || vPath.pOwnerBundle == nullptr || vPath.pOwnerBundle->isActive()) return;

	bool mapped = false;
	{
		FastMutex::ScopedLock lock(_mutex);

		const auto hasFactory = [&vPath](const VirtualPath& mapping)
		{
			return mapping.pFactory == vPath.pFactory;
		};
		mapped = std::any_of(_pathMap.begin(), _pathMap.end(), [&hasFactory](const PathMap::value_type& mapping)
			{
				return hasFactory(mapping.second);
			}) || std::any_of(_patternVec.begin(), _patternVec.end(), hasFactory);
	}
	if (!mapped)
	{
		retainForSessions(vPath.pOwnerBundle, std::make_shared<const RequestHandlerFactoryPtr>(vPath.pFactory));
	}
}


void WebServerDispatcher::retainIfRemoved(const std::string& mediaType, const WebFilterFactoryInfo& factoryInfo)
{
	if (factoryInfo.pFactory == nullptr) return;

	bool removed = false;
	{
		Poco::FastMutex::ScopedLock lock(_filterFactoryMutex);

		const FilterFactoryMap::const_iterator it = _filterFactoryMap.find(mediaType);
		removed = it == _filterFactoryMap.end() || it->second.pFactory != factoryInfo.pFactory;
	}
	if (removed)
	{
		retainForSessions(factoryInfo.pOwnerBundle, std::make_shared<const WebFilterFactoryPtr>(factoryInfo.pFactory));
	}
}


std::string WebServerDispatcher::normalizePath(const std::string& path)
{
	if (!path.empty())
	{
		std::string absPath("/");
		if (path[0] != '/')
			absPath += path;
		else
			absPath = path;
		if (absPath != "/")
		{
			Poco::Path p(absPath, Poco::Path::PATH_UNIX);
			p.makeDirectory();
			try
			{
				return Poco::UTF8::normalize(p.toString(Poco::Path::PATH_UNIX), Poco::UTF8::NORMALIZATION_FORM_C);
			}
			catch (Poco::RuntimeException&)
			{
				// The path is not UTF-8, so it has no other normalization form.
				return p.toString(Poco::Path::PATH_UNIX);
			}
		}
	}
	return "/"s;
}


const WebServerDispatcher::VirtualPath& WebServerDispatcher::mapPath(const std::string& path, const std::string& method) const
{
	PatternVec::const_iterator foundItv = _patternVec.end();
	for (PatternVec::const_iterator itv = _patternVec.begin(); itv != _patternVec.end(); ++itv)
	{
		if (itv->pPattern->match(path))
		{
			if (foundItv == _patternVec.end())
			{
				// Return something matching the pattern even if methods don't match.
				// Methods will be checked by caller, so a proper 405 can be returned.
				foundItv = itv;
			}
			else
			{
				// Already have something, but this also matches method.
				if (itv->methods.empty() || itv->methods.contains(method))
					foundItv = itv;
			}
		}
	}
	if (foundItv != _patternVec.end())
	{
		return *foundItv;
	}
	else
	{
		std::string normPath(normalizePath(path));
		Path p(normPath, Path::PATH_UNIX);
		std::string parent("/");
		PathMap::const_iterator foundIt = _pathMap.find(parent);
		for (int i = 0; i < p.depth(); ++i)
		{
			parent += p[i];
			parent += '/';
			PathMap::const_iterator it = _pathMap.find(parent);
			if (it != _pathMap.end() && it->second.pPattern == nullptr)
				foundIt = it;
		}
		if (foundIt != _pathMap.end())
		{
			if (!foundIt->second.exactMatch || Poco::UTF8::icompare(foundIt->second.path, normPath) == 0)
				return foundIt->second;
		}
		throw Poco::NotFoundException(path);
	}
}


std::istream* WebServerDispatcher::findResource(Bundle::ConstPtr pBundle, const std::string& base, const std::string& res, const std::string& index, std::string& mediaType, std::string& resolvedPath, bool canCache, Poco::File::FileSize* pSize, Poco::Timestamp* pLastModified) const
{
	// Build bundle-internal (UNIX) path for requested resource
	Path basePath(base, Path::PATH_UNIX);
	basePath.makeDirectory();
	Path resPath(res, Path::PATH_UNIX);
	basePath.append(resPath);
	resolvedPath = basePath.toString(Path::PATH_UNIX);
	mediaType = _pMediaTypeMapper->map(basePath.getExtension());

	// 1) If configured, try to serve from the bundle's persistent directory
	if (isPersistentResource(pBundle, base))
	{
		try
		{
			Poco::OSP::BundleContext::Ptr pApiContext = _pContext->contextForBundle(pBundle);
			if (pApiContext != nullptr)
			{
				// FS root: <persistentDir>/<base>
				Poco::Path fsRoot(pApiContext->persistentDirectory());
				fsRoot.pushDirectory(base);
				fsRoot.makeAbsolute();
				const std::string fsRootPath(fsRoot.toString());
				const auto isBelowRoot = [&fsRootPath](const Poco::Path& p)
				{
					return p.toString().compare(0, fsRootPath.size(), fsRootPath) == 0;
				};

				// Candidate file: <fsRoot>/<res>. resolve() removes dot segments, so
				// a candidate that leaves fsRoot no longer starts with it. A res that
				// starts with '/' (from "//" in the request) would replace fsRoot.
				Poco::Path candidate(fsRoot);
				candidate.resolve(Poco::Path(res.substr(std::min(res.find_first_not_of('/'), res.size())),
					Poco::Path::PATH_UNIX));
				if (!isBelowRoot(candidate)) return nullptr;

				Poco::File f(candidate);
				if (f.exists() && f.isFile() && BundleDirectory::isExactPath(fsRoot, candidate))
				{
					if (pSize != nullptr) *pSize = f.getSize();
					if (pLastModified != nullptr) *pLastModified = f.getLastModified();
					return new Poco::FileInputStream(candidate.toString());
				}

				// Index fallback: <fsRoot>/<res>/index.html
				Poco::Path idx(candidate);
				idx.makeDirectory();
				idx.setFileName(index);
				if (!isBelowRoot(idx)) return nullptr;
				Poco::File fi(idx);
				if (fi.exists() && fi.isFile() && BundleDirectory::isExactPath(fsRoot, idx))
				{
					// Update resolvedPath and media type to reflect index
					Path b(base, Path::PATH_UNIX);
					Path r(res, Path::PATH_UNIX);
					Path dir(b);
					dir.append(r);
					dir.makeDirectory();
					dir.setFileName(index);
					resolvedPath = dir.toString(Path::PATH_UNIX);
					mediaType = _pMediaTypeMapper->map(Path(resolvedPath, Path::PATH_UNIX).getExtension());

					if (pSize != nullptr) *pSize = fi.getSize();
					if (pLastModified != nullptr) *pLastModified = fi.getLastModified();
					return new Poco::FileInputStream(idx.toString());
				}

				// Strict FS? If yes, stop here so caller returns 404.
				if (isStrictFS(pBundle))
					return nullptr;
			}
		}
		catch (Poco::Exception& exc)
		{
			const bool strict = isStrictFS(pBundle);
			_pContext->logger().warning("Cannot read the persistent copy of resource %s of bundle %s: %s. %s"s,
				resolvedPath, pBundle->symbolicName(), exc.displayText(),
				strict ? "The resource is not served (web.static.strict)."s : "The resource is served from the bundle."s);
			if (strict) return nullptr;
		}
	}

	// 2) Fall back to bundle content (ZIP or directory), optionally using cache
	std::unique_ptr<std::istream> pResourceStream(getCachedResource(pBundle, resolvedPath, canCache, pSize));
	if (pResourceStream == nullptr)
	{
		// 3) Index fallback in bundle
		basePath.makeDirectory();
		basePath.setFileName(index);
		resolvedPath = basePath.toString(Path::PATH_UNIX);
		mediaType = _pMediaTypeMapper->map(basePath.getExtension());
		pResourceStream.reset(getCachedResource(pBundle, resolvedPath, canCache, pSize));
	}
	if (pResourceStream != nullptr && pLastModified != nullptr)
		*pLastModified = resourceLastModified(pBundle, resolvedPath);
	return pResourceStream.release();
}


std::istream* WebServerDispatcher::getCachedResource(Bundle::ConstPtr pBundle, const std::string& path, bool canCache, Poco::File::FileSize* pSize) const
{
	if (_cacheResources && canCache)
	{
		std::string cachePath = "//";
		cachePath += pBundle->symbolicName();
		cachePath += "/";
		cachePath += path;
		Poco::ScopedLockWithUnlock<Poco::FastMutex> lockWithUnlock(_resourceCacheMutex);
		ResourceCache::iterator it = _resourceCache.find(cachePath);
		if (it != _resourceCache.end())
		{
			if (pSize != nullptr) *pSize = it->second->size();
			return new CachedResourceStream(it->second);
		}
		else
		{
			lockWithUnlock.unlock();
			std::unique_ptr<std::istream> pResourceStream(pBundle->getResource(path));
			if (pResourceStream != nullptr)
			{
				auto pCachedData = std::make_shared<std::string>();
				Poco::StreamCopier::copyToString(*pResourceStream, *pCachedData);
				if (pSize != nullptr) *pSize = pCachedData->size();

				Poco::FastMutex::ScopedLock lock(_resourceCacheMutex);
				// emplace() keeps the entry of another thread that has cached the resource in the meantime.
				return new CachedResourceStream(_resourceCache.emplace(cachePath,
					std::move(pCachedData)).first->second);
			}
			else return nullptr;
		}
	}
	else
	{
		_pContext->logger().debug("Cannot cache: %s"s, path);
		std::unique_ptr<std::istream> pResourceStream(pBundle->getResource(path));
		if (pResourceStream != nullptr && pSize != nullptr)
		{
			pResourceStream->seekg(0, std::ios::end);
			if (*pResourceStream)
			{
				*pSize = static_cast<Poco::File::FileSize>(pResourceStream->tellg());
				pResourceStream->seekg(0, std::ios::beg);
			}
			else
			{
				// A resource stream from a ZIP bundle cannot seek, so the size
				// is counted by reading the resource once more.
				pResourceStream->clear();
				std::unique_ptr<std::istream> pCountStream(pBundle->getResource(path));
				if (pCountStream != nullptr)
				{
					pCountStream->ignore(std::numeric_limits<std::streamsize>::max());
					*pSize = static_cast<Poco::File::FileSize>(pCountStream->gcount());
				}
			}
		}
		return pResourceStream.release();
	}
}


bool WebServerDispatcher::cleanPath(std::string& path)
{
	// The file system would see a decoded NUL as the end of the path, and the media type
	// would still come from the extension after it.
	if (std::any_of(path.begin(), path.end(), isControlCharacter)) return false;

	std::string::iterator it(path.begin());
	std::string::iterator end(path.end());
	for (; it != end; ++it)
	{
		if (*it == '\\')
			*it = '/';
	}

	// Dot segments are rejected, not resolved, so that the path mapping, its
	// permission and the resource lookup all work with the same path.
	Poco::StringTokenizer segments(path, "/"s);
	for (const auto& segment: segments)
	{
		if (segment == "." || segment == "..")
			return false;
	}

	while (path.size() > 0 && path[path.size() - 1] == '.')
		path.resize(path.size() - 1);

	// Windows removes a trailing dot or space from every segment when it opens a file,
	// so such a segment would open a file below a mapping that the path does not name.
	return !path.empty() && path.back() != ' ' && path.find("./"s) == std::string::npos
		&& path.find(" /"s) == std::string::npos;
}


bool WebServerDispatcher::isResourcePath(const std::string& path)
{
	if (path.find(':') != std::string::npos) return false;
#if defined(POCO_OS_FAMILY_WINDOWS)
	for (std::string::size_type pos = path.find('~'); pos != std::string::npos; pos = path.find('~', pos + 1))
	{
		if (pos + 1 < path.size() && Poco::Ascii::isDigit(path[pos + 1])) return false;
	}
#endif
	return true;
}


bool WebServerDispatcher::handleCORS(const Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response, const VirtualPath& vPath) const
{
	if (!_corsEnabled || !vPath.cors.enable) return true;

	// The answer depends on the Origin header, which caches must take into account
	// also for responses to requests without a cross-origin Origin.
	const std::string& allowOrigin = allowedOrigin(vPath);
	if (!allowOrigin.empty() && allowOrigin != "*") response.add("Vary"s, "Origin"s);

	if (request.has("Origin"s) && !isSameOrigin(request, request.get("Origin"s)))
	{
		const bool allowed = enableCORS(request, response, vPath);
		if (request.getMethod() == Poco::Net::HTTPRequest::HTTP_OPTIONS)
		{
			if (allowed)
			{
				if (vPath.cors.allowMethods.empty())
				{
					if (vPath.methods.empty())
					{
						response.set("Access-Control-Allow-Methods"s, "*"s);
					}
					else
					{
						response.set("Access-Control-Allow-Methods"s, Poco::cat(", "s, vPath.methods.begin(), vPath.methods.end()));
					}
				}
				else
				{
					response.set("Access-Control-Allow-Methods"s, vPath.cors.allowMethods);
				}
				if (!vPath.cors.allowHeaders.empty())
				{
					response.set("Access-Control-Allow-Headers"s, vPath.cors.allowHeaders);
				}
			}
			else
			{
				response.setStatusAndReason(Poco::Net::HTTPResponse::HTTP_FORBIDDEN);
			}
			response.setContentLength(0);
			response.send();
			return false;
		}
		const std::string& method = request.getMethod();
		const bool safeMethod = method == Poco::Net::HTTPRequest::HTTP_GET || method == Poco::Net::HTTPRequest::HTTP_HEAD;
		if (!allowed && !safeMethod)
		{
			// Blocks requests that another site makes a browser send, such as form posts.
			response.setStatusAndReason(Poco::Net::HTTPResponse::HTTP_FORBIDDEN);
			response.setContentLength(0);
			response.send();
			return false;
		}
		// Safe requests from an origin that is not allowed are answered without CORS headers.
	}

	return true;
}


const std::string& WebServerDispatcher::allowedOrigin(const VirtualPath& vPath) const
{
	return vPath.cors.allowOrigin.empty() ? _corsAllowedOrigin : vPath.cors.allowOrigin;
}


bool WebServerDispatcher::enableCORS(const Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response, const VirtualPath& vPath) const
{
	const std::string& allowOrigin = allowedOrigin(vPath);

	// An empty allowed origin allows no cross-origin access.
	if (allowOrigin.empty()) return false;

	// "*" does not work with credentials in browsers, so it is sent without them.
	if (allowOrigin == "*")
	{
		response.set("Access-Control-Allow-Origin"s, allowOrigin);
		return true;
	}

	const std::string& origin = request.get("Origin"s);
	Poco::StringTokenizer allowedOrigins(allowOrigin, ","s, Poco::StringTokenizer::TOK_TRIM | Poco::StringTokenizer::TOK_IGNORE_EMPTY);
	if (allowOrigin == "**" || allowedOrigins.has(origin))
	{
		response.set("Access-Control-Allow-Origin"s, origin);
		if (vPath.cors.allowCredentials)
		{
			response.set("Access-Control-Allow-Credentials"s, "true"s);
		}
		return true;
	}
	return false;
}


bool WebServerDispatcher::authorize(Poco::Net::HTTPServerRequest& request, const VirtualPath& vPath,
	std::string& username, std::string& scope, bool& authenticated) const
{
	// Only the dispatcher states the authorized user; copies sent by the client are removed.
	request.erase(X_OSP_AUTHORIZED_USER);
	request.erase(X_OSP_AUTHORIZED_SCOPE);

	bool authorized = false;
	authenticated = false;
	int authMethods = vPath.security.authMethods;
	if (authMethods == 0) authMethods = _authMethods;

	if (vPath.security.permission.empty())
	{
		authorized = true;
	}
	else
	{
		if (request.hasCredentials())
		{
			std::string scheme;
			std::string authInfo;
			request.getCredentials(scheme, authInfo);
			try
			{
				if (scheme == Poco::Net::HTTPBasicCredentials::SCHEME && (authMethods & AUTH_BASIC) != 0)
					authorized = authorizeBasic(request, authInfo, vPath, username, authenticated);
				else if (scheme == BEARER && (authMethods & AUTH_BEARER) != 0)
					authorized = authorizeBearer(request, authInfo, vPath, username, scope, authenticated);
			}
			catch (...)
			{
				// The credentials are cleared also when authorization fails with an exception.
				Poco::secureClear(authInfo);
				throw;
			}
			Poco::secureClear(authInfo);
		}
		else if (!vPath.security.session.empty() && (authMethods & AUTH_SESSION) != 0)
		{
			authorized = authorizeSession(request, vPath, username, authenticated);
		}
		else if (vPath.security.permission == "**")
		{
			authorized = true;
		}
		if (authorized && _addAuthHeader && !username.empty())
		{
			request.set(X_OSP_AUTHORIZED_USER, username);
			if (!scope.empty())
			{
				request.set(X_OSP_AUTHORIZED_SCOPE, scope);
			}
		}
	}
	return authorized;
}


bool WebServerDispatcher::authorizeSession(Poco::Net::HTTPServerRequest& request, const VirtualPath& vPath, std::string& username, bool& authenticated) const
{
	WebSessionManager::Ptr pSessionManager = sessionManager();
	if (pSessionManager != nullptr)
	{
		WebSession::Ptr pSession = pSessionManager->find(vPath.security.session, request);
		if (pSession != nullptr)
		{
			username = pSession->getValue<std::string>("username"s, ""s);
			if (!username.empty())
			{
				AuthService::Ptr pAuthService = authService();
				if (pAuthService == nullptr) return false;
				username = knownUserName(*pAuthService, username);
				if (pAuthService->userExists(username))
				{
					if (!pAuthService->userEnabled(username))
					{
						_pContext->logger().debug("Request for %s is not authenticated: the user %s of the session "
							"is disabled or locked out."s, loggedText(request.getURI()), Poco::UTF8::escape(username));
						return false;
					}
					// The session belongs to a real, logged-in user; the request is
					// authenticated even if the permission check below fails.
					authenticated = true;
					if (hasPermission(pAuthService, username, vPath.security.permission))
					{
						if (vPath.security.csrfProtection && !vPath.security.csrfTokenHeader.empty())
						{
							if (request.getMethod() != Poco::Net::HTTPRequest::HTTP_GET && request.getMethod() != Poco::Net::HTTPRequest::HTTP_HEAD)
							{
								if (pSession->isCSRFToken(request.get(vPath.security.csrfTokenHeader, ""s)))
								{
									return true;
								}
								else
								{
									_pContext->logger().warning("User %s failed CSRF check with %s request to %s."s,
										Poco::UTF8::escape(username), loggedText(request.getMethod()),
										loggedText(request.getURI()));
									// The request may have been sent by another site in the user's browser.
									authenticated = false;
									return false;
								}
							}
							else
							{
								return true;
							}
						}
						else
						{
							return true;
						}
					}
					else
					{
						_pContext->logger().warning("User %s does not have the permission (%s) to access %s."s,
							Poco::UTF8::escape(username), vPath.security.permission, loggedText(request.getURI()));
					}
				}
				else
				{
					_pContext->logger().warning("User %s (authenticated via session) does not exist."s, Poco::UTF8::escape(username));
				}
			}
			else if (vPath.security.permission == "**")
			{
				return true;
			}
			else
			{
				_pContext->logger().warning(
					"Failed to authorize user for path %s because session is not authenticated."s,
					loggedText(request.getURI()));
			}
		}
		else if (vPath.security.permission == "**")
		{
			return true;
		}
		else
		{
			_pContext->logger().warning("Failed to authorize user for path %s because no session is available."s,
				loggedText(request.getURI()));
		}
	}
	else if (vPath.security.permission == "**")
	{
		return true;
	}
	else
	{
		_pContext->logger().warning(
			"Failed to authorize user for path %s via session because no WebSessionManager is available."s,
			loggedText(request.getURI()));
	}
	return false;
}


bool WebServerDispatcher::authorizeBasic(Poco::Net::HTTPServerRequest& request, const std::string& creds, const VirtualPath& vPath, std::string& username, bool& authenticated) const
{
	AuthService::Ptr pAuthService = authService();
	if (pAuthService != nullptr)
	{
		HTTPBasicCredentials cred(creds);
		username = cred.getUsername();
		std::string accountName;
		if (pAuthService->authenticateAccount(username, cred.getPassword(), accountName))
		{
			authenticated = true;
			username = knownUserName(*pAuthService, accountName);
			if (hasPermission(pAuthService, username, vPath.security.permission))
			{
				return true;
			}
			else
			{
				_pContext->logger().warning("User %s does not have the permission (%s) to access %s."s,
					Poco::UTF8::escape(username), vPath.security.permission, loggedText(request.getURI()));
			}
		}
		else
		{
			_pContext->logger().warning("User %s failed authentication."s, Poco::UTF8::escape(username));
		}
	}
	return false;
}


bool WebServerDispatcher::authorizeBearer(Poco::Net::HTTPServerRequest& request, const std::string& token, const VirtualPath& vPath, std::string& username, std::string& scope, bool& authenticated) const
{
	AuthService::Ptr pAuthService = authService();
	TokenValidator::Ptr pTokenValidator = tokenValidator();
	if (pTokenValidator == nullptr)
	{
		if (!_tokenValidatorName.empty())
			_pContext->logger().warning(
				"Bearer token for %s cannot be validated: the token validator service (%s) is not available."s,
				loggedText(request.getURI()), _tokenValidatorName);
	}
	else if (pAuthService != nullptr)
	{
		if (pTokenValidator->validateToken(token, username, scope))
		{
			username = knownUserName(*pAuthService, username);
			if (pAuthService->userExists(username))
			{
				if (!pAuthService->userEnabled(username))
				{
					_pContext->logger().debug("Request for %s is not authenticated: the user %s of the bearer token "
						"is disabled or locked out."s, loggedText(request.getURI()), Poco::UTF8::escape(username));
					return false;
				}
				authenticated = true;
				if (hasPermission(pAuthService, username, scope, vPath.security.permission))
				{
					return true;
				}
				else
				{
					_pContext->logger().warning("User %s does not have the permission (%s) to access %s."s,
						Poco::UTF8::escape(username), vPath.security.permission, loggedText(request.getURI()));
				}
			}
			else
			{
				_pContext->logger().warning("User %s (authenticated via token) does not exist."s, Poco::UTF8::escape(username));
			}
		}
		else
		{
			_pContext->logger().warning("Bearer token for %s failed validation."s, loggedText(request.getURI()));
		}
	}
	return false;
}


bool WebServerDispatcher::hasPermission(AuthService::Ptr pAuthService, const std::string& username, const std::string& permission) const
{
	// The AuthService interface defines an empty scope to mean no scope.
	return hasPermission(pAuthService, username, ""s, permission);
}


bool WebServerDispatcher::hasPermission(AuthService::Ptr pAuthService, const std::string& username, const std::string& scope, const std::string& permission) const
{
	if (permission == "*" || permission == "**")
	{
		return true;
	}
	// Comma-separated permissions: the user needs at least one of them.
	Poco::StringTokenizer tok(permission, ","s, Poco::StringTokenizer::TOK_TRIM | Poco::StringTokenizer::TOK_IGNORE_EMPTY);
	for (const auto& perm: tok)
	{
		if (pAuthService->authorize(username, scope, perm))
		{
			return true;
		}
	}
	return false;
}


void WebServerDispatcher::sendFound(Poco::Net::HTTPServerRequest& request, const std::string& path, ResponseFormat format)
{
	request.response().set("Location"s, path);
	sendResponse(request, HTTPResponse::HTTP_FOUND, formatMessage("redirect"s, path), format);
}


void WebServerDispatcher::sendNotFound(Poco::Net::HTTPServerRequest& request, const std::string& path, ResponseFormat format)
{
	sendResponse(request, HTTPResponse::HTTP_NOT_FOUND, formatMessage("notfound"s, path), format);
}


void WebServerDispatcher::sendNotAuthorized(Poco::Net::HTTPServerRequest& request, const std::string& path, ResponseFormat format)
{
	sendResponse(request, HTTPResponse::HTTP_UNAUTHORIZED, formatMessage("notauth"s, path), format);
}


void WebServerDispatcher::sendForbidden(Poco::Net::HTTPServerRequest& request, const std::string& path, ResponseFormat format)
{
	sendResponse(request, HTTPResponse::HTTP_FORBIDDEN, formatMessage("forbidden"s, path), format);
}


void WebServerDispatcher::sendBadRequest(Poco::Net::HTTPServerRequest& request, const std::string& message, ResponseFormat format)
{
	sendResponse(request, HTTPResponse::HTTP_BAD_REQUEST, message, format);
}


void WebServerDispatcher::sendMethodNotAllowed(Poco::Net::HTTPServerRequest& request, const std::string& message, ResponseFormat format)
{
	sendResponse(request, HTTPResponse::HTTP_METHOD_NOT_ALLOWED, message, format);
}


void WebServerDispatcher::sendInternalError(Poco::Net::HTTPServerRequest& request, const std::string& message, ResponseFormat format)
{
	sendResponse(request, HTTPResponse::HTTP_INTERNAL_SERVER_ERROR, message, format);
}


void WebServerDispatcher::sendResponse(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPResponse::HTTPStatus status, const std::string& message, ResponseFormat format)
{
	if (!request.response().sent())
	{
		if (request.get("Accept"s, ""s).find("application/json") != std::string::npos || format == RESPONSE_FORMAT_JSON)
		{
			sendJSONResponse(request, status, message);
		}
		else
		{
			sendHTMLResponse(request, status, message);
		}
	}
	else
	{
		std::string msg("Cannot send response (\"");
		msg += loggedText(message);
		msg += "\") because a response has already been sent for this request.";
		_pContext->logger().warning(msg);
	}
}


void WebServerDispatcher::sendHTMLResponse(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPResponse::HTTPStatus status, const std::string& message)
{
	const std::string& softwareVersion = request.serverParams().getSoftwareVersion();
	request.response().setContentType("text/html"s);
	request.response().setStatusAndReason(status);
	request.response().setChunkedTransferEncoding(false);
	request.response().erase("Content-Encoding"s);

	std::string html("<!DOCTYPE html>\n<html><head><title>");
	html += NumberFormatter::format(static_cast<int>(status));
	html += " - ";
	html += request.response().getReasonForStatus(status);
	html += "</title></head><body><header class=\"osp-web-error\"><h1>"s;
	html += NumberFormatter::format(static_cast<int>(status));
	html += " - ";
	html += request.response().getReasonForStatus(status);
	html += "</h1></header><section class=\"osp-web-error\"><p>"s;
	html += htmlize(message);
	html += "</p></section><hr>"s;
	if (_addSignature)
	{
		html += "<footer class=\"osp-web-error\"><address>"s;
		html += htmlize(softwareVersion);
		html += " at ";
		html += request.serverAddress().toString();
		html += "</address></footer>"s;
	}
	html += "</body></html>"s;
	request.response().sendBuffer(html.data(), html.size());
}


void WebServerDispatcher::sendJSONResponse(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPResponse::HTTPStatus status, const std::string& message)
{
	request.response().setContentType("application/json"s);
	request.response().setStatusAndReason(status);
	request.response().setChunkedTransferEncoding(false);
	request.response().erase("Content-Encoding"s);

	std::string json(
		Poco::format("{ \"error\": \"%s\", \"detail\": \"%s\", \"code\": %d }"s,
			request.response().getReasonForStatus(status),
			Poco::UTF8::escape(message, true),
			static_cast<int>(status)
		)
	);
	request.response().sendBuffer(json.data(), json.size());
}


std::string WebServerDispatcher::htmlize(const std::string& str)
{
	std::string::const_iterator it(str.begin());
	std::string::const_iterator end(str.end());
	std::string html;
	for (; it != end; ++it)
	{
		switch (*it)
		{
		case '<': html += "&lt;"; break;
		case '>': html += "&gt;"; break;
		case '"': html += "&quot;"; break;
		case '&': html += "&amp;"; break;
		default:  html += *it; break;
		}
	}
	return html;
}


Poco::OSP::Auth::AuthService::Ptr WebServerDispatcher::authService() const
{
	Poco::FastMutex::ScopedLock lock(_authServiceMutex);

	return _pAuthService;
}


TokenValidator::Ptr WebServerDispatcher::tokenValidator() const
{
	Poco::FastMutex::ScopedLock lock(_tokenValidatorMutex);

	return _pTokenValidator;
}


void WebServerDispatcher::onAuthServiceRegistered(const ServiceRef::Ptr& pServiceRef)
{
	{
		Poco::FastMutex::ScopedLock lock(_authServiceMutex);

		_pAuthService = pServiceRef->castedInstance<AuthService>();
	}
	_pContext->logger().information("The auth service %s has been registered."s, _authServiceName);
}


void WebServerDispatcher::onAuthServiceUnregistered(const ServiceRef::Ptr&)
{
	{
		Poco::FastMutex::ScopedLock lock(_authServiceMutex);

		_pAuthService = nullptr;
	}
	// Not a warning: the auth bundle may stop before the web bundle when the application shuts down.
	_pContext->logger().notice("The auth service %s has been unregistered. Requests for paths that require a permission are answered with 503 Service Unavailable until it is registered again."s, _authServiceName);
}


void WebServerDispatcher::onTokenValidatorRegistered(const ServiceRef::Ptr& pServiceRef)
{
	Poco::FastMutex::ScopedLock lock(_tokenValidatorMutex);

	_pTokenValidator = pServiceRef->castedInstance<TokenValidator>();
}


void WebServerDispatcher::onTokenValidatorUnregistered(const ServiceRef::Ptr&)
{
	Poco::FastMutex::ScopedLock lock(_tokenValidatorMutex);

	_pTokenValidator = nullptr;
}


void WebServerDispatcher::onSessionManagerRegistered(const ServiceRef::Ptr& pServiceRef)
{
	Poco::FastMutex::ScopedLock lock(_sessionManagerMutex);

	_pSessionManager = pServiceRef->castedInstance<WebSessionManager>();
}


void WebServerDispatcher::onSessionManagerUnregistered(const ServiceRef::Ptr&)
{
	Poco::FastMutex::ScopedLock lock(_sessionManagerMutex);

	_pSessionManager = nullptr;
}


bool WebServerDispatcher::sessionNamesUser(const Poco::Net::HTTPServerRequest& request, const VirtualPath& vPath) const
{
	const int authMethods = vPath.security.authMethods != 0 ? vPath.security.authMethods : _authMethods;
	if (vPath.security.session.empty() || (authMethods & AUTH_SESSION) == 0) return false;
	WebSessionManager::Ptr pSessionManager = sessionManager();
	if (pSessionManager == nullptr) return false;
	WebSession::Ptr pSession = pSessionManager->find(vPath.security.session, request);
	return pSession != nullptr && !pSession->getValue<std::string>("username"s, ""s).empty();
}


std::set<std::string, Poco::CILess> WebServerDispatcher::csrfTokenHeaders() const
{
	FastMutex::ScopedLock lock(_mutex);

	return _csrfTokenHeaders;
}


WebSessionManager::Ptr WebServerDispatcher::sessionManager() const
{
	Poco::FastMutex::ScopedLock lock(_sessionManagerMutex);

	return _pSessionManager;
}


std::string WebServerDispatcher::formatMessage(const std::string& messageId, const std::string& arg1, const std::string& arg2)
{
	std::string message;
	std::string realId("message.");
	realId += messageId;
	std::string rawMessage(_pContext->thisBundle()->properties().getString(realId, ""s));
	if (rawMessage.empty())
	{
		message = "Message not found: ";
		message += realId;
	}
	else
	{
		std::string::const_iterator it(rawMessage.begin());
		std::string::const_iterator end(rawMessage.end());
		while (it != end)
		{
			if (*it == '$')
			{
				++it;
				if (it != end)
				{
					if (*it == '1')
						message += arg1;
					else if (*it == '2')
						message += arg2;
					else
						message += *it;
					++it;
				}
				else message += '$';
			}
			else message += *it++;
		}
	}
	return message;
}


bool WebServerDispatcher::shouldCompressMediaType(const std::string& mediaType) const
{
	std::string::size_type endPos = mediaType.find(';');
	std::string mt(mediaType, 0, endPos);
	if (_compressedMediaTypes.contains(mt))
		return true;

	endPos = mt.find('/');
	if (endPos != std::string::npos)
	{
		mt.resize(endPos);
		mt.append("/*");
		return _compressedMediaTypes.contains(mt);
	}
	else return false;
}


void WebServerDispatcher::addCustomResponseHeaders(Poco::Net::HTTPServerResponse& response)
{
	for (Poco::Net::NameValueCollection::ConstIterator it = _customResponseHeaders.begin(); it != _customResponseHeaders.end(); ++it)
	{
		response.add(it->first, it->second);
	}
}


int WebServerDispatcher::parseAuthMethods(const std::string& methods)
{
	int result = 0;
	Poco::StringTokenizer tok(methods, ",;"s, Poco::StringTokenizer::TOK_TRIM | Poco::StringTokenizer::TOK_IGNORE_EMPTY);
	for (Poco::StringTokenizer::Iterator it = tok.begin(); it != tok.end(); ++it)
	{
		if (*it == "basic")
			result |= AUTH_BASIC;
		else if (*it == "session")
			result |= AUTH_SESSION;
		else if (*it == "bearer")
			result |= AUTH_BEARER;
		else
			throw Poco::InvalidArgumentException("authentication method", *it);
	}
	return result;
}


std::string WebServerDispatcher::canonicalPathWithQuery(const std::string& canonicalPath, const std::string& requestUri)
{
	std::string result(canonicalPath);
	Poco::URI uri(requestUri);
	if (!uri.getRawQuery().empty())
	{
		result += '?';
		result += uri.getRawQuery();
	}
	return result;
}


bool WebServerDispatcher::isSameOrigin(const Poco::Net::HTTPServerRequest& request, const std::string& origin)
{
	try
	{
		const Poco::URI originURI(origin);
		const Poco::URI self((request.secure() ? "https://"s : "http://"s) + request.get("Host"s, ""s));
		return !originURI.getHost().empty()
			&& Poco::icompare(originURI.getScheme(), self.getScheme()) == 0
			&& Poco::icompare(originURI.getHost(), self.getHost()) == 0
			&& originURI.getPort() == self.getPort();
	}
	catch (Poco::SyntaxException&)
	{
		return false;
	}
}


std::string WebServerDispatcher::redirectLocationOn401(const std::string& loginTarget, const std::string& requestUri)
{
	Poco::URI redirectUri(loginTarget);
	const Poco::URI origRequestUri(requestUri);
	const bool alreadyAtLogin = origRequestUri.getPath() == redirectUri.getPath();
	bool alreadyHasRedirect = false;
	for (const auto& [k, v] : origRequestUri.getQueryParameters())
	{
		if (k == "redirect") { alreadyHasRedirect = true; break; }
	}
	// Only the path and query are passed on, so that an absolute request target
	// cannot make the login page redirect to another site.
	if (!requestUri.empty() && !alreadyAtLogin && !alreadyHasRedirect)
	{
		// A browser takes a path starting with "//" or "/\" for a reference to another host.
		std::string path = origRequestUri.getPath();
		const std::string::size_type start = path.find_first_not_of("/\\"s);
		path.replace(0, start == std::string::npos ? path.size() : start, "/"s);
		Poco::URI target;
		target.setPath(path);
		target.setRawQuery(origRequestUri.getRawQuery());
		redirectUri.addQueryParameter("redirect", target.getPathAndQuery());
	}
	return redirectUri.toString();
}


Poco::Net::SocketAddress WebServerDispatcher::realClientAddress(const Poco::Net::HTTPServerRequest& request) const
{
	// The header comes from the client, so only an IP address is taken from it:
	// no host name is resolved, and no other text from it reaches the logs.
	std::string xff = request.get("X-Forwarded-For"s, ""s);
	std::string::size_type pos = xff.find(',');
	if (pos != std::string::npos) xff.resize(pos);
	Poco::trimInPlace(xff);
	Poco::Net::IPAddress address;
	if (!xff.empty() && Poco::Net::IPAddress::tryParse(xff, address))
	{
		return Poco::Net::SocketAddress(address, 0);
	}
	return request.clientAddress();
}


} // namespace Poco::OSP::Web
