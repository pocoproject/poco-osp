//
// WebFilterFactory.cpp
//
// Library: OSP/Web
// Package: Web
// Module:  WebFilterFactory
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "Poco/OSP/Web/WebFilterFactory.h"


namespace Poco::OSP::Web {


WebFilterFactory::WebFilterFactory() = default;


WebFilterFactory::~WebFilterFactory() = default;


void WebFilterFactory::init(BundleContext::Ptr pContext)
{
	_pContext = pContext;
	initImpl();
}


void WebFilterFactory::initImpl()
{
}


} // namespace Poco::OSP::Web
