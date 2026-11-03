//
// ServiceFactory.cpp
//
// Library: OSP
// Package: Service
// Module:  ServiceFactory
//
// Copyright (c) 2007-2014, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#include "Poco/OSP/ServiceFactory.h"


namespace Poco::OSP {


ServiceFactory::ServiceFactory() = default;


ServiceFactory::~ServiceFactory() = default;


const std::type_info& ServiceFactory::type() const
{
	return typeid(ServiceFactory);
}


bool ServiceFactory::isA(const std::type_info& otherType) const
{
	std::string name(typeid(ServiceFactory).name());
	return name == otherType.name() || Service::isA(otherType);
}


} // namespace Poco::OSP
