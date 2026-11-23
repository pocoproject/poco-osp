//
// WebEventService.cpp
//
// Library: OSP/WebEvent
// Package: WebEvent
// Module:  WebEventService
//
// Copyright (c) 2013, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "Poco/OSP/WebEvent/WebEventService.h"


namespace Poco::OSP::WebEvent {


const std::string WebEventService::SERVICE_NAME("com.appinf.osp.webevent");


WebEventService::WebEventService() = default;


WebEventService::~WebEventService() = default;


void WebEventService::notifyEach(const std::string& subjectName, const std::string& data)
{
	notify(subjectName, data);
}


const std::type_info& WebEventService::type() const
{
	return typeid(WebEventService);
}


bool WebEventService::isA(const std::type_info& otherType) const
{
	std::string name(typeid(WebEventService).name());
	return name == otherType.name() || Poco::OSP::Service::isA(otherType);
}


} // namespace Poco::OSP::WebEvent
