//
// WebRequestHandlerFactory.cpp
//
// Library: OSP/Web
// Package: Web
// Module:  WebRequestHandlerFactory
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#include "Poco/OSP/Web/WebRequestHandlerFactory.h"


namespace Poco::OSP::Web {


WebRequestHandlerFactory::WebRequestHandlerFactory() = default;


WebRequestHandlerFactory::~WebRequestHandlerFactory() = default;


void WebRequestHandlerFactory::init(BundleContext::Ptr pContext)
{
	_pContext = pContext;
	initImpl();
}


void WebRequestHandlerFactory::initImpl()
{
}


} // namespace Poco::OSP::Web
