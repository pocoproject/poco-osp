//
// Authorizer.cpp
//
// Library: OSP/Auth/Remoting
// Package: Auth
// Module:  Authorizer
//
// Copyright (c) 2025, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "Poco/OSP/Auth/Remoting/Authorizer.h"
#include "Poco/OSP/Auth/AuthService.h"
#include "Poco/OSP/Auth/RequestIdentity.h"
#include "Poco/OSP/ServiceRegistry.h"
#include "Poco/OSP/ServiceRef.h"
#include <string>


namespace Poco::OSP::Auth::Remoting {


using namespace std::string_literals;


Authorizer::Authorizer(Poco::OSP::BundleContext::Ptr pContext):
	_pContext(pContext)
{
}


Authorizer::~Authorizer() = default;


bool Authorizer::authorize(const std::string& /*method*/, const std::string& permission)
{
	// No fallback to the Basic credentials of the Context: without a URL-level permission,
	// the dispatcher has not verified the password.
	const RequestIdentity* pIdentity = RequestIdentity::current();
	if (pIdentity == nullptr) return false;

	ServiceRef::Ptr pAuthRef = _pContext->registry().findByName("osp.auth"s);
	if (pAuthRef == nullptr) return false;
	auto pAuth = pAuthRef->castedInstance<AuthService>();

	// A scoped token gets only what both the user and the scope are granted.
	if (!pIdentity->scope().empty())
	{
		return pAuth->authorize(pIdentity->user(), pIdentity->scope(), permission);
	}
	return pAuth->authorize(pIdentity->user(), permission);
}


} // namespace Poco::OSP::Auth::Remoting
