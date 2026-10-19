//
// SSHShellConnection.h
//
// Library: OSP/Shell
// Package: Shell
// Module:  SSHShellConnection
//
// OSP shell session handler over SSH.
//
// Copyright (c) 2024, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//


#ifndef OSP_Shell_SSHShellConnection_INCLUDED
#define OSP_Shell_SSHShellConnection_INCLUDED


#include "Poco/SSH/SSHSession.h"
#include "Poco/SSH/SSHServerConfig.h"
#include "Poco/OSP/Shell/CommandProcessor.h"
#include <string>


namespace Poco {
namespace OSP {
namespace Shell {


class SSHShellConnection: public Poco::SSH::SSHSession
	/// OSP shell session handler over SSH.
	///
	/// Extends SSHSession to run the OSP command loop
	/// over an SSH channel after authentication.
{
public:
	SSHShellConnection(
		ssh_session session,
		const Poco::SSH::SSHServerConfig& config,
		Poco::SSH::SSHServer& server,
		CommandProcessor& commandProcessor,
		const std::string& authServiceName);

	~SSHShellConnection();

protected:
	void handleSession(ssh_channel channel, const std::string& username) override;

private:
	CommandProcessor& _commandProcessor;
	std::string _authServiceName;
};


} } } // namespace Poco::OSP::Shell


#endif // OSP_Shell_SSHShellConnection_INCLUDED
