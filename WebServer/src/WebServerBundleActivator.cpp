//
// WebServerBundleActivator.cpp
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#include "Poco/OSP/Web/AbstractWebServerBundleActivator.h"
#include "Poco/OSP/BundleActivator.h"
#include "Poco/Net/ServerSocket.h"
#include "Poco/Net/SocketAddress.h"
#include "Poco/ClassLibrary.h"


class WebServerBundleActivator: public Poco::OSP::Web::AbstractWebServerBundleActivator
	/// Runs the HTTP server of the OSP Web Server bundle.
{
public:
	WebServerBundleActivator():
		AbstractWebServerBundleActivator(false)
	{
	}

protected:
	Poco::Net::ServerSocket createServerSocket(const Poco::Net::SocketAddress& address) override
	{
		return Poco::Net::ServerSocket(address);
	}
};


POCO_BEGIN_MANIFEST(Poco::OSP::BundleActivator)
	POCO_EXPORT_CLASS(WebServerBundleActivator)
POCO_END_MANIFEST
