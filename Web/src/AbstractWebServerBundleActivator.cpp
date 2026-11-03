//
// AbstractWebServerBundleActivator.cpp
//
// Library: OSP/Web
// Package: Web
// Module:  AbstractWebServerBundleActivator
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "Poco/OSP/Web/AbstractWebServerBundleActivator.h"
#include "Poco/OSP/Web/WebServerDispatcher.h"
#include "Poco/OSP/Web/WebServerRequestHandlerFactory.h"
#include "Poco/OSP/Web/WebServerService.h"
#include "Poco/OSP/Configuration.h"
#include "Poco/OSP/PreferencesService.h"
#include "Poco/OSP/Properties.h"
#include "Poco/OSP/ServiceRegistry.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/ServiceRef.h"
#include "Poco/Net/HTTPServer.h"
#include "Poco/Net/HTTPServerParams.h"
#include "Poco/Net/ServerSocket.h"
#include "Poco/Util/AbstractConfiguration.h"
#include "Poco/Format.h"
#include "Poco/NumberFormatter.h"
#include "Poco/Timespan.h"
#include "Poco/AutoPtr.h"
#include "Poco/Types.h"
#include <memory>
#include <string>
#include <utility>


using Poco::Net::HTTPServer;
using Poco::Net::HTTPServerParams;
using namespace std::string_literals;


namespace Poco::OSP::Web {


namespace
{
	constexpr int DEFAULT_HTTP_PORT = 22080;
	constexpr int DEFAULT_HTTPS_PORT = 22443;
	constexpr int DEFAULT_MAX_QUEUED = 100;
	constexpr int DEFAULT_MAX_THREADS = 8;
	constexpr int DEFAULT_KEEP_ALIVE_TIME = 10;
	constexpr int DEFAULT_MAX_KEEP_ALIVE = 10;

	struct ServerSettings
	{
		std::string serverName;
		std::string host;
		int port = 0;
		int maxQueued = 0;
		int maxThreads = 0;
		bool keepAlive = false;
		int keepAliveTime = 0;
		int maxKeepAlive = 0;
	};

	[[nodiscard]] ServerSettings readSettings(BundleContext::Ptr pContext, bool secure)
		/// Reads the server settings from the global application configuration,
		/// with the bundle properties as defaults.
	{
		const std::string portKey(secure ? "securePort"s : "port"s);
		const Poco::Util::AbstractConfiguration& props = pContext->thisBundle()->properties();
		ServerSettings settings;
		settings.serverName    = props.getString("serverName"s, pContext->thisBundle()->name());
		settings.host          = props.getString("host"s, "0.0.0.0"s);
		settings.port          = props.getInt(portKey, secure ? DEFAULT_HTTPS_PORT : DEFAULT_HTTP_PORT);
		settings.maxQueued     = props.getInt("maxQueued"s, DEFAULT_MAX_QUEUED);
		settings.maxThreads    = props.getInt("maxThreads"s, DEFAULT_MAX_THREADS);
		settings.keepAlive     = props.getBool("keepAlive"s, true);
		settings.keepAliveTime = props.getInt("keepAliveTime"s, DEFAULT_KEEP_ALIVE_TIME);
		settings.maxKeepAlive  = props.getInt("maxKeepAlive"s, DEFAULT_MAX_KEEP_ALIVE);

		ServiceRef::Ptr pPrefsRef = pContext->registry().findByName(PreferencesService::SERVICE_NAME);
		if (pPrefsRef != nullptr)
		{
			Configuration::Ptr pConfig = pPrefsRef->castedInstance<PreferencesService>()->configuration();
			settings.host          = pConfig->getString(secure ? "osp.web.server.secureHost"s : "osp.web.server.host"s, settings.host);
			settings.port          = pConfig->getInt("osp.web.server."s + portKey, settings.port);
			settings.maxQueued     = pConfig->getInt("osp.web.server.maxQueued"s, settings.maxQueued);
			settings.maxThreads    = pConfig->getInt("osp.web.server.maxThreads"s, settings.maxThreads);
			settings.keepAlive     = pConfig->getBool("osp.web.server.keepAlive"s, settings.keepAlive);
			settings.keepAliveTime = pConfig->getInt("osp.web.server.keepAliveTime"s, settings.keepAliveTime);
			settings.maxKeepAlive  = pConfig->getInt("osp.web.server.maxKeepAlive"s, settings.maxKeepAlive);
		}
		else
		{
			pContext->logger().warning("The preferences service is not available. The %s server uses the settings from the bundle properties of %s."s,
				secure ? "HTTPS"s : "HTTP"s, pContext->thisBundle()->symbolicName());
		}
		return settings;
	}

