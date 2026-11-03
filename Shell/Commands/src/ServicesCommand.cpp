//
// ServicesCommand.cpp
//
// Library: OSP/Shell
// Package: Shell
// Module:  ServicesCommand
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties, 
// copied or duplicated in any form, in whole or in part.
//


#include "ServicesCommand.h"
#include "Poco/OSP/ServiceRegistry.h"
#include "Poco/OSP/ServiceRef.h"
#include "Poco/Util/Option.h"
#include "Poco/Util/OptionCallback.h"


namespace Poco::OSP::Shell {


ServicesCommand::ServicesCommand(Poco::OSP::BundleContext::Ptr pContext):
	OSPCommand(pContext, "services"),
	_query("name")
{
}


ServicesCommand::~ServicesCommand() = default;


int ServicesCommand::executeImpl(Session& /*session*/, const std::vector<std::string>& args, std::ostream& ostr)
{
	std::vector<Poco::OSP::ServiceRef::Ptr> services;
	context()->registry().find(_query, services);
	for (std::vector<Poco::OSP::ServiceRef::Ptr>::iterator it = services.begin(); it != services.end(); ++it)
	{
		if (match(args, (*it)->name()))
		{
			ostr << (*it)->name() << '\n';
			if (_properties)
			{
				std::vector<std::string> keys;
				(*it)->properties().keys(keys);
				for (std::vector<std::string>::const_iterator itk = keys.begin(); itk != keys.end(); ++itk)
				{
					ostr << "\t" << *itk << " = " << (*it)->properties()[*itk] << '\n';
				}
			}
		}
	}
	
	return Command::STATUS_OK;
}


void ServicesCommand::defineOptions(Poco::Util::OptionSet& options)
{
	OSPCommand::defineOptions(options);
	
	options.addOption(
		Poco::Util::Option("query", "q", "find services using the given expression")
			.required(false)
			.repeatable(false)
			.argument("expression")
			.callback(Poco::Util::OptionCallback<ServicesCommand>(this, &ServicesCommand::handleQuery)));

	options.addOption(
		Poco::Util::Option("properties", "p", "print service properties")
			.required(false)
			.repeatable(false)
			.callback(Poco::Util::OptionCallback<ServicesCommand>(this, &ServicesCommand::handleProperties)));
}


void ServicesCommand::handleQuery(const std::string& /*name*/, const std::string& value)
{
	_query = value;
}


void ServicesCommand::handleProperties(const std::string& /*name*/, const std::string& /*value*/)
{
	_properties = true;
}


} // namespace Poco::OSP::Shell
