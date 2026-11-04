//
// AbstractWebServerBundleActivator.h
//
// Library: OSP/Web
// Package: Web
// Module:  AbstractWebServerBundleActivator
//
// Definition of the AbstractWebServerBundleActivator class.
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef OSP_Web_AbstractWebServerBundleActivator_INCLUDED
#define OSP_Web_AbstractWebServerBundleActivator_INCLUDED


#include "Poco/OSP/Web/Web.h"
#include "Poco/OSP/BundleActivator.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/OSP/ServiceRef.h"
#include "Poco/Net/ServerSocket.h"
#include "Poco/Net/SocketAddress.h"
#include "Poco/AutoPtr.h"
#include <memory>


namespace Poco::Net {


class HTTPServer;


} // namespace Poco::Net


namespace Poco::OSP::Web {


class WebServerDispatcher;


class OSPWeb_API AbstractWebServerBundleActivator: public BundleActivator
	/// The base class of the bundle activators of the OSP Web Server
	/// (HTTP) and the OSP Secure Web Server (HTTPS) bundles.
	///
	/// start() creates a Poco::Net::HTTPServer that passes requests to the
	/// WebServerDispatcher service, and registers a WebServerService for it,
	/// named "osp.web.server" (HTTP) or "osp.web.server.secure" (HTTPS),
	/// with the service properties "protocol", "host" and "port".
	/// stop() stops the server, unregisters the service and returns the
	/// threads that start() added to the dispatcher's thread pool.
	///
	/// The server settings are read from the global application configuration:
	/// osp.web.server.host and osp.web.server.port (HTTP), or
	/// osp.web.server.secureHost and osp.web.server.securePort (HTTPS), and
	/// osp.web.server.maxQueued, osp.web.server.maxThreads, osp.web.server.keepAlive,
	/// osp.web.server.keepAliveTime and osp.web.server.maxKeepAlive.
	/// The bundle properties host, port (HTTP) or securePort (HTTPS), maxQueued,
	/// maxThreads, keepAlive, keepAliveTime and maxKeepAlive are the defaults;
	/// without a preferences service, they are used alone.
	/// A port of 0 disables the server; any other port outside 1 to 65535 is
	/// logged as an error and the server is not started.
{
public:
	AbstractWebServerBundleActivator() = delete;

	~AbstractWebServerBundleActivator() override;
		/// Destroys the AbstractWebServerBundleActivator.

	void start(BundleContext::Ptr pContext) override;
		/// Starts the server and registers the WebServerService.

	void stop(BundleContext::Ptr pContext) override;
		/// Stops the server and unregisters the WebServerService.

protected:
	explicit AbstractWebServerBundleActivator(bool secure);
		/// Creates the activator for a HTTPS server if secure is true,
		/// otherwise for a HTTP server.

	[[nodiscard]] virtual Poco::Net::ServerSocket createServerSocket(const Poco::Net::SocketAddress& address) = 0;
		/// Returns a server socket that listens on the given address:
		/// a Poco::Net::ServerSocket for HTTP, a Poco::Net::SecureServerSocket
		/// for HTTPS.

private:
	bool _secure;
	std::shared_ptr<Poco::Net::HTTPServer> _pServer;
	ServiceRef::Ptr _pService;
	Poco::AutoPtr<WebServerDispatcher> _pDispatcher;
	int _addedThreads = 0;
};


} // namespace Poco::OSP::Web


#endif // OSP_Web_AbstractWebServerBundleActivator_INCLUDED
