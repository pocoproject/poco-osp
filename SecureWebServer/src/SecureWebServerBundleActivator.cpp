//
// SecureWebServerBundleActivator.cpp
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#include "Poco/OSP/Web/AbstractWebServerBundleActivator.h"
#include "Poco/OSP/BundleActivator.h"
#include "Poco/Net/SecureServerSocket.h"
#include "Poco/Net/SocketAddress.h"
#include "Poco/Net/ServerSocket.h"
#include "Poco/ClassLibrary.h"


class SecureWebServerBundleActivator: public Poco::OSP::Web::AbstractWebServerBundleActivator
	/// Runs the HTTPS server of the OSP Secure Web Server bundle.
	///
	/// The server socket uses the default server context of
	/// Poco::Net::SSLManager, which is configured with the
	/// openSSL.server.* properties of the application configuration.
{
public:
	SecureWebServerBundleActivator():
		AbstractWebServerBundleActivator(true)
	{
	}

protected:
	[[nodiscard]] Poco::Net::ServerSocket createServerSocket(const Poco::Net::SocketAddress& address) override
	{
		// The copy shares the socket implementation, which performs the TLS handshake.
		return Poco::Net::SecureServerSocket(address);
	}
};


POCO_BEGIN_MANIFEST(Poco::OSP::BundleActivator)
	POCO_EXPORT_CLASS(SecureWebServerBundleActivator)
POCO_END_MANIFEST
