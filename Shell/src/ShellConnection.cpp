//
// ShellConnection.cpp
//
// Library: OSP/Shell
// Package: Shell
// Module:  ShellConnection
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties, 
// copied or duplicated in any form, in whole or in part.
//


#include "Poco/OSP/Shell/ShellConnection.h"
#include "Poco/OSP/Shell/CommandProcessor.h"
#include "Poco/OSP/Shell/Command.h"
#include "Poco/OSP/Shell/Session.h"
#include "Poco/OSP/Shell/ResponseStream.h"
#include "Poco/OSP/BundleEvents.h"
#include "Poco/Net/DialogSocket.h"
#include "Poco/Net/SocketStream.h"
#include "Poco/Delegate.h"


using Poco::Delegate;


namespace Poco {
namespace OSP {
namespace Shell {


ShellConnection::ShellConnection(const Poco::Net::StreamSocket& s, CommandProcessor& commandProcessor, const std::string& authServiceName):
	Poco::Net::TCPServerConnection(s),
	_commandProcessor(commandProcessor),
	_authServiceName(authServiceName)
{
	_commandProcessor.context()->events().bundleStopping += Delegate<ShellConnection, BundleEvent>(this, &ShellConnection::onBundleStopping);
}


ShellConnection::~ShellConnection()
{
	try
	{
		_commandProcessor.context()->events().bundleStopping -= Delegate<ShellConnection, BundleEvent>(this, &ShellConnection::onBundleStopping);
	}
	catch (...)
	{
		poco_unexpected();
	}
}


void ShellConnection::run()
{
	_commandProcessor.context()->logger().information("New connection from " + this->socket().peerAddress().toString());
	
	try
	{
		Session session(_commandProcessor.context(), _authServiceName);
		Poco::Net::DialogSocket dialogSocket(this->socket());
		Poco::Net::SocketOutputStream ostr(this->socket());
		ResponseStream responseStream(ostr);
		
		responseStream.setStatus(STATUS_OK);
		std::string greeting(_commandProcessor.context()->thisBundle()->properties().getString("greeting", ""));
		if (!greeting.empty())
			responseStream << greeting << std::endl;
		
		std::string commandLine;
		while (dialogSocket.receiveMessage(commandLine))
		{
			try
			{
				responseStream.setStatus(STATUS_INTERMEDIATE);
				int exitCode = _commandProcessor.execute(session, commandLine, responseStream);
				std::string response;
				int status = statusFor(exitCode, response);
				responseStream.setStatus(status);
				responseStream << response << std::endl;
				if (exitCode < 0) break;
			}
			catch (Poco::Exception& exc)
			{
				responseStream.setStatus(STATUS_SERVER_ERROR);
				responseStream << exc.displayText() << std::endl;
			}
		}
	}
	catch (Poco::Exception& exc)
	{
		std::string msg("Error handling connection from ");
		msg += this->socket().peerAddress().toString();
		msg += ": ";
		msg += exc.displayText();
		_commandProcessor.context()->logger().error(msg);
	}
	_commandProcessor.context()->logger().information("Closing connection from " + this->socket().peerAddress().toString());
}


int ShellConnection::statusFor(int exitCode, std::string& response)
{
	switch (exitCode)
	{
	case Command::STATUS_OK:
		response = "OK";
		return STATUS_OK;
	case Command::STATUS_USAGE:
		response = "USAGE";
		return STATUS_CLIENT_USAGE;
	case Command::STATUS_ERROR:
		response = "ERROR";
		return STATUS_CLIENT_ERROR;
	case Command::STATUS_DENIED:
		response = "DENIED";
		return STATUS_CLIENT_DENIED;
	default:
		if (exitCode < 0)
		{
			response = "GOODBYE";
			return STATUS_GOODBYE;
		}
		else
		{
			response = "ERROR";
			return STATUS_CLIENT_ERROR + exitCode;
		}
	}
}


void ShellConnection::onBundleStopping(const void* pSender, Poco::OSP::BundleEvent& ev)
{
	if (ev.bundle() == _commandProcessor.context()->thisBundle())
	{
		this->socket().shutdown();
	}
}


} } } // namespace Poco::OSP::Shell
