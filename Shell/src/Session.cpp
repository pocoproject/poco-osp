//
// Session.cpp
//
// Library: OSP/Shell
// Package: OSPShell
// Module:  Session
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties, 
// copied or duplicated in any form, in whole or in part.
//


#include "Poco/OSP/Shell/Session.h"
#include "Poco/OSP/ServiceRegistry.h"
#include "Poco/OSP/ServiceRef.h"


namespace Poco {
namespace OSP {
namespace Shell {


Session::Session(Poco::OSP::BundleContext::Ptr pContext, const std::string& authServiceName):
	_pContext(pContext),
	_authServiceName(authServiceName),
	_authenticated(false)
{
}


Session::~Session()
{
}


bool Session::login(const std::string& userName, const std::string& credentials)
{
	_userName = userName;
	Poco::OSP::Auth::AuthService::Ptr pAuthService = authService();
	if (pAuthService)
	{
		_authenticated = pAuthService->authenticate(_userName, credentials);
	}
	else
	{
		_authenticated = true;
	}
	return _authenticated;
}


void Session::loginWithKey(const std::string& userName)
{
	_userName = userName;
	_authenticated = true;
}


bool Session::authorize(const std::string& permission) const
{
	Poco::OSP::Auth::AuthService::Ptr pAuthService = authService();
	if (authenticated() && pAuthService && !permission.empty())
		return pAuthService->authorize(_userName, permission);
	else
		return authenticated();
}


Poco::OSP::Auth::AuthService::Ptr Session::authService() const
{	
	if (!_pAuthService && !_authServiceName.empty())
	{
		if (!_authServiceName.empty())
		{
			Poco::OSP::ServiceRef::Ptr pAuthRef = _pContext->registry().findByName(_authServiceName);
			if (pAuthRef)
			{
				_pAuthService = pAuthRef->castedInstance<Poco::OSP::Auth::AuthService>();
			}
			else
			{
				std::string msg("No auth service (");
				msg += _authServiceName;
				msg += ") is available.";
				_pContext->logger().warning(msg);
			}
		}

	}
	return _pAuthService;
}


} } } // namespace Poco::OSP::Shell
