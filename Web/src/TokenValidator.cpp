//
// TokenValidator.cpp
//
// Library: OSP/Web
// Package: Web
// Module:  TokenValidator
//
// Copyright (c) 2019, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#include "Poco/OSP/Web/TokenValidator.h"


namespace Poco::OSP::Web {


const std::type_info& TokenValidator::type() const
{
	return typeid(TokenValidator);
}


bool TokenValidator::isA(const std::type_info& otherType) const
{
	std::string name(typeid(TokenValidator).name());
	return name == otherType.name() || Service::isA(otherType);
}


} // namespace Poco::OSP::Web
