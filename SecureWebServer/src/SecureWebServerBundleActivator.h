//
// SecureWebServerBundleActivator.h
//
// Library: OSP/SecureWebServer
// Package: SecureWebServer
// Module:  SecureWebServerBundleActivator
//
// Definition of the SecureWebServerBundleActivator class.
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef OSP_SecureWebServer_SecureWebServerBundleActivator_INCLUDED
#define OSP_SecureWebServer_SecureWebServerBundleActivator_INCLUDED


#include "Poco/OSP/Web/AbstractWebServerBundleActivator.h"
#include "Poco/Net/ServerSocket.h"
#include "Poco/Net/SocketAddress.h"


class SecureWebServerBundleActivator: public Poco::OSP::Web::AbstractWebServerBundleActivator
	/// Runs the HTTPS server of the OSP Secure Web Server bundle.
	///
	/// The server socket uses the default server context of
	/// Poco::Net::SSLManager, which is configured with the
	/// openSSL.server.* properties of the application configuration.
{
public:
	SecureWebServerBundleActivator();
		/// Creates the SecureWebServerBundleActivator.

	~SecureWebServerBundleActivator() override;
		/// Destroys the SecureWebServerBundleActivator.

	SecureWebServerBundleActivator(const SecureWebServerBundleActivator&) = delete;
	SecureWebServerBundleActivator(SecureWebServerBundleActivator&&) = delete;
	SecureWebServerBundleActivator& operator = (const SecureWebServerBundleActivator&) = delete;
	SecureWebServerBundleActivator& operator = (SecureWebServerBundleActivator&&) = delete;

protected:
	[[nodiscard]] Poco::Net::ServerSocket createServerSocket(const Poco::Net::SocketAddress& address) override;
		/// Returns a Poco::Net::SecureServerSocket that listens on the given address.
};


#endif // OSP_SecureWebServer_SecureWebServerBundleActivator_INCLUDED
