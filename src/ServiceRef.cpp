//
// ServiceRef.cpp
//
// Library: OSP
// Package: Service
// Module:  ServiceRef
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "Poco/OSP/ServiceRef.h"
#include "Poco/OSP/ServiceFactory.h"
#include "Poco/Types.h"


using namespace std::string_literals;


namespace Poco::OSP {


ServiceRef::ServiceRef(const std::string& name, const Properties& props, Service::Ptr pService):
	_name(name),
	_props(props),
	_pService(pService)
{
}


ServiceRef::~ServiceRef() = default;


Service::Ptr ServiceRef::instance() const
{
	Poco::AutoPtr<ServiceFactory> pFactory = _pService.cast<ServiceFactory>();
	if (pFactory != nullptr)
	{	
		return pFactory->createService();
	}
	else
	{
		return _pService;
	}
}


std::string ServiceRef::demangle(const char* typeName)
{
	return Poco::demangle(typeName);
}


} // namespace Poco::OSP
