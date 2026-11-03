//
// WebServerRequestHandlerFactory.cpp
//
// Library: OSP/Web
// Package: Web
// Module:  WebServerRequestHandlerFactory
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#include "Poco/OSP/Web/WebServerRequestHandlerFactory.h"
#include "Poco/OSP/Web/WebServerRequestHandler.h"


namespace Poco::OSP::Web {


WebServerRequestHandlerFactory::WebServerRequestHandlerFactory(WebServerDispatcher& dispatcher, bool secure):
	_dispatcher(dispatcher),
	_secure(secure)
{
}

	
WebServerRequestHandlerFactory::~WebServerRequestHandlerFactory() = default;


Poco::Net::HTTPRequestHandler* WebServerRequestHandlerFactory::createRequestHandler(
	const Poco::Net::HTTPServerRequest& /*request*/)
{
	return new WebServerRequestHandler(_dispatcher, _secure);
}


} // namespace Poco::OSP::Web
