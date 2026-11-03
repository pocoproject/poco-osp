//
// HelpCommand.cpp
//
// Library: OSP/Shell
// Package: Shell
// Module:  HelpCommand
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties, 
// copied or duplicated in any form, in whole or in part.
//


#include "Poco/OSP/Shell/HelpCommand.h"
#include "Poco/OSP/Shell/CommandProcessor.h"
#include "Poco/String.h"


namespace Poco::OSP::Shell {


HelpCommand::HelpCommand(Poco::OSP::BundleContext::Ptr pContext, CommandProcessor& commandProcessor):
	_pContext(pContext),
	_commandProcessor(commandProcessor)
{
}


HelpCommand::~HelpCommand() = default;


int HelpCommand::execute(Session& session, const std::vector<std::string>& args, std::ostream& ostr)
{
	if (args.size() == 1)
	{
		std::string header(_pContext->thisBundle()->properties().getString("help.header", ""));
		if (!header.empty()) ostr << header << '\n';
	
		std::vector<std::string> commands;
		_commandProcessor.listCommands(session, commands);
		for (std::vector<std::string>::const_iterator it = commands.begin(); it != commands.end(); ++it)
		{
			ostr << "  " << *it;
			std::set<std::string> aliases;
			_commandProcessor.aliases(*it, aliases);
			if (!aliases.empty())
			{
				ostr << " (" << Poco::cat(std::string(", "), aliases.begin(), aliases.end()) << ")";
			}
			ostr << '\n';
		}
		ostr << '\n';
		std::string footer(_pContext->thisBundle()->properties().getString("help.footer", ""));
		if (!footer.empty()) ostr << footer << '\n';
		
		return Command::STATUS_OK;
	}
	else
	{
		std::string commandLine = args[1];
		commandLine += " --help";
		return _commandProcessor.execute(session, commandLine, ostr);
	}
}


} // namespace Poco::OSP::Shell
