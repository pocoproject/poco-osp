//
// AbstractCommand.cpp
//
// Library: OSP/Shell
// Package: Shell
// Module:  AbstractCommand
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties, 
// copied or duplicated in any form, in whole or in part.
//


#include "Poco/OSP/Shell/AbstractCommand.h"
#include "Poco/Util/OptionProcessor.h"
#include "Poco/Util/Option.h"
#include "Poco/Util/Validator.h"
#include "Poco/Util/OptionException.h"
#include "Poco/Util/AbstractConfiguration.h"
#include "Poco/Util/HelpFormatter.h"


namespace Poco::OSP::Shell {


AbstractCommand::AbstractCommand() = default;


AbstractCommand::AbstractCommand(const std::string& description, const std::string& usage):
	_description(description),
	_usage(usage)
{
}


AbstractCommand::~AbstractCommand() = default;


int AbstractCommand::execute(Session& session, const std::vector<std::string>& args, std::ostream& ostr)
{
	std::vector<std::string> processedArgs(args);
	try
	{
		processOptions(processedArgs);
		if (_showHelp)
		{
			displayHelp(ostr);
			return Command::STATUS_OK;
		}
		else
		{
			return executeImpl(session, processedArgs, ostr);
		}
	}
	catch (Poco::Util::OptionException& exc)
	{
		ostr << exc.displayText() << '\n';
		return Command::STATUS_USAGE;		
	}
	catch (Poco::Exception& exc)
	{
		ostr << exc.displayText() << '\n';
		return Command::STATUS_ERROR;
	}
}


void AbstractCommand::defineOptions(Poco::Util::OptionSet& options)
{
	options.addOption(
		Poco::Util::Option("help", "h", "display help information on command line arguments")
			.required(false)
			.repeatable(false)
			.callback(Poco::Util::OptionCallback<AbstractCommand>(this, &AbstractCommand::handleHelp)));
}


void AbstractCommand::processOptions(std::vector<std::string>& args)
{
	defineOptions(_options);
	Poco::Util::OptionProcessor processor(_options);
	processor.setUnixStyle(true);
	_commandName = args[0];
	args.erase(args.begin());
	std::vector<std::string>::iterator it = args.begin();
	while (it != args.end() && !_stopOptionsProcessing)
	{
		std::string name;
		std::string value;
		if (processor.process(*it, name, value))
		{
			if (!name.empty()) // "--" option to end options processing
			{
				handleOption(name, value);
			}
			it = args.erase(it);
		}
		else ++it;
	}
	if (!_stopOptionsProcessing)
		processor.checkRequired();
}


void AbstractCommand::stopOptionsProcessing()
{
	_stopOptionsProcessing = true;
}


void AbstractCommand::handleOption(const std::string& name, const std::string& value)
{
	const Poco::Util::Option& option = _options.getOption(name);
	if (option.validator() != nullptr)
	{
		option.validator()->validate(option, value);
	}
	if (!option.binding().empty())
	{
		Poco::Util::AbstractConfiguration* pConfig = option.config();
		poco_check_ptr (pConfig);
		pConfig->setString(option.binding(), value);
	}
	if (option.callback() != nullptr)
	{
		option.callback()->invoke(name, value);
	}
}


void AbstractCommand::handleHelp(const std::string& /*name*/, const std::string& /*value*/)
{
	_showHelp = true;
	stopOptionsProcessing();
}


void AbstractCommand::displayHelp(std::ostream& ostr)
{
	Poco::Util::HelpFormatter helpFormatter(_options);
	helpFormatter.setCommand(_commandName);
	helpFormatter.setUsage(_usage);
	helpFormatter.setHeader(_description);
	helpFormatter.setUnixStyle(true);
	helpFormatter.setAutoIndent();
	helpFormatter.format(ostr);
}


} // namespace Poco::OSP::Shell
