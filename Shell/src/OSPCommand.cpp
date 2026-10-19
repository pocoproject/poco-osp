//
// OSPCommand.cpp
//
// Library: OSP/Shell
// Package: Shell
// Module:  OSPCommand
//
// Copyright (c) 2009, Applied Informatics Software Engineering GmbH.
// All rights reserved.
//
// This is unpublished proprietary source code of Applied Informatics.
// The contents of this file may not be disclosed to third parties, 
// copied or duplicated in any form, in whole or in part.
//


#include "Poco/OSP/Shell/OSPCommand.h"
#include "Poco/NumberParser.h"
#include "Poco/Glob.h"


using Poco::NumberParser;


namespace Poco {
namespace OSP {
namespace Shell {


OSPCommand::OSPCommand(Poco::OSP::BundleContext::Ptr pContext, const std::string& name):
	AbstractCommand(
		pContext->thisBundle()->properties().getString(name + ".description"),
		pContext->thisBundle()->properties().getString(name + ".usage")),
	_pContext(pContext)
{
}


OSPCommand::~OSPCommand()
{
}


Poco::OSP::Bundle::Ptr OSPCommand::findBundle(const std::string& symbolicNameOrId) const
{
	int id;
	if (NumberParser::tryParse(symbolicNameOrId, id))
	{
		return _pContext->findBundle(id);
	}
	else
	{
		return _pContext->findBundle(symbolicNameOrId);
	}
}


bool OSPCommand::match(const std::string& pattern, const std::string& subject)
{
	if (pattern.empty())
	{
		return true;
	}
	else
	{
		Poco::Glob glob(pattern);
		return glob.match(subject);
	}
}


bool OSPCommand::match(const std::vector<std::string>& patterns, const std::string& subject)
{
	if (patterns.empty())
	{
		return true;
	}
	else
	{
		for (std::vector<std::string>::const_iterator it = patterns.begin(); it != patterns.end(); ++it)
		{
			Poco::Glob glob(*it);
			if (glob.match(subject))
				return true;
		}
		return false;
	}
}


} } } // namespace Poco::OSP::Shell
