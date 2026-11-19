//
// RequestIdentity.cpp
//
// Library: OSP
// Package: Auth
// Module:  RequestIdentity
//
// Copyright (c) 2026, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "Poco/OSP/Auth/RequestIdentity.h"
#include <string>


namespace Poco::OSP::Auth {


namespace
{
	[[nodiscard]] const RequestIdentity*& currentIdentity()
		// A thread_local static data member cannot be exported from a DLL.
	{
		thread_local const RequestIdentity* pIdentity = nullptr;
		return pIdentity;
	}
}


RequestIdentity::RequestIdentity(const std::string& user, const std::string& scope):
	_user(user),
	_scope(scope),
	_pPrevious(currentIdentity())
{
	currentIdentity() = this;
}


RequestIdentity::~RequestIdentity()
{
	currentIdentity() = _pPrevious;
}


const RequestIdentity* RequestIdentity::current()
{
	return currentIdentity();
}


} // namespace Poco::OSP::Auth
