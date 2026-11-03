//
// ServiceEvent.cpp
//
// Library: OSP
// Package: Service
// Module:  ServiceEvent
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#include "Poco/OSP/ServiceEvent.h"


namespace Poco::OSP {


ServiceEvent::ServiceEvent(ServiceRef::Ptr pService, EventKind what):
	_pService(pService),
	_what(what)
{
}


ServiceEvent::ServiceEvent(const ServiceEvent& event) = default;


ServiceEvent::~ServiceEvent() = default;

	
ServiceEvent& ServiceEvent::operator = (const ServiceEvent& event)
{
	if (this != &event)
	{
		_pService = event._pService;
		_what     = event._what;
	}
	return *this;
}


} // namespace Poco::OSP
