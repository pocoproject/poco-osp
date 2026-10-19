//
// CommandProcessor.cpp
//
// Library: OSP/Shell
// Package: OSPShell
// Module:  CommandProcessor
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties,
// copied or duplicated in any form, in whole or in part.
//


#include "Poco/OSP/Shell/CommandProcessor.h"
#include "Poco/OSP/Shell/Command.h"
#include "Poco/OSP/Shell/Session.h"


using Poco::FastMutex;


namespace Poco {
namespace OSP {
namespace Shell {


const std::string CommandProcessor::COMMAND_LOGIN("login");
const std::string CommandProcessor::COMMAND_HELP("help");
const std::string CommandProcessor::COMMAND_QUIT("quit");


CommandProcessor::CommandProcessor(Poco::OSP::BundleContext::Ptr pContext):
	_pContext(pContext)
{
}


CommandProcessor::~CommandProcessor()
{
}


int CommandProcessor::execute(Session& session, const std::string& commandLine, std::ostream& ostr)
{
	std::vector<std::string> items;
	parseCommandLine(commandLine, items);
	if (!items.empty())
	{
		const std::string& command = items[0];
		Poco::SharedPtr<Command> pCommand = createCommand(session, command);
		if (pCommand)
			return pCommand->execute(session, items, ostr);
		else
			return Command::STATUS_DENIED;
	}
	else return Command::STATUS_OK;
}


void CommandProcessor::addCommand(const std::string& command, const std::set<std::string>& aliases, FactoryPtr pFactory, const std::string& permission)
{
	FastMutex::ScopedLock lock(_mutex);

	CommandMap::iterator it = _commandMap.find(command);
	if (it == _commandMap.end())
	{
		CommandInfo& info = _commandMap[command];
		info.name        = command;
		info.aliases     = aliases;
		info.pFactory    = pFactory;
		info.permission  = permission;
	}
	else throw Poco::ExistsException(command);
}


void CommandProcessor::removeCommand(const std::string& command)
{
	FastMutex::ScopedLock lock(_mutex);

	CommandMap::iterator it = _commandMap.find(command);
	if (it != _commandMap.end())
		_commandMap.erase(it);
	else
		throw Poco::NotFoundException(command);
}


void CommandProcessor::removeBundle(Poco::OSP::Bundle::Ptr pBundle)
{
	FastMutex::ScopedLock lock(_mutex);

	CommandMap::iterator it = _commandMap.begin();
	while (it != _commandMap.end())
	{
		if (it->second.pFactory->context()->thisBundle() == pBundle)
		{
			CommandMap::iterator delIt(it);
			++it;
			_commandMap.erase(delIt);
		}
		else ++it;
	}
}


void CommandProcessor::listCommands(std::vector<std::string>& commands) const
{
	FastMutex::ScopedLock lock(_mutex);

	for (CommandMap::const_iterator it = _commandMap.begin(); it != _commandMap.end(); ++it)
	{
		commands.push_back(it->first);
	}
}


void CommandProcessor::aliases(const std::string& command, std::set<std::string>& aliases) const
{
	FastMutex::ScopedLock lock(_mutex);

	CommandMap::const_iterator it = _commandMap.find(command);
	if (it != _commandMap.end())
	{
		aliases = it->second.aliases;
	}
}


Command* CommandProcessor::createCommand(const Session& session, const std::string& command)
{
	FastMutex::ScopedLock lock(_mutex);

	CommandMap::iterator it = _commandMap.find(command);
	if (it == _commandMap.end())
	{
		// look for aliases
		for (it = _commandMap.begin(); it != _commandMap.end(); ++it)
		{
			if (it->second.aliases.find(command) != it->second.aliases.end())
				break;
		}
	}

	if (it != _commandMap.end())
	{
		if (it->second.permission.empty() || session.authorize(it->second.permission))
			return it->second.pFactory->createCommand();
		else
			return 0;
	}
	else throw Poco::NotFoundException(command);
}


void CommandProcessor::parseCommandLine(const std::string& commandLine, std::vector<std::string>& items)
{
	std::string::const_iterator it  = commandLine.begin();
	std::string::const_iterator end = commandLine.end();

	std::string item;
	while (it != end)
	{
		switch (*it)
		{
		case '"':
			++it;
			while (it != end && *it != '"')
			{
				if (*it == '\\')
				{
					++it;
					if (it == end) break;
				}
				item += *it++;
			}
			if (it != end) ++it;
			break;
		case '\'':
			++it;
			while (it != end && *it != '\'')
			{
				if (*it == '\\')
				{
					++it;
					if (it == end) break;
				}
				item += *it++;
			}
			if (it != end) ++it;
			break;
		case '\\':
			++it;
			if (it != end) item += *it++;
			break;
		case ' ':
		case '\t':
			if (!item.empty())
			{
				items.push_back(item);
				item.clear();
			}
			++it;
			break;
		default:
			item += *it++;
			break;
		}
	}
	if (!item.empty())
		items.push_back(item);
}


} } } // namespace Poco::OSP::Shell
