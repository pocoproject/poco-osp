//
// GlobalSimpleAuthService.cpp
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "GlobalSimpleAuthService.h"
#include <typeinfo>


class SimpleAuthService
{
};


const std::type_info& globalSimpleAuthServiceType()
{
	return typeid(SimpleAuthService);
}
