//
// SSHShellConnection.cpp
//
// Library: OSP/Shell
// Package: Shell
// Module:  SSHShellConnection
//
// Copyright (c) 2024, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//


#include "SSHShellConnection.h"
#include "Poco/SSH/SSHChannelStream.h"
#include "Poco/OSP/Shell/Command.h"
#include "Poco/OSP/Shell/Session.h"
#include "Poco/OSP/Shell/ResponseStream.h"
#include <libssh/libssh.h>


namespace Poco {
namespace OSP {
namespace Shell {


SSHShellConnection::SSHShellConnection(
	ssh_session session,
	const Poco::SSH::SSHServerConfig& config,
	Poco::SSH::SSHServer& server,
	CommandProcessor& commandProcessor,
	const std::string& authServiceName):
	Poco::SSH::SSHSession(session, config, server),
	_commandProcessor(commandProcessor),
	_authServiceName(authServiceName)
{
}


SSHShellConnection::~SSHShellConnection()
{
}


void SSHShellConnection::handleSession(ssh_channel channel, const std::string& username)
{
	Session session(_commandProcessor.context(), _authServiceName);
	session.loginWithKey(username);

	Poco::SSH::SSHChannelStream outStream(channel);
	ResponseStream responseStream(outStream);

	// Send greeting
	std::string greeting = _commandProcessor.context()->thisBundle()->properties().getString("greeting", "");
	if (!greeting.empty())
	{
		responseStream.setStatus(200 /*STATUS_OK*/);
		responseStream << greeting << std::endl;
	}

	// Send initial prompt
	outStream << "osp> " << std::flush;

	std::string commandLine;
	while (Poco::SSH::sshReadLine(channel, commandLine))
	{
		if (ssh_channel_is_eof(channel) || ssh_channel_is_closed(channel))
			break;

		// Trim whitespace
		while (!commandLine.empty() && (commandLine.back() == ' ' || commandLine.back() == '\t'))
			commandLine.pop_back();

		if (commandLine.empty())
		{
			outStream << "osp> " << std::flush;
			continue;
		}

		try
		{
			responseStream.setStatus(100 /*STATUS_INTERMEDIATE*/);
			int exitCode = _commandProcessor.execute(session, commandLine, responseStream);
			std::string response;
			int status;
			switch (exitCode)
			{
			case Command::STATUS_OK:
				response = "OK";
				status = 200;
				break;
			case Command::STATUS_USAGE:
				response = "USAGE";
				status = 401;
				break;
			case Command::STATUS_ERROR:
				response = "ERROR";
				status = 400;
				break;
			case Command::STATUS_DENIED:
				response = "DENIED";
				status = 403;
				break;
			default:
				if (exitCode < 0)
				{
					response = "GOODBYE";
					status = 201;
				}
				else
				{
					response = "ERROR";
					status = 400 + exitCode;
				}
				break;
			}
			responseStream.setStatus(status);
			responseStream << response << std::endl;

			if (exitCode < 0)
				break;
		}
		catch (Poco::Exception& exc)
		{
			responseStream.setStatus(500 /*STATUS_SERVER_ERROR*/);
			responseStream << exc.displayText() << std::endl;
		}

		outStream << "osp> " << std::flush;
	}
}


} } } // namespace Poco::OSP::Shell
