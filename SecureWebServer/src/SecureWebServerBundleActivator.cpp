//
// SecureWebServerBundleActivator.cpp
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "SecureWebServerBundleActivator.h"
#include "Poco/OSP/Web/AbstractWebServerBundleActivator.h"
#include "Poco/OSP/BundleActivator.h"
#include "Poco/Net/SecureServerSocket.h"
#include "Poco/Net/ServerSocket.h"
#include "Poco/Net/SocketAddress.h"
#include "Poco/ClassLibrary.h"


SecureWebServerBundleActivator::SecureWebServerBundleActivator():
	AbstractWebServerBundleActivator(true)
{
}


SecureWebServerBundleActivator::~SecureWebServerBundleActivator() = default;


Poco::Net::ServerSocket SecureWebServerBundleActivator::createServerSocket(const Poco::Net::SocketAddress& address)
{
	// The copy shares the socket implementation, which performs the TLS handshake.
	// NOLINTNEXTLINE(cppcoreguidelines-slicing)
	return Poco::Net::SecureServerSocket(address);
}


POCO_BEGIN_MANIFEST(Poco::OSP::BundleActivator)
	POCO_EXPORT_CLASS(SecureWebServerBundleActivator)
POCO_END_MANIFEST
