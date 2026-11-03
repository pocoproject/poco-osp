//
// WebSessionStore.cpp
//
// Library: OSP/Web
// Package: Web
// Module:  WebSession
//
// Copyright (c) 2007-2019, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// License: Applied Informatics Software and Source Code License Agreement
//


#include "Poco/OSP/Web/WebSessionStore.h"


namespace Poco::OSP::Web {


const std::type_info& WebSessionStore::type() const
{
	return typeid(WebSessionStore);
}


bool WebSessionStore::isA(const std::type_info& otherType) const
{
	std::string name(typeid(WebSessionStore).name());
	return name == otherType.name() || Service::isA(otherType);
}


} // namespace Poco::OSP::Web
