//
// WebServerService.h
//
// Library: OSP/Web
// Package: Web
// Module:  WebServerService
//
// Definition of the WebServerService interface.
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef OSP_Web_WebServerService_INCLUDED
#define OSP_Web_WebServerService_INCLUDED


#include "Poco/OSP/Web/Web.h"
#include "Poco/OSP/Service.h"
#include "Poco/OSP/BundleContext.h"
#include "Poco/Net/HTTPServerParams.h"
#include "Poco/Types.h"


namespace Poco::OSP::Web {


class OSPWeb_API WebServerService: public Service
	/// The WebServerService is a service that is used
	/// by HTTP server implementations to register
	/// the HTTP server instance in the service registry.
	///
	/// By looking up instances of this service, bundles
	/// can find out which web servers are available,
	/// and what port numbers they are running on.
	///
	/// Also, some statistics about the server may be obtained.
{
public:
	[[nodiscard]] virtual int currentThreads() const = 0;
		/// Returns the number of currently used connection threads.

	[[nodiscard]] virtual Poco::Int64 totalConnections() const = 0;
		/// Returns the total number of handled connections.
		
	[[nodiscard]] virtual int currentConnections() const = 0;
		/// Returns the number of currently handled connections.

	[[nodiscard]] virtual int maxConcurrentConnections() const = 0;
		/// Returns the maximum number of concurrently handled connections.	
		
	[[nodiscard]] virtual int queuedConnections() const = 0;
		/// Returns the number of queued connections.

	[[nodiscard]] virtual int refusedConnections() const = 0;
		/// Returns the number of refused connections.

	[[nodiscard]] virtual std::string host() const = 0;
		/// Returns the host name (interface) the server listens on.

	[[nodiscard]] virtual Poco::UInt16 port() const = 0;
		/// Returns the port the server socket listens on.
		
	[[nodiscard]] virtual bool secure() const = 0;
		/// Returns true iff the web server supports HTTPS.
		
	[[nodiscard]] virtual Poco::Net::HTTPServerParams::Ptr params() const = 0;
		/// Returns the Poco::Net::HTTPServerParams object for the server instance.

	// Service
	[[nodiscard]] const std::type_info& type() const override;
	[[nodiscard]] bool isA(const std::type_info& otherType) const override;
	
protected:
	WebServerService();	
		/// Creates a WebServerService.
		
	~WebServerService() override;
		/// Destroys the WebServerService.
};


} // namespace Poco::OSP::Web


#endif // OSP_Web_WebServerService_INCLUDED
