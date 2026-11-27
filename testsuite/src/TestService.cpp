//
// TestService.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "TestService.h"
#include "Poco/OSP/Service.h"
#include <string>
#include <typeinfo>


const std::type_info& TestService::type() const
{
	return typeid(TestService);
}


bool TestService::isA(const std::type_info& otherType) const
{
	std::string name(typeid(TestService).name());
	return name == otherType.name() || Service::isA(otherType);
}
