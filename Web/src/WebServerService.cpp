//
// WebServerService.cpp
//
// Library: OSP/Web
// Package: Web
// Module:  WebServerService
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#include "Poco/OSP/Web/WebServerService.h"


namespace Poco::OSP::Web {


WebServerService::WebServerService() = default;


WebServerService::~WebServerService() = default;


const std::type_info& WebServerService::type() const
{
	return typeid(WebServerService);
}


bool WebServerService::isA(const std::type_info& otherType) const
{
	std::string name(typeid(WebServerService).name());
	return name == otherType.name() || Service::isA(otherType);
}


} // namespace Poco::OSP::Web