	class ServerService: public WebServerService
		/// The WebServerService for a running HTTPServer.
	{
	public:
		ServerService() = delete;

		ServerService(HTTPServerParams::Ptr pParams, std::shared_ptr<const HTTPServer> pServer, const std::string& host, bool secure):
			_pParams(std::move(pParams)),
			_pServer(std::move(pServer)),
			_host(host),
			_secure(secure)
		{
		}

		ServerService(const ServerService&) = delete;
		ServerService(ServerService&&) = delete;
		ServerService& operator = (const ServerService&) = delete;
		ServerService& operator = (ServerService&&) = delete;

		[[nodiscard]] int currentThreads() const override
		{
			return _pServer->currentThreads();
		}

		[[nodiscard]] int totalConnections() const override
		{
			return static_cast<int>(_pServer->totalConnections());
		}

		[[nodiscard]] int currentConnections() const override
		{
			return _pServer->currentConnections();
		}

		[[nodiscard]] int maxConcurrentConnections() const override
		{
			return _pServer->maxConcurrentConnections();
		}

		[[nodiscard]] int queuedConnections() const override
		{
			return _pServer->queuedConnections();
		}

		[[nodiscard]] int refusedConnections() const override
		{
			return _pServer->refusedConnections();
		}

		[[nodiscard]] std::string host() const override
		{
			return _host;
		}

		[[nodiscard]] Poco::UInt16 port() const override
		{
			return _pServer->port();
		}

		[[nodiscard]] bool secure() const override
		{
			return _secure;
		}

		[[nodiscard]] HTTPServerParams::Ptr params() const override
		{
			return _pParams;
		}

	protected:
		~ServerService() override = default;
			/// Reference counted: destroyed through release() only.

	private:
		HTTPServerParams::Ptr _pParams;
		std::shared_ptr<const HTTPServer> _pServer;
		std::string _host;
		bool _secure;
	};
}


AbstractWebServerBundleActivator::AbstractWebServerBundleActivator(bool secure):
	_secure(secure)
{
}


AbstractWebServerBundleActivator::~AbstractWebServerBundleActivator() = default;


void AbstractWebServerBundleActivator::start(BundleContext::Ptr pContext)
{
	const std::string serverKind(_secure ? "HTTPS"s : "HTTP"s);
	ServiceRef::Ptr pDispatcherRef = pContext->registry().findByName(WebServerDispatcher::SERVICE_NAME);
	if (pDispatcherRef == nullptr)
	{
		pContext->logger().error("The WebServerDispatcher service could not be found. The %s server is not started."s, serverKind);
		return;
	}
	Poco::AutoPtr<WebServerDispatcher> pDispatcher = pDispatcherRef->castedInstance<WebServerDispatcher>();

	ServerSettings settings = readSettings(pContext, _secure);
	if (settings.port == 0) return;

	HTTPServerParams::Ptr pParams = new HTTPServerParams;
	pParams->setServerName(Poco::format("%s:%d"s, settings.host, settings.port));
	pParams->setSoftwareVersion(settings.serverName + "/" + pContext->thisBundle()->version().toString());
	pParams->setKeepAlive(settings.keepAlive);
	pParams->setKeepAliveTimeout(Poco::Timespan(settings.keepAliveTime, 0));
	pParams->setMaxKeepAliveRequests(settings.maxKeepAlive);
	pParams->setMaxQueued(settings.maxQueued);
	pParams->setMaxThreads(settings.maxThreads);

	pContext->logger().information("Starting %s server on port %d."s, serverKind, settings.port);

	Poco::Net::ServerSocket socket = createServerSocket(Poco::Net::SocketAddress(settings.host, settings.port));
	auto pServer = std::make_shared<HTTPServer>(new WebServerRequestHandlerFactory(*pDispatcher, _secure), pDispatcher->threadPool(), socket, pParams);
	pDispatcher->threadPool().addCapacity(settings.maxThreads);
	try
	{
		pServer->start();
	}
	catch (...)
	{
		pDispatcher->threadPool().addCapacity(-settings.maxThreads);
		throw;
	}
	_pServer = pServer;
	_pDispatcher = pDispatcher;
	_addedThreads = settings.maxThreads;

	Properties props;
	props.set("protocol"s, _secure ? "https"s : "http"s);
	props.set("host"s, settings.host);
	props.set("port"s, Poco::NumberFormatter::format(settings.port));
	_pService = pContext->registry().registerService(_secure ? "osp.web.server.secure"s : "osp.web.server"s, new ServerService(pParams, pServer, settings.host, _secure), props);
}


void AbstractWebServerBundleActivator::stop(BundleContext::Ptr pContext)
{
	if (_pServer != nullptr)
	{
		pContext->logger().information("Stopping %s server."s, _secure ? "HTTPS"s : "HTTP"s);
		pContext->registry().unregisterService(_pService);
		_pService.reset();
		_pServer->stopAll(true);
		_pServer.reset();
		_pDispatcher->threadPool().addCapacity(-_addedThreads);
		_pDispatcher.reset();
		_addedThreads = 0;
	}
}


} // namespace Poco::OSP::Web
