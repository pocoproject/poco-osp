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
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties,
// copied or duplicated in any form, in whole or in part.
//


#include "Poco/OSP/Auth/Remoting/Authorizer.h"
#include "Poco/OSP/Auth/AuthService.h"
#include "Poco/OSP/ServiceFinder.h"
#include "Poco/RemotingNG/Context.h"
#include "Poco/Net/HTTPServerRequest.h"


namespace Poco {
namespace OSP {
namespace Auth {
namespace Remoting {


using namespace std::string_literals;


Authorizer::Authorizer(Poco::OSP::BundleContext::Ptr pContext):
	_pContext(pContext)
{
}


Authorizer::~Authorizer() = default;


bool Authorizer::authorize(const std::string& /*method*/, const std::string& permission)
{
	// The authenticated username and scope (if any) are read from HTTP headers
	// set by WebServerDispatcher. For any endpoint with a non-empty URL-level
	// permission, the dispatcher guarantees:
	//   - X-OSP-Authorized-User is set to the authenticated username
	//   - X-OSP-Authorized-Scope is set to the JWT audience for Bearer auth (empty otherwise)
	//   - Both headers are erased if the user is not authorized
	//
	// We do NOT fall back to Basic auth credentials from Context: for endpoints
	// with URL-level permissions, the dispatcher has already validated Basic
	// credentials and set the header. For endpoints without URL-level permissions,
	// trusting the Basic username would bypass password verification.
	auto pContext = Poco::RemotingNG::Context::get();
	if (!pContext || !pContext->has("httpRequest"s)) return false;

	auto* pRequest = pContext->getValue<Poco::Net::HTTPServerRequest*>("httpRequest"s);
	if (!pRequest) return false;

	const std::string username = pRequest->get("X-OSP-Authorized-User"s, ""s);
	if (username.empty()) return false;

	auto pAuth = Poco::OSP::ServiceFinder::findByName<Poco::OSP::Auth::AuthService>(
		_pContext, "osp.auth"s);
	if (!pAuth) return false;

	// Scope is set by WebServerDispatcher for Bearer JWT auth. When present, the
	// permission must be granted by both the user AND the scope (the JWT audience
	// role). This prevents a scoped token from accessing resources outside its scope.
	const std::string scope = pRequest->get("X-OSP-Authorized-Scope"s, ""s);
	if (!scope.empty())
	{
		return pAuth->authorize(username, scope, permission);
	}
	return pAuth->authorize(username, permission);
}


} } } } // namespace Poco::OSP::Auth::Remoting
