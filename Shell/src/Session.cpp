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
#include "Poco/OSP/Auth/AuthService.h"
#include "Poco/Exception.h"
#include "Poco/Logger.h"
#include <exception>
#include <string>
#include <utility>


namespace Poco::OSP::Shell {


namespace
{
	void logLoginNotChecked(Poco::Logger& logger, const std::string& authServiceName, const std::string& reason)
	{
		std::string msg("The authentication service ");
		msg += authServiceName;
		msg += " could not check a shell login, so the login is rejected: ";
		msg += reason;
		logger.error(msg);
	}

	void logPermissionNotChecked(Poco::Logger& logger, const std::string& authServiceName,
		const std::string& permission, const std::string& userName, const std::string& reason)
	{
		std::string msg("The authentication service ");
		msg += authServiceName;
		msg += " could not check the permission ";
		msg += permission;
		msg += " of the shell user ";
		msg += userName;
		msg += ", so the permission is denied: ";
		msg += reason;
		logger.warning(msg);
	}
}


Session::Session(Poco::OSP::BundleContext::Ptr pContext, const std::string& authServiceName):
	_pContext(pContext),
	_authServiceName(authServiceName)
{
}


Session::~Session() = default;


bool Session::login(const std::string& userName, const std::string& credentials)
{
	std::string accountName(userName);
	if (_authServiceName.empty())
	{
		_authenticated = true;
	}
	else
	{
		_authenticated = false;
		try
		{
			Poco::OSP::Auth::AuthService::Ptr pAuthService = authService();
			_authenticated = pAuthService != nullptr
				&& pAuthService->authenticateAccount(userName, credentials, accountName);
		}
		catch (Poco::Exception& exc)
		{
			logLoginNotChecked(_pContext->logger(), _authServiceName, exc.displayText());
		}
		catch (std::exception& exc)
		{
			logLoginNotChecked(_pContext->logger(), _authServiceName, exc.what());
		}
	}
	if (_authenticated)
	{
		_userName = std::move(accountName);
	}
	else
	{
		_userName.clear();
		++_failedLogins;
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
	if (!authenticated()) return false;
	if (_authServiceName.empty() || permission.empty()) return true;

	try
	{
		Poco::OSP::Auth::AuthService::Ptr pAuthService = authService();
		return pAuthService != nullptr && pAuthService->authorize(_userName, permission);
	}
	catch (Poco::Exception& exc)
	{
		logPermissionNotChecked(_pContext->logger(), _authServiceName, permission, _userName, exc.displayText());
	}
	catch (std::exception& exc)
	{
		logPermissionNotChecked(_pContext->logger(), _authServiceName, permission, _userName, exc.what());
	}
	return false;
}


Poco::OSP::Auth::AuthService::Ptr Session::authService() const
{
	if (_authServiceName.empty()) return nullptr;

	Poco::OSP::ServiceRef::Ptr pAuthRef = _pContext->registry().findByName(_authServiceName);
	if (pAuthRef != nullptr) return pAuthRef->castedInstance<Poco::OSP::Auth::AuthService>();

	std::string msg("The authentication service ");
	msg += _authServiceName;
	msg += " is not available: shell logins and commands that require a permission are denied.";
	_pContext->logger().warning(msg);
	return nullptr;
}


} // namespace Poco::OSP::Shell
